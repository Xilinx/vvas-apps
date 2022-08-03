# vvas-apps for Embedded platforms

This section covers the example designs for Embedded platforms.

There are two example designs provided in this release. These are for zcu104 Embedded platform that is based on Zynq UltraScale+ MPSoC device.

### opticalflow_analytics

The example demonstrates how to build **Optical Flow** based Gstreamer pipelines using VVAS and hardware accelerated kernel for optical flow. This example design can be modified and used in many other applications like object tracking, motion-based segmentation, depth estimation etc. Details about how to use this example is covered in [opticalflow_analytics](opticalflow_analytics/README.md)

### compositor_example

The example covers steps to create hardware accelerated composition pipelines that takes inputs from up to 16 different video sources and then compose all these input frames in to an output frame. User can define the resolutin, position of each input onto the output frame. This example explains to build four channel compositor pipeline that processes 4 streams in parallel. This tutorial explains step by step approach to build zcu104 based platform and application for running compositor pipeline. Details about how to use this example is covered in [compositor_example](compositor_example/README.md)

### vvas_appsrcsink

This example demonstrates integration of VVAS into the current user architecture, which need not be Gstreamer based. Details about how to use this example is covered in [vvas_appsrcsink](vvas_appsrcsink/README.md)

