# Set compiler warning flags
compiler = meson.get_compiler('c')
if compiler.get_argument_syntax() == 'msvc'
    compiler_args = compiler.get_supported_arguments([
        '/wd4100', # 'identifier' : unreferenced formal parameter
        '/wd4127', # conditional expression is constant
        '/wd4200', # nonstandard extension used : zero-sized array in struct/union
        '/wd4214', # bit field types other than int
        '/wd4706', # assignment within conditional expression
        '/wd4245', # 'conversion' : conversion from 'type1' to 'type2', signed/unsigned mismatch
        '/wd4389', # 'operator' : signed/unsigned mismatch
        '/wd4702', # unreachable code
        '/wd4701', # Potentially uninitialized local variable 'name' used
        '/wd4244', # 'conversion' conversion from 'type1' to 'type2', possible loss of data
    ])
else
    compiler_args = compiler.get_supported_arguments([
        '-Wfloat-equal',
        '-Wshadow',
        '-Wpointer-arith',
        '-Winit-self',
        '-Wno-unused-function',
        '-Wno-unused-parameter',
        '-Wno-unreachable-code',
        '-Wstrict-prototypes',
        # fix GStreamer build with -Werror
        '-Wno-missing-prototypes',
        '-Wno-incompatible-pointer-types-discards-qualifiers',
        '-Wno-address-of-packed-member',
        '-Wno-discarded-qualifiers',
        '-Wno-missing-declarations',
        '-Wno-old-style-definition',
        '-Wno-redundant-decls',
    ])
endif

# Configuration

compile_args = [compiler_args]

# Dependency: Threads
thread_dep = dependency('threads', required: true)

# Dependencies list
dependencies = [
    thread_dep,
]

# Global settings
compile_args += [
    '-D__Userspace__',
    '-DSCTP_SIMPLE_ALLOCATOR',
    '-DSCTP_PROCESS_LEVEL_LOCKS',
]

# OS-specific settings
system = host_machine.system()
if system in ['linux', 'android']
    compile_args += [
        '-D_GNU_SOURCE',
    ]
elif system == 'freebsd'
    compile_args += [compiler.get_supported_arguments([
            '-Wno-address-of-packed-member',
        ])]
elif system in ['darwin', 'ios']
    compile_args += [[
            '-D__APPLE_USE_RFC_2292',
        ] + compiler.get_supported_arguments([
            '-Wno-address-of-packed-member',
            '-Wno-deprecated-declarations',
        ])]
elif system == 'windows'
    dependencies += compiler.find_library('ws2_32', required: true)
    dependencies += compiler.find_library('iphlpapi', required: true)
    if compiler.get_id() == 'gcc'
        compile_args += [compiler.get_supported_arguments([
            '-Wno-format',
        ])]
    endif
else
    warning('Unknown system: @0@'.format(system))
    usrsctp_dep = dependency('', required: false)
    subdir_done()
endif

# Feature: sys/queue
if compiler.has_header('sys/queue.h')
    compile_args += ['-DHAVE_SYS_QUEUE_H']
endif

# Feature: sys/socket, linux/ifaddr, linux/rtnetlink
if compiler.has_header('sys/socket.h')
    if compiler.has_header('linux/if_addr.h')
        compile_args += ['-DHAVE_LINUX_IF_ADDR_H']
    endif

    if compiler.has_header('linux/rtnetlink.h')
        compile_args += ['-DHAVE_LINUX_RTNETLINK_H']
    endif
endif

# Feature: ICMP
have_sys_types = compiler.has_header('sys/types.h')
have_netinet_in = compiler.has_header('netinet/in.h')
have_netinet_ip = compiler.has_header('netinet/ip.h')
have_netinet_ip_icmp = compiler.has_header('netinet/ip_icmp.h')
if have_sys_types and have_netinet_in and have_netinet_ip and have_netinet_ip_icmp
    compile_args += ['-DHAVE_NETINET_IP_ICMP_H']
endif

# Feature: stdatomic
if compiler.has_header('stdatomic.h')
    compile_args += ['-DHAVE_STDATOMIC_H']
endif

# Feature: sockaddr.sa_len
prefix = '''
#include <sys/types.h>
#include <sys/socket.h>
'''
have_sa_len = compiler.has_member('struct sockaddr', 'sa_len', prefix: prefix)
if have_sa_len
    compile_args += ['-DHAVE_SA_LEN']
endif

# Feature: sockaddr_in.sin_len / sockaddr_in6.sin6_len / sockaddr_conn.sconn_len
prefix = '''
#include <sys/types.h>
#include <netinet/in.h>
'''
have_sin_len = compiler.has_member('struct sockaddr_in', 'sin_len', prefix: prefix)
if have_sin_len
    compile_args += ['-DHAVE_SIN_LEN']
endif
have_sin6_len = compiler.has_member('struct sockaddr_in6', 'sin6_len', prefix: prefix)
if have_sin6_len
    compile_args += ['-DHAVE_SIN6_LEN']
endif
have_sconn_len = compiler.has_member('struct sockaddr_conn', 'sconn_len', prefix: '#include "usrsctp.h"', include_directories: include_directories('usrsctplib'))
if have_sconn_len
    compile_args += ['-DHAVE_SCONN_LEN']
endif

# Options
if false
    compile_args += ['-DINVARIANTS']
endif
if not gst_debug_disabled
    compile_args += ['-DSCTP_DEBUG']
endif
# We do not need the socket API in GStreamer since we will wrap inside a
# DTLS packet anyway, because we use SCTP for WebRTC data channels.
if false
    compile_args += ['-DINET']
endif
if false
    compile_args += ['-DINET6']
endif

compile_args += ['-DSCTP_STDINT_INCLUDE=<stdint.h>']

# Library
subdir('usrsctplib')

# Build library
usrsctp_static = static_library('usrsctp-static', sources,
    c_args: compile_args,
    dependencies: dependencies,
    include_directories: include_dirs,
    override_options: ['werror=false'],
    install: false)

# Declare dependency
usrsctp_dep = declare_dependency(
    include_directories: include_dirs,
    link_with: usrsctp_static)
