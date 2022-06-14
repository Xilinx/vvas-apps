# /*
# * Copyright 2022 Xilinx Inc.
# *
# * Licensed under the Apache License, Version 2.0 (the "License");
# * you may not use this file except in compliance with the License.
# * You may obtain a copy of the License at
# *
# *    http://www.apache.org/licenses/LICENSE-2.0
# *
# * Unless required by applicable law or agreed to in writing, software
# * distributed under the License is distributed on an "AS IS" BASIS,
# * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# * See the License for the specific language governing permissions and
# * limitations under the License.
# */

project('vvas-example-video-libs', 'c', 'cpp',
  version : '1.16.2',
  meson_version : '>= 0.54.1',
  default_options : [ 'warning_level=1',
                      'buildtype=debugoptimized' ])

gst_version = meson.project_version()
version_arr = gst_version.split('.')
gst_version_major = version_arr[0].to_int()
gst_version_minor = version_arr[1].to_int()
gst_version_micro = version_arr[2].to_int()

glib_req = '>= 2.44.0'
gst_req = '>= @0@.@1@.0'.format(gst_version_major, gst_version_minor)

glib_deps = [dependency('glib-2.0', version : glib_req, fallback: ['glib', 'libglib_dep']),
             dependency('gobject-2.0', fallback: ['glib', 'libgobject_dep'])]

gst_plugins_vvas_args = ['-DHAVE_CONFIG_H']

configinc = include_directories('.')

api_version = '1.0'
soversion = 0
# maintaining compatibility with the previous libtool versioning
curversion = gst_version_minor * 100 + gst_version_micro
libversion = '@0@.@1@.0'.format(soversion, curversion)

cdata = configuration_data()
cc = meson.get_compiler('c')

cdata.set_quoted('VERSION', gst_version)
cdata.set_quoted('PACKAGE', 'gst-plugins-vvas')
cdata.set_quoted('PACKAGE_VERSION', gst_version)
cdata.set_quoted('PACKAGE_BUGREPORT', 'https://gitenterprise.xilinx.com/IPS-SSW/gst-plugins-vvas/issues')
cdata.set_quoted('PACKAGE_NAME', 'GStreamer VVAS Plug-ins')
cdata.set_quoted('GETTEXT_PACKAGE', 'gst-plugins-vvas-1.0')
cdata.set_quoted('GST_API_VERSION', api_version)
#TODO: currently proprietary until release
cdata.set_quoted('GST_LICENSE', 'proprietary')
cdata.set_quoted('LIBDIR', join_paths(get_option('prefix'), get_option('libdir')))
cdata.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))

if host_machine.cpu_family() == 'x86_64'
  message ('compiling for PCIe platform')
  cdata.set('XLNX_PCIe_PLATFORM', 1)
elif host_machine.cpu_family() == 'aarch64'
  message ('compiling for Embedded platform')
  cdata.set('XLNX_EMBEDDED_PLATFORM ', 1)
else
  error('not supporting host machine families other than x86_64 & aarch64')
endif

warning_flags = [
  '-Wmissing-declarations',
  '-Wredundant-decls',
  '-Wwrite-strings',
  '-Winit-self',
  '-Wno-multichar',
  '-Wvla',
  '-Wpointer-arith',
]

warning_c_flags = [
  '-Wmissing-prototypes',
  '-Wdeclaration-after-statement',
  '-Wold-style-definition',
  '-Waggregate-return',
]

foreach extra_arg : warning_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
endforeach

foreach extra_arg : warning_c_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
endforeach

#VVAS utility dependency
gstvvas_dep = dependency('vvas-gst-plugins', required: true)

#include directories
subdir('vvas_xpreprocessor')

configure_file(output : 'config.h', configuration : cdata)

