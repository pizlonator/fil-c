        .section .debug_info
.Lcu1_begin:
        .4byte        .Lcu1_end - .Lcu1_start
.Lcu1_start:
        .2byte        4                 /* Version */
        .4byte        .Labbrev1_begin   /* Abbrevs */
        .byte        8                  /* Pointer size */
        .uleb128        2               /* Abbrev (DW_TAG_compile_unit) */
        .uleb128        3               /* Abbrev (DW_TAG_variable) */
        .ascii        "v\0"
        .4byte        .Llabel1 - .Lcu1_begin
.Llabel1:
        .uleb128        4               /* Abbrev (DW_TAG_array_type) */
        .4byte        .Llabel2 - .Lcu1_begin
        .uleb128        5               /* Abbrev (DW_TAG_subrange_type) */
        .byte        -1
        .2byte        255
        .byte        0x0                /* Terminate children */
.Llabel2:
        .uleb128        6               /* Abbrev (DW_TAG_base_type) */
        .byte        1
        .byte        0x0                /* Terminate children */
.Lcu1_end:
        .section .note.gnu.build-id, "a", %note
        .4byte        4
        .4byte        8
        .4byte        3
        .ascii        "GNU\0"
        .byte        0x01
        .byte        0x02
        .byte        0x03
        .byte        0x04
        .byte        0x05
        .byte        0x06
        .byte        0x07
        .byte        0x08
        .section .debug_abbrev
.Labbrev1_begin:
        .uleb128        2               /* Abbrev start */
        .uleb128        0x11            /* DW_TAG_compile_unit */
        .byte        1                  /* has_children */
        .byte        0x0                /* Terminator */
        .byte        0x0                /* Terminator */
        .uleb128        3               /* Abbrev start */
        .uleb128        0x34            /* DW_TAG_variable */
        .byte        0                  /* has_children */
        .uleb128        0x03            /* DW_AT_name */
        .uleb128        0x08            /* DW_FORM_string */
        .uleb128        0x49            /* DW_AT_type */
        .uleb128        0x13            /* DW_FORM_ref4 */
        .byte        0x0                /* Terminator */
        .byte        0x0                /* Terminator */
        .uleb128        4               /* Abbrev start */
        .uleb128        0x01            /* DW_TAG_array_type */
        .byte        1                  /* has_children */
        .uleb128        0x49            /* DW_AT_type */
        .uleb128        0x13            /* DW_FORM_ref4 */
        .byte        0x0                /* Terminator */
        .byte        0x0                /* Terminator */
        .uleb128        5               /* Abbrev start */
        .uleb128        0x21            /* DW_TAG_subrange_type */
        .byte        0                  /* has_children */
        .uleb128        0x22            /* DW_AT_lower_bound */
        .uleb128        0x0b            /* DW_FORM_data1 */
        .uleb128        0x2f            /* DW_AT_upper_bound */
        .uleb128        0x05            /* DW_FORM_data2 */
        .byte        0x0                /* Terminator */
        .byte        0x0                /* Terminator */
        .uleb128        6               /* Abbrev start */
        .uleb128        0x24            /* DW_TAG_base_type */
        .byte        0                  /* has_children */
        .uleb128        0x0b            /* DW_AT_byte_size */
        .uleb128        0x0b            /* DW_FORM_data1 */
        .byte        0x0                /* Terminator */
        .byte        0x0                /* Terminator */
        .byte        0x0                /* Terminator */
        .byte        0x0                /* Terminator */
