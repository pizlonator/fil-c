
# Copyright (C) 2012 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

# marshal_XML.py: factory for interpreting XML for the purpose of
# building thread marshalling code.

import gl_XML
import sys
import copy
import typeexpr

def pot_align(base, pot_alignment):
    return (base + pot_alignment - 1) & ~(pot_alignment - 1);


class marshal_item_factory(gl_XML.gl_item_factory):
    """Factory to create objects derived from gl_item containing
    information necessary to generate thread marshalling code."""

    def create_function(self, element, context):
        return marshal_function(element, context)


class marshal_function(gl_XML.gl_function):
    # We decrease the type size when it's safe, such as when the maximum value
    # and all greater values are invalid.
    def get_marshal_type(self, param):
        type = param.type_string()

        if ('Draw' in self.name and
            ('Arrays' in self.name or
             'Elements' in self.name or
             'TransformFeedback' in self.name)):
            if (type, param.name) == ('GLenum', 'mode'):
                return 'GLenum8'

            if (type, param.name) == ('GLenum', 'type'):
                return 'GLindextype'

        if type == 'GLenum':
            return 'GLenum16' # clamped to 0xffff (always invalid enum)

        if self.is_vertex_pointer_call:
            if (type, param.name) == ('GLsizei', 'stride'):
                return 'GLclamped16i'

            if (type, param.name) == ('GLint', 'size'):
                return 'GLpacked16i'

            # glVertexAttrib*Pointer(index)
            # glVertexArrayVertexBuffer(bindingindex)
            if ((type, param.name) == ('GLuint', 'index') or
                (type, param.name) == ('GLuint', 'bindingindex')):
                return 'GLenum8' # clamped to 0xff

        return type

    def get_type_size(self, param):
        type = self.get_marshal_type(param)

        if type.find('*') != -1:
            return 8;

        mapping = {
            'GLboolean': 1,
            'GLbyte': 1,
            'GLubyte': 1,
            'GLenum8': 1, # clamped by glthread
            'GLindextype': 1,
            'GLenum16': 2, # clamped by glthread
            'GLshort': 2,
            'GLushort': 2,
            'GLhalfNV': 2,
            'GLclamped16i': 2, # clamped by glthread
            'GLpacked16i': 2, # clamped by glthread
            'GLint': 4,
            'GLuint': 4,
            'GLbitfield': 4,
            'GLsizei': 4,
            'GLfloat': 4,
            'GLclampf': 4,
            'GLfixed': 4,
            'GLclampx': 4,
            'GLhandleARB': 4,
            'int': 4,
            'float': 4,
            'GLintptr': self.context.pointer_size,
            'GLsizeiptr': self.context.pointer_size,
            'GLsync': self.context.pointer_size,
            'GLDEBUGPROC': self.context.pointer_size,
            'GLdouble': 8,
            'GLclampd': 8,
            'GLint64': 8,
            'GLuint64': 8,
            'GLuint64EXT': 8,
        }
        val = mapping.get(type, 9999)
        if val == 9999:
            print('Unhandled type in marshal_XML.get_type_size: "{0}"'.format(type), file=sys.stderr)
            assert False
        return val

    def process_element(self, element):
        # Do normal processing.
        super(marshal_function, self).process_element(element)

        # Only do further processing when we see the canonical
        # function name.
        if element.get('name') != self.name:
            return

        # Classify fixed and variable parameters.
        self.fixed_params = []
        self.variable_params = []
        for p in self.parameters:
            if p.is_padding:
                continue
            if p.is_variable_length():
                self.variable_params.append(p)
            else:
                self.fixed_params.append(p)

        # Store the "marshal" attribute, if present.
        self.marshal = element.get('marshal')
        self.marshal_sync = element.get('marshal_sync')
        self.marshal_call_before = element.get('marshal_call_before')
        self.marshal_call_after = element.get('marshal_call_after')
        self.marshal_struct = element.get('marshal_struct')
        self.marshal_no_error = gl_XML.is_attr_true(element, 'marshal_no_error')
        self.is_vertex_pointer_call = (self.name == 'InterleavedArrays' or
                                       self.name.endswith('VertexBuffer') or
                                       self.name.endswith('VertexBufferEXT') or
                                       self.name.endswith('Pointer') or
                                       self.name.endswith('PointerEXT') or
                                       self.name.endswith('PointerOES') or
                                       self.name.endswith('OffsetEXT'))

        # marshal_sync means whether a function should be called to determine
        # whether we should sync.
        if self.marshal_sync:
            # This is a case of a pointer with an unknown size. Move
            # variable-sized pointer parameters to fixed parameters because
            # they will be passed as-is if the marshal_sync function evaluates
            # to true.
            self.fixed_params = self.fixed_params + self.variable_params
            self.variable_params = []

        # Sort the parameters, so that the marshal structure fields are sorted
        # from smallest to biggest.
        self.fixed_params = sorted(self.fixed_params, key=lambda p: self.get_type_size(p))

        # Compute the marshal structure size and the largest hole
        self.struct_size = 2 # sizeof(struct marshal_cmd_base)
        largest_hole = 0

        for p in self.fixed_params:
            type_size = self.get_type_size(p)
            aligned_size = pot_align(self.struct_size, type_size)
            largest_hole = max(aligned_size - self.struct_size, largest_hole)
            self.struct_size = aligned_size
            self.struct_size = self.struct_size + type_size

        # Round down largest_hole to a power of two.
        largest_hole = int(2 ** (largest_hole.bit_length() - 1))

        # Align the structure to 8 bytes.
        aligned_size = pot_align(self.struct_size, 8)
        padding_hole = aligned_size - self.struct_size
        self.struct_size = aligned_size

        # Determine whether to generate a packed version of gl*Pointer calls.
        # If there is a hole in the cmd structure, the pointer/offset parameter
        # can be truncated and stored in the hole to save 8 bytes per call.
        # The version of the structure is determined at runtime based on
        # whether the truncation doesn't change the value. This is common with
        # VBOs because the pointer/offset is usually small.
        #
        # If there is no hole, the packed version completely removes
        # the pointer/offset parameter and is used when the value is NULL/0
        # to remove 8 bytes per call. This is common with VBOs.
        self.packed_param_name = None

        if (self.is_vertex_pointer_call and
            # 32-bit CPUs only benefit if we remove the whole 8-byte slot,
            # which means there must be exactly 4-byte padding after the 4-byte
            # pointer/offset parameter.
            (self.context.pointer_size != 4 or padding_hole == 4)):
            for pname in ['pointer', 'offset']:
                if pname in [p.name for p in self.fixed_params]:
                    self.packed_param_name = pname

            assert self.packed_param_name
            assert not self.variable_params
            assert not self.marshal_sync

        # Prepare the parameters for the packed version by replacing the type
        # of the packed variable or removing it completely.
        self.packed_fixed_params = []
        if self.packed_param_name:
            for p in self.fixed_params:
                if p.name == self.packed_param_name:
                    if largest_hole > 0:
                        # Select the truncated type.
                        type = ['GLubyte', 'GLushort', 'GLuint'][largest_hole.bit_length() - 1]

                        # Clone the parameter and change its type
                        new_param = copy.deepcopy(p)
                        new_param.type_expr = typeexpr.type_expression(type, self.context)
                        self.packed_fixed_params.append(new_param)
                else:
                    self.packed_fixed_params.append(p)
            self.packed_param_size = largest_hole
        # Sort the parameters by size to move the truncated type into the hole.
        self.packed_fixed_params = sorted(self.packed_fixed_params, key=lambda p: self.get_type_size(p))


    def get_fixed_params(self, is_packed):
        return self.packed_fixed_params if is_packed else self.fixed_params

    def marshal_flavor(self):
        """Find out how this function should be marshalled between
        client and server threads."""
        # If a "marshal" attribute was present, that overrides any
        # determination that would otherwise be made by this function.
        if self.marshal is not None:
            return self.marshal

        if self.exec_flavor == 'skip':
            # Functions marked exec="skip" are not yet implemented in
            # Mesa, so don't bother trying to marshal them.
            return 'skip'

        if self.return_type != 'void':
            return 'sync'
        for p in self.parameters:
            if p.is_output:
                return 'sync'
            if (p.is_pointer() and not
                (p.count or p.counter or p.marshal_count or p.marshal_large_count)):
                return 'sync'
            if p.count_parameter_list and not (p.marshal_count or p.marshal_large_count):
                # Parameter size is determined by enums; haven't
                # written logic to handle this yet.  TODO: fix.
                return 'sync'
        return 'async'

    def marshal_is_static(self):
        return (self.marshal_flavor() != 'custom' and
                self.name[0:8] != 'Internal' and
                self.exec_flavor != 'beginend')

    def print_struct(self, is_header=False, is_packed=False):
        if (self.marshal_struct == 'public') == is_header:
            print(self.get_marshal_struct_name(is_packed))
            print('{')
            print('   struct marshal_cmd_base cmd_base;')
            if self.variable_params:
                print('   uint16_t num_slots;')

            for p in self.get_fixed_params(is_packed):
                if p.count:
                    print('   {0} {1}[{2}];'.format(
                            p.get_base_type_string(), p.name, p.count))
                else:
                    print('   {0} {1};'.format(self.get_marshal_type(p), p.name))

            for p in self.variable_params:
                if p.img_null_flag:
                    print('   bool {0}_null; /* If set, no data follows '
                        'for "{0}" */'.format(p.name))

            for p in self.variable_params:
                if p.count_scale != 1:
                    print(('   /* Next {0} bytes are '
                         '{1} {2}[{3}][{4}] */').format(
                            p.size_string(marshal=1), p.get_base_type_string(),
                            p.name, p.counter, p.count_scale))
                else:
                    print(('   /* Next {0} bytes are '
                         '{1} {2}[{3}] */').format(
                            p.size_string(marshal=1), p.get_base_type_string(),
                            p.name, p.counter))
            print('};')
        elif self.marshal_flavor() in ('custom', 'async'):
            print('{0};'.format(self.get_marshal_struct_name(is_packed)))

        if not is_packed and self.packed_fixed_params:
            self.print_struct(is_header, True)

    def get_marshal_struct_name(self, is_packed=False):
        return 'struct marshal_cmd_{0}{1}'.format(self.name, '_packed' if is_packed else '')

    def print_unmarshal_prototype(self, is_packed=False, suffix=''):
        print(('uint32_t _mesa_unmarshal_{0}{1}(struct gl_context *ctx, '
               'const {2} *restrict cmd){3}')
               .format(self.name, '_packed' if is_packed else '',
                       self.get_marshal_struct_name(is_packed), suffix))
