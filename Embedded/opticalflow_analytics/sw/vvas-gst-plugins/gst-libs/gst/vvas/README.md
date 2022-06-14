# VVAS Overlay metadata

VVAS overlay metadata structure hold the information of geometric shapes and text need to be overlaid on video frames.  VVAS overlay plugin parses the overlay metadata structures to overlay information on the frames.  An intermediate plugin is required for converting metadata generated from upstream plugins like infer, segmentation or optical flow plugins to overlay metadata for displaying information on frames.  Currently supported structures in gstvvasoverlaymeta  are rectangles, text, lines, arrows, circles and polygons. Maximum number of structures of any geometric shape or text that can be draw on frames are 16 (VVAS_MAX_OVERLAY_DATA 16).  For displaying text, text need to be display must be copied into the text structure.

The GStreamer plug-ins can set and get overlay metadata from the GstBuffer by using the gst_buffer_add_meta () API and gst_buffer_get_meta () API, respectively.

~~~~~~~~~~~~~~~~
GstOverlayMeta
~~~~~~~~~~~~~~~~

GstOverlayMeta structure stores the information of different geometric structures and text.

.. code-block::

      typedef enum
      {
        AT_START,
        AT_END,
        BOTH_ENDS,
      } vvas_arrow_direction;
      
      struct _vvas_pt
      {
        int x;
        int y;
      };

      struct _vvas_font_params
      {
        int font_num;
        float font_size;
        VvasColorMetadata font_color;
      };
	
      struct _vvas_rect_params
      {
        vvas_pt offset;
        int width;
        int height;
        int thickness;
        VvasColorMetadata rect_color;
        int apply_bg_color;
        VvasColorMetadata bg_color;
      };

      struct _vvas_text_params
      {
        vvas_pt offset;
        char disp_text[VVAS_MAX_TEXT_SIZE];
        int bottom_left_origin;
        vvas_font_params text_font;
        int apply_bg_color;
        VvasColorMetadata bg_color;
      };

      struct _vvas_line_params
      {
        vvas_pt start_pt;
        vvas_pt end_pt;
        int thickness;
        VvasColorMetadata line_color;
      };

      struct _vvas_arrow_params
      {
        vvas_pt start_pt;
        vvas_pt end_pt;
        vvas_arrow_direction arrow_direction;
        int thickness;
        float tipLength;
        VvasColorMetadata line_color;
      };

      struct _vvas_circle_params
      {
        vvas_pt center_pt;
        int radius;
        int thickness;
        VvasColorMetadata circle_color;
      };

      struct _vvas_polygon_params
      {
        vvas_pt poly_pts[VVAS_MAX_OVERLAY_POLYGON_POINTS];
        int num_pts;
        int thickness;
        VvasColorMetadata poly_color;
      };

      /**
      * GstVvasOverlayMeta:
      * @num_rects: number of bounding boxes
      * @num_text: number of text boxes 
      * @num_lines: number of lines 
      * @num_arrows: number of arrows
      * @num_circles: number of circles
      * @num_polys: number of polygons
      * @vvas_rect_params: structure for holding rectangles information
      * @vvas_text_params: structure for holding text information
      * @vvas_line_params: structure for holding lines information
      * @vvas_arrow_params: structure for holding arrows information
      * @vvas_circle_params: structure for holding circles information
      * @vvas_polygon_params: structure for holding polygons information
      */
      struct _GstVvasOverlayMeta {
        GstMeta meta;
        int num_rects;
        int num_text;
        int num_lines;
        int num_arrows;
        int num_circles;
        int num_polys;
        
        vvas_rect_params rects[VVAS_MAX_OVERLAY_DATA];
        vvas_text_params text[VVAS_MAX_OVERLAY_DATA];
        vvas_line_params lines[VVAS_MAX_OVERLAY_DATA];
        vvas_arrow_params arrows[VVAS_MAX_OVERLAY_DATA];
        vvas_circle_params circles[VVAS_MAX_OVERLAY_DATA];
        vvas_polygon_params polygons[VVAS_MAX_OVERLAY_DATA];
      };


# VVAS Optical flow metadata

VVAS optical flow metadata structure hold the information of motion of frame in x and y direction and object motion information.  VVAS optical flow plugin set the optical flow meta data of frame.  This metadata structure also supports storing of motion information in object level for further analysis by downstream plugins.

The GStreamer plug-ins can set and get optical flow metadata from the GstBuffer by using the gst_buffer_add_meta () API and gst_buffer_get_meta () API, respectively.

~~~~~~~~~~~~~~~~
GstOptflowMeta
~~~~~~~~~~~~~~~~

GstOptflowMeta stores the information of optical flow of frames and object motion information.

.. code-block::

	struct _vvas_obj_motinfo
	{
	  float mean_x_displ;
	  float mean_y_displ;
	  float angle;
	  float dist;
	  char dirc_name[DIR_NAME_SZ];
	  BoundingBox bbox;
	};

	/**
        * GstVvasOverlayMeta:
        * @num_objs: number of objects with motion information
        * @obj_mot_infos: list of objects  
	* @x_displ: pointer to motion data of frame in x-direction
        * @y_displ: pointer to motion data of frame in y-directiont
        */
	struct _GstVvasOFMeta {
	  GstMeta meta;

	  guint num_objs;
	  GList *obj_mot_infos;

	  GstBuffer *x_displ;
	  GstBuffer *y_displ;
	};

## Copyright and license statement
Copyright 2022 Xilinx Inc.

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at
[http://www.apache.org/licenses/LICENSE-2.0](http://www.apache.org/licenses/LICENSE-2.0).

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.


This directory has sources for the example designs for Embedded Platforms.
Using these sources, user can create binaries (sd_card.img) that can be flashed onto the target board.
