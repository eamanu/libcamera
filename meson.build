project('libcamera', 'c', 'cpp',
    meson_version : '>= 0.40',
    version : '0.0.0',
    default_options : [
        'werror=true',
        'warning_level=2',
        'cpp_std=c++11',
    ],
    license : 'LGPL 2.1+')

# Generate version information. The libcamera_git_version variable contains the
# full version with git patch count and SHA1 (e.g. 1.2.3+211-c94a24f4), while
# the libcamera_version variable contains the major.minor.patch (e.g. 1.2.3)
# only. If the source tree isn't under git control, or if it matches the last
# git version tag, the build metadata (e.g. +211-c94a24f4) is omitted from
# libcamera_git_version.
libcamera_git_version = run_command('utils/gen-version.sh',
                                    meson.build_root()).stdout().strip()
if libcamera_git_version == ''
    libcamera_git_version = meson.project_version()
endif

libcamera_version = libcamera_git_version.split('+')[0]

# Configure the build environment.
cc = meson.get_compiler('c')
config_h = configuration_data()

if cc.has_header_symbol('stdlib.h', 'secure_getenv', prefix : '#define _GNU_SOURCE')
    config_h.set('HAVE_SECURE_GETENV', 1)
endif

common_arguments = [
    '-Wno-unused-parameter',
    '-include', 'config.h',
]

c_arguments = common_arguments
cpp_arguments = common_arguments

# Use libc++ by default if available instead of libstdc++ when compiling with
# clang.
if cc.get_id() == 'clang' and cc.find_library('libc++', required: false).found()
    cpp_arguments += [
        '-stdlib=libc++',
    ]
endif

add_project_arguments(c_arguments, language : 'c')
add_project_arguments(cpp_arguments, language : 'cpp')
add_project_link_arguments(cpp_arguments, language : 'cpp')

libcamera_includes = include_directories('include')

subdir('include')
subdir('src')
subdir('utils')

# The documentation and test components are optional and can be disabled
# through configuration values. They are enabled by default.

if get_option('documentation')
    subdir('Documentation')
endif

if get_option('tests')
    subdir('test')
endif

configure_file(output : 'config.h', configuration : config_h)

pkg_mod = import('pkgconfig')
pkg_mod.generate(libraries : libcamera,
                 version : '1.0',
                 name : 'libcamera',
                 filebase : 'camera',
                 description : 'Complex Camera Support Library')
