#!/bin/bash

if [ -z "$CC" ]; then
  echo "Cross compilation not set - source environment setup first"
  exit
fi

rm -rf install

cd vvas-gst-plugins
sed -E 's@<SYSROOT>@'"$SDKTARGETSYSROOT"'@g; s@<NATIVESYSROOT>@'"$OECORE_NATIVE_SYSROOT"'@g' meson.cross.template > meson.cross
cp meson.cross $OECORE_NATIVE_SYSROOT/usr/share/meson/aarch64-xilinx-linux-meson.cross
meson --prefix /usr build --cross-file meson.cross

cd build
ninja

DESTDIR=../../install ninja install

cd ../../vvas-accel-sw-libs
sed -E 's@<SYSROOT>@'"$SDKTARGETSYSROOT"'@g; s@<NATIVESYSROOT>@'"$OECORE_NATIVE_SYSROOT"'@g' meson.cross.template > meson.cross
cp meson.cross $OECORE_NATIVE_SYSROOT/usr/share/meson/aarch64-xilinx-linux-meson.cross
meson build --cross-file meson.cross --prefix /usr
    
cd build
ninja

DESTDIR=../../install ninja install

cd ../../install
tar -pczvf vvas_ofexample_installer.tar.gz usr
cd ..

########################################################################
 # Copyright 2022 Xilinx, Inc.
 #
 # Licensed under the Apache License, Version 2.0 (the "License");
 # you may not use this file except in compliance with the License.
 # You may obtain a copy of the License at
 #
 #     http://www.apache.org/licenses/LICENSE-2.0
 #
 # Unless required by applicable law or agreed to in writing, software
 # distributed under the License is distributed on an "AS IS" BASIS,
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and
 # limitations under the License.
#########################################################################
