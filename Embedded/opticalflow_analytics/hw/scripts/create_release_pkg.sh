#!/bin/bash

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

########################### Usage Note ########################################
# This script should be called from the parent directory i.e.
# smart_model_select as follows.
#     ./script/create_release_pkg.sh
#
# Before envocation the MODEL_PATH should be set the  to the directory which
# has folder corresponding to each of the model as listed below in the models
# bash array.
###############################################################################


APPNAME=${PWD##*/}
PVERSION=$(grep PETALINUX_VER= ../../../vvas-platforms/Embedded/zcu104_vcuDec_DP/petalinux/.petalinux/metadata | cut -d'=' -f2)
FOLDERNAME=vvas_${APPNAME}_${PVERSION}_zcu104

models=( \
  "ssd_pedestrian_pruned_0_97"  \
  "ssd_adas_pruned_0_95"  \
  "inception_v1"  \
  "yolov3_voc_tf"  \
  "refinedet_pruned_0_96"  \
  "densebox_640_360"  \
  "resnet18"  \
  "tiny_yolov3_vmss"  \
  "yolov3_adas_pruned_0_9"  \
  "yolov2_voc_pruned_0_77"  \
  "resnet50"  \
  "mobilenet_v2"  \
  "yolov2_voc"  \
  "densebox_320_320"  \
  "ssd_traffic_pruned_0_9"  \
  "ssd_mobilenet_v2"  \
)

# Check if MODEL_DIR is set correctly
if [ ! -d ${MODEL_DIR}/${models[0]} ]; then
  echo ERROR: Please check if MODEL_DIR is set correctly !
  echo Export the MODEL_DIR before executing this script as follows
  echo
  echo "  export MODEL_DIR=<leading_path>xilinx_model_zoo_zcu102_zcu104_kv260_all-2.0.0-Linux/usr/share/vitis_ai_library/models/"
  exit 0;
fi

# Create dir
if [ ! -d $FOLDERNAME ]; then
  mkdir -p ${FOLDERNAME}/app/jsons ${FOLDERNAME}/models
fi

# Install kernel json files
cp src/jsons/kernel*.json ${FOLDERNAME}/app/jsons/

# Install setup.sh
cp src/setup.sh ${FOLDERNAME}/app/

# Install app
cp src/smart_model_select ${FOLDERNAME}/app/

# Install app templates
cp -r src/templates ${FOLDERNAME}/app/

# Install models
for model in ${models[@]}; do
  cp -r ${MODEL_DIR}/${model} ${FOLDERNAME}/models
  if [ -f src/jsons/label_${model}.json ]; then
    cp src/jsons/label_${model}.json ${FOLDERNAME}/models/${model}/label.json
  fi
done

# Install arch.json
cp $(find -name arch.json -print -quit) ${FOLDERNAME}/

# Install sdk.sh
cp `pwd`/../../../vvas-platforms/Embedded/zcu104_vcuDec_DP/platform_repo/tmp/sw_components/sdk.sh ${FOLDERNAME}/

# Install sd_card.img
cp binary_container_1/sd_card.img ${FOLDERNAME}/


# Create a Zip archive of the release package
zip -r ${FOLDERNAME}.zip ${FOLDERNAME}
