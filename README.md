# vvas-apps
Copyright 2022 Xilinx Inc.

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

## vvas-apps Overview

vvas-apps has been created to host different examples and reference applications to demonstrate VVAS capabilities, how to use VVAS plug-ins, how to create different pipelines with VVAS and othere GStreamer open source plug-ins etc. By using these ready to use example designs along with pre-built binaries, one can quickly evaluate the VVAS capabilities

This is first release of vvas-apps and the examples are based on **VVAS 1.1 Release**. There are two example designs provided in this release. These are for zcu104 Embedded platform that is based on Zynq UltraScale+ MPSoC device.

### opticalflow_analytics

The example demonstrates how to build **Optical Flow** based Gstreamer pipelines using VVAS and hardware accelerated kernel for optical flow. This example design can be modified and used in many other applications like object tracking, motion-based segmentation, depth estimation etc. Details about how to use this example is covered in [opticalflow_analytics](Embedded/opticalflow_analytics/README.md)

### compositor_example

The example covers steps to create hardware accelerated composition pipelines that takes inputs from up to 16 different video sources and then compose all these input frames in to an output frame. User can define the resolutin, position of each input onto the output frame. This example explains to build four channel compositor pipeline that processes 4 streams in parallel. This tutorial explains step by step approach to build zcu104 based platform and application for running compositor pipeline. Details about how to use this example is covered in [compositor_example](Embedded/compositor_example/README.md)

### vvas_appsrcsink

This example demonstrates integration of VVAS into the current user architecture, which need not be Gstreamer based.

