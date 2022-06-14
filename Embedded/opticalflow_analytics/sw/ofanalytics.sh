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

gst-launch-1.0 filesrc location=$1 ! \
h264parse ! \
queue ! \
omxh264dec internal-entropy-buffers=2 ! \
queue ! \
vvas_xinfer preprocess-config=jsons\kernel_preprocessor_dev0_yolov3_voc_tf.json jsons\infer-config=kernel_yolov3_voc_tf.json name=infer ! \
vvas_optflow xclbin-location="/usr/lib/dpu.xclbin" ! \
vvas_xofanalytics start-angle=0 end-angle=90 ! \
vvas_overlay ! \
filesink location=op.nv12 -v


