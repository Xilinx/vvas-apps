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

#file copy -force ./stage1_suggestions.rqs ./binary_container_1/link/vivado/vpl/prj/prj.runs/impl_1/
#file copy -force ./prj_conf/stage1_suggestions.rqs ./binary_container_1/link/vivado/vpl/prj/prj.runs/impl_1/
set script_path [ file dirname [ file normalize [ info script ] ] ]
puts $script_path
read_qor_suggestions ${script_path}/stage3_suggestions.rqs
