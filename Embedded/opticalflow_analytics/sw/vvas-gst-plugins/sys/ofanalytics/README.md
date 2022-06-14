# VVAS ofanalytics plugin

Ofanalytics plugin is application specific plugin, which uses the optical flow metadata from optical flow plugin and object position from infer plugin to estimate the object motion and direction.  This estimated information is updated as object level information of optical flow metadata. This object level metadata includes object displacement, angle, motion in x and y direction, bounding box info and direction name.

Based on the num-directions plugin property set by the user, motion is estimated as 4 or 8 directions.  To show objects which are moving in specific direction with different color from other moving object user can set direction of interest angle range by setting dir-of-interest-start and dir-of-interest-end properties, and specifying the dir-of-interest-color and other-dir-color properties (example:  dir-of-interest-start = 45, dir-of-interest-end=135 to differentiate objects moving up direction from other directions).  Example usage of this applications is anomaly detection where user can specify which direction range is considered as normal.

For displaying ofanalytics information, overlay metadata is created or updated and attached to the buffer.  This overlay metadata contains, bounding box info, color of bounding box set based on direction of interest range and arrow structure to display object motion direction.  

### Input & output:

The plugin operates in inplace transform with modifying or attaching new metadata.  This plugin required infermetadata and ofmetadata. ofmetadata is updated with object level information and gstvvasoverlaymeta is for displaying object level information on frames.

### Plugin properties

| Property Name | Description | Type | Range | Default |
| --- | --- | --- | --- | --- |
| num-directions | number of motion estimation directions | int | 4 or 8 | 4 |
| dir-of-interest-start | start angle of direction of interest range| integer | -180 to 180 | 0 |
| dir-of-interest-end | end angle of direction of interest range | integer | -180 to 180 | 0 |
| dir-of-interest-color  | color of objects in direction of interest range as 0xRGB0 | int | 0 to 4,294,967,295 | 0xff00 |
| violator-color | color of objects other than direction of interest range as 0xRGB0 | int | 0 to 4,294,967,295 | 0xff00 |

## Copyright and license statement
Copyright 2022 Xilinx Inc.

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at
[http://www.apache.org/licenses/LICENSE-2.0](http://www.apache.org/licenses/LICENSE-2.0).

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.


This directory has sources for the example designs for Embedded Platforms.
Using these sources, user can create binaries (sd_card.img) that can be flashed onto the target board.
