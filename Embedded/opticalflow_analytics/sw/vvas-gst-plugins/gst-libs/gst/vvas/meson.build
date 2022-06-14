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

# VVAS opticalflow metadata
ofmeta_sources = ['gstvvasofmeta.c']

gstvvasofmeta = library('gstvvasofmeta-' + api_version,
  ofmeta_sources,
  c_args : gst_plugins_vvas_args,
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  install : true,
  dependencies : [gst_dep, gstvideo_dep],
)
gstvvasofmeta_dep = declare_dependency(link_with : [gstvvasofmeta], dependencies : [gst_dep, gstvvas_dep, math_dep])

# VVAS overlay metadata
overlaymeta_sources = ['gstvvasoverlaymeta.c']

gstvvasoverlaymeta = library('gstvvasoverlaymeta-' + api_version,
  overlaymeta_sources,
  c_args : gst_plugins_vvas_args,
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  install : true,
  dependencies : [gst_dep, gstvideo_dep],
)
gstvvasoverlaymeta_dep = declare_dependency(link_with : [gstvvasoverlaymeta], dependencies : [gst_dep, gstvvas_dep, math_dep])

