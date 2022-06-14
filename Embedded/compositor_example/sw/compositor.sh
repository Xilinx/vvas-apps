#! /bin/bash

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

gst-launch-1.0 -v  filesrc location=$1 !\
h264parse !\
omxh264dec !\
comp.sink_0 \
vvas_xcompositor xclbin-location=/usr/lib/dpu.xclbin name=comp !\
video/x-raw , width=3840, height=2160 , format=NV12  !\
queue !\
fpsdisplaysink video-sink="kmssink  bus-id=a0130000.v_mix async=true" text-overlay=false sync=false \
filesrc location=$2 !\
h264parse !\
omxh264dec name=decoder_1 !\
queue !\
comp.sink_1 \
filesrc location=$3 !\
h264parse !\
omxh264dec !\
queue !\
comp.sink_2 \
filesrc location=$4 !\
h264parse !\
omxh264dec !\
queue !\
comp.sink_3
