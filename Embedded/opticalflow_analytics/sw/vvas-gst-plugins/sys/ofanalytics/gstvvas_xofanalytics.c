/*
* Copyright (C) 2020 - 2022 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the Software
* is furnished to do so, subject to the following conditions:
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
* KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
* EVENT SHALL XILINX BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
* OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE. Except as contained in this notice, the name of the Xilinx shall
* not be used in advertising or otherwise to promote the sale, use or other
* dealings in this Software without prior written authorization from Xilinx.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/vvas/gstinferencemeta.h>
#include <gst/vvas/gstinferenceprediction.h>
#include <gst/vvas/gstvvasofmeta.h>
#include <gst/vvas/gstvvasoverlaymeta.h>
#include "gstvvas_xofanalytics.h"

#define MIN_DISPLACE 0.5

GST_DEBUG_CATEGORY_STATIC (gst_vvas_xofanalytics_debug_category);
#define GST_CAT_DEFAULT gst_vvas_xofanalytics_debug_category

#define gst_vvas_xofanalytics_parent_class parent_class

static GstFlowReturn gst_vvas_xofanalytics_transform_ip (GstBaseTransform *
    trans, GstBuffer * buf);
static void gst_vvas_xofanalytics_finalize (GObject * gobject);

enum
{
  PROP_0,
  PROP_NUM_DIRECTIONS,
  PROP_DIR_OF_INTEREST_START,
  PROP_DIR_OF_INTEREST_END,
  PROP_DIR_OF_INTEREST_COLOR,
  PROP_OTHER_DIR_COLOR,
};

struct _GstVvas_XOfAnalyticsPrivate
{
  gint width;
  gint height;
  gint stride;
  GstVvasOFMeta *of_meta;
  GstVideoInfo *in_vinfo;
  GstVvasOverlayMeta *overlay_meta;
  float *x_data, *y_data;
  gint aoi_color_r;
  gint aoi_color_g;
  gint aoi_color_b;
  gint other_color_r;
  gint other_color_g;
  gint other_color_b;
};

struct vvas_dircbin
{
  gfloat st_angle;
  gfloat ed_angle;
  const gchar *dirc_name;
};

static struct vvas_dircbin dirc_bins_4[4] = {
  {-45.0, 45.0, "right"},
  {45.0, 135.0, "top"},
  {135.0, -135.0, "left"},
  {-135.0, -45.0, "bottom"}
};

static struct vvas_dircbin dirc_bins_8[8] = {
  {-22.5, 22.5, "right"},
  {22.5, 67.5, "top-right"},
  {67.5, 112.5, "top"},
  {112.5, 157.5, "top-left"},
  {157.5, -157.5, "left"},
  {-157.5, -112.5, "bottom-left"},
  {-112.5, -67.5, "bottom"},
  {-67.5, -22.5, "bottom-right"},
};

G_DEFINE_TYPE_WITH_PRIVATE (GstVvas_XOfAnalytics, gst_vvas_xofanalytics,
    GST_TYPE_BASE_TRANSFORM);

#define GST_VVAS_XOFANALYTICS_PRIVATE(self) (GstVvas_XOfAnalyticsPrivate *) (gst_vvas_xofanalytics_get_instance_private (self))
#define GST_TYPE_VVAS_XOFANALYTICS_TYPE (gst_vvas_xofanalytics_type ())
#define OF_PI 3.14159265

static void
gst_vvas_xofanalytics_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstVvas_XOfAnalytics *self = GST_VVAS_XOFANALYTICS (object);
  guint32 val;

  switch (prop_id) {
    case PROP_NUM_DIRECTIONS:
      self->num_directions = g_value_get_int (value);
      if (self->num_directions < 8)
        self->num_directions = 4;
      else
        self->num_directions = 8;
      break;
    case PROP_DIR_OF_INTEREST_START:
      self->start_angle_interest = g_value_get_int (value);
      break;
    case PROP_DIR_OF_INTEREST_END:
      self->end_angle_interest = g_value_get_int (value);
      break;
    case PROP_DIR_OF_INTEREST_COLOR:
      self->dir_of_interest_color = g_value_get_uint (value);
      val = self->dir_of_interest_color;
      val = val >> 8;
      self->priv->aoi_color_b = val & 0xff;
      val = val >> 8;
      self->priv->aoi_color_g = val & 0xff;
      val = val >> 8;
      self->priv->aoi_color_r = val & 0xff;
      break;
    case PROP_OTHER_DIR_COLOR:
      self->other_dir_color = g_value_get_uint (value);
      val = self->other_dir_color;
      val = val >> 8;
      self->priv->other_color_b = val & 0xff;
      val = val >> 8;
      self->priv->other_color_g = val & 0xff;
      val = val >> 8;
      self->priv->other_color_r = val & 0xff;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_vvas_xofanalytics_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstVvas_XOfAnalytics *self = GST_VVAS_XOFANALYTICS (object);

  switch (prop_id) {
    case PROP_NUM_DIRECTIONS:
      g_value_set_int (value, self->num_directions);
      break;
    case PROP_DIR_OF_INTEREST_START:
      g_value_set_int (value, self->start_angle_interest);
      break;
    case PROP_DIR_OF_INTEREST_END:
      g_value_set_int (value, self->end_angle_interest);
      break;
    case PROP_DIR_OF_INTEREST_COLOR:
      g_value_set_uint (value, self->dir_of_interest_color);
      break;
    case PROP_OTHER_DIR_COLOR:
      g_value_set_uint (value, self->other_dir_color);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_vvas_xofanalytics_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstVvas_XOfAnalytics *self = GST_VVAS_XOFANALYTICS (trans);
  gboolean bret = TRUE;
  GstVvas_XOfAnalyticsPrivate *priv = self->priv;

  GST_INFO_OBJECT (self,
      "incaps = %" GST_PTR_FORMAT "and outcaps = %" GST_PTR_FORMAT, incaps,
      outcaps);

  if (!gst_video_info_from_caps (priv->in_vinfo, incaps)) {
    GST_ERROR_OBJECT (self, "Failed to parse input caps");
    return FALSE;
  }

  GST_DEBUG_OBJECT (self,
      "input and output caps: %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT,
      incaps, outcaps);

  return bret;
}

static gboolean
gst_vvas_xofanalytics_stop (GstBaseTransform * trans)
{
  GstVvas_XOfAnalytics *self = GST_VVAS_XOFANALYTICS (trans);
  GST_DEBUG_OBJECT (self, "stopping");

  if (self->priv->in_vinfo) {
    gst_video_info_free (self->priv->in_vinfo);
    self->priv->in_vinfo = NULL;
  }

  return TRUE;
}

static void
gst_vvas_xofanalytics_class_init (GstVvas_XOfAnalyticsClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *transform_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_vvas_xofanalytics_set_property;
  gobject_class->get_property = gst_vvas_xofanalytics_get_property;
  gobject_class->finalize = gst_vvas_xofanalytics_finalize;

  transform_class->set_caps = gst_vvas_xofanalytics_set_caps;
  transform_class->stop = gst_vvas_xofanalytics_stop;

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL))));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL))));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Optical flow metadata analyser",
      "Video/Filter", "Optical Flow Metadata analyser", "Xilinx Inc");

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_NUM_DIRECTIONS,
      g_param_spec_int ("num-directions", "Directions",
          "Number of directions", 4, 8, 4,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_DIR_OF_INTEREST_START, g_param_spec_int ("dir-of-interest-start",
          "direction of interest start angle",
          "Starting angle of object direction of interest", -180, 180, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_DIR_OF_INTEREST_END, g_param_spec_int ("dir-of-interest-end",
          "direction of interest end angle",
          "ending angle of object direction of interest", -180, 180, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_DIR_OF_INTEREST_COLOR, g_param_spec_uint ("dir-of-interest-color",
          "color of direction of interest",
          "color to be used for representing direction of interest", 0, 0xffffffff, 0xffffffff,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_OTHER_DIR_COLOR,
      g_param_spec_uint ("other-dir-color", "other direction objects color",
          "color to be used for representing objects other than interest direction",
          0, 0xffffffff, 0x0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (gst_vvas_xofanalytics_debug_category,
      "vvas_xofanalytics", 0, "debug category for VVAS ofanalytics element");

  transform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_vvas_xofanalytics_transform_ip);
}

static void
gst_vvas_xofanalytics_init (GstVvas_XOfAnalytics * self)
{
  self->priv = GST_VVAS_XOFANALYTICS_PRIVATE (self);
  self->num_directions = 4;
  self->start_angle_interest = 0;
  self->end_angle_interest = 0;
  self->dir_of_interest_color = 0xffffffff;
  self->other_dir_color = 0x0;
  self->priv->aoi_color_r = 255;
  self->priv->aoi_color_g = 255;
  self->priv->aoi_color_b = 255;
  self->priv->other_color_r = 0;
  self->priv->other_color_g = 0;
  self->priv->other_color_b = 0;
  self->priv->in_vinfo = gst_video_info_new ();
  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (self), TRUE);
}

static void
gst_vvas_xofanalytics_finalize (GObject * gobject)
{
  GstVvas_XOfAnalytics *self = GST_VVAS_XOFANALYTICS (gobject);
  GST_LOG_OBJECT (self, "finalize");
  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
find_direction (float angle, int num_dir, char *dirc_name)
{
  int r_idx, dir = -1;

  switch (num_dir) {
    case 4:
      dir = 2;
      for (r_idx = 0; r_idx < num_dir; r_idx++) {
        if (angle > dirc_bins_4[r_idx].st_angle
            && angle <= dirc_bins_4[r_idx].ed_angle) {
          dir = r_idx;
          break;
        }
      }
      snprintf (dirc_name, DIR_NAME_SZ, "%s", dirc_bins_4[dir].dirc_name);
      break;
    case 8:
      dir = 4;
      for (r_idx = 0; r_idx < num_dir; r_idx++) {
        if (angle > dirc_bins_8[r_idx].st_angle
            && angle <= dirc_bins_8[r_idx].ed_angle) {
          dir = r_idx;
          break;
        }
      }
      snprintf (dirc_name, DIR_NAME_SZ, "%s", dirc_bins_8[dir].dirc_name);
      break;
  }
}

static void
get_arrow_points (vvas_pt origin, int *x, int *y, int len, int angle)
{
  *x = origin.x + (len * cos (angle * OF_PI / 180));
  *y = origin.y + (len * sin (angle * OF_PI / 180));

  return;
}

static gboolean
prepare_obj_motion_info (GNode * node, gpointer ptr)
{
  GstVvas_XOfAnalytics *self = GST_VVAS_XOFANALYTICS (ptr);
  GstVvas_XOfAnalyticsPrivate *priv = self->priv;
  GstInferencePrediction *prediction = (GstInferencePrediction *) node->data;
  int x, y, idx, len;
  unsigned int width, height, r_idx, c_idx;
  unsigned int  count;
  float *x_pos, *y_pos;
  float x_mean, y_mean;
  float displ, angle = 0;
  vvas_obj_motinfo *obj_motinfo;
  vvas_pt origin;
  int arrow_x, arrow_y;
  
  x_mean = y_mean = 0;
  count = 0;

  if (!node->parent)
    return FALSE;

  /* prediction node info */
  x = prediction->bbox.x;
  y = prediction->bbox.y;
  width = prediction->bbox.width;
  height = prediction->bbox.height;

  x_pos = priv->x_data + y * priv->stride + x;
  y_pos = priv->y_data + y * priv->stride + x;

  for (r_idx = 0; r_idx < height; r_idx++) {
    for (c_idx = 0; c_idx < width; c_idx++) {
      if (fabs(x_pos[c_idx]) >= 0.5 || fabs(y_pos[c_idx]) >= 0.5) {
      //if (x_pos[c_idx] != 0 || y_pos[c_idx] != 0) {
        x_mean += x_pos[c_idx];
        y_mean += y_pos[c_idx];
        count++;
      }
    }
    /* check if of meta is calculated with stride included */
    x_pos += priv->stride;
    y_pos += priv->stride;
  }

  displ = 0;
  if (count) {
    x_mean /= count;
    y_mean /= count;
    displ = sqrt (x_mean * x_mean + y_mean * y_mean);
  }

  obj_motinfo = (vvas_obj_motinfo *) calloc (1, sizeof (vvas_obj_motinfo));
  obj_motinfo->mean_x_displ = x_mean;
  obj_motinfo->mean_y_displ = -y_mean;
  obj_motinfo->dist = displ;

  obj_motinfo->bbox.x = x;
  obj_motinfo->bbox.y = y;
  obj_motinfo->bbox.width = width;
  obj_motinfo->bbox.height = height;

  if (displ > MIN_DISPLACE) {
    angle = (atan2 (-y_mean, x_mean) * 180) / OF_PI;
    obj_motinfo->angle = angle;
    find_direction (angle, self->num_directions, obj_motinfo->dirc_name);
    if (angle >= self->start_angle_interest && angle <= self->end_angle_interest) {
      /* angle in between user provided angles */
      obj_motinfo->bbox.box_color.red = self->priv->aoi_color_r;
      obj_motinfo->bbox.box_color.green = self->priv->aoi_color_g;
      obj_motinfo->bbox.box_color.blue = self->priv->aoi_color_b;
    } else {
      obj_motinfo->bbox.box_color.red = self->priv->other_color_r;
      obj_motinfo->bbox.box_color.green = self->priv->other_color_g;
      obj_motinfo->bbox.box_color.blue = self->priv->other_color_b;
    }
  } else {
    /* no displacement */
    obj_motinfo->angle = 0;
    snprintf (obj_motinfo->dirc_name, DIR_NAME_SZ, "%s", "no-motion");
    obj_motinfo->bbox.box_color.red = self->priv->other_color_r;
    obj_motinfo->bbox.box_color.green = self->priv->other_color_g;
    obj_motinfo->bbox.box_color.blue = self->priv->other_color_b;
  }

  priv->of_meta->obj_mot_infos =
      g_list_append (priv->of_meta->obj_mot_infos, obj_motinfo);
  priv->of_meta->num_objs++;

  /* draw rectangle */
  idx = priv->overlay_meta->num_rects;
  priv->overlay_meta->rects[idx].offset.x = obj_motinfo->bbox.x;
  priv->overlay_meta->rects[idx].offset.y = obj_motinfo->bbox.y;
  priv->overlay_meta->rects[idx].width = obj_motinfo->bbox.width;
  priv->overlay_meta->rects[idx].height = obj_motinfo->bbox.height;
  priv->overlay_meta->rects[idx].thickness = 2; //TODO
  priv->overlay_meta->rects[idx].rect_color.red =
      obj_motinfo->bbox.box_color.red;
  priv->overlay_meta->rects[idx].rect_color.green =
      obj_motinfo->bbox.box_color.green;
  priv->overlay_meta->rects[idx].rect_color.blue =
      obj_motinfo->bbox.box_color.blue;
  priv->overlay_meta->rects[idx].apply_bg_color = 0;
  priv->overlay_meta->num_rects++;

  /* draw arrow */
  if (displ > MIN_DISPLACE) {
    origin.x = x + width / 2;
    origin.y = y + height / 2;
    len = 0.15 * (width > height ? width : height);
    idx = priv->overlay_meta->num_arrows;
    /* image origin is top left */
    get_arrow_points (origin, &arrow_x, &arrow_y, len, 360 - angle);
    priv->overlay_meta->arrows[idx].start_pt.x = arrow_x;
    priv->overlay_meta->arrows[idx].start_pt.y = arrow_y;
    get_arrow_points (origin, &arrow_x, &arrow_y, len, 360 - angle + 180);
    priv->overlay_meta->arrows[idx].end_pt.x = arrow_x;
    priv->overlay_meta->arrows[idx].end_pt.y = arrow_y;
    priv->overlay_meta->arrows[idx].arrow_direction = 0;
    priv->overlay_meta->arrows[idx].thickness = 2;
    priv->overlay_meta->arrows[idx].tipLength = 0.25;
    priv->overlay_meta->arrows[idx].line_color.red = 0;
    priv->overlay_meta->arrows[idx].line_color.green = 0;
    priv->overlay_meta->arrows[idx].line_color.blue = 255;
    priv->overlay_meta->arrows[idx].line_color.alpha =
        obj_motinfo->bbox.box_color.alpha;
    priv->overlay_meta->num_arrows++;
  }

  return FALSE;
}

static GstFlowReturn
gst_vvas_xofanalytics_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstVvas_XOfAnalytics *self = GST_VVAS_XOFANALYTICS (trans);
  GstInferenceMeta *infer_meta = NULL;
  GstVvasOFMeta *of_meta = NULL;
  GstVvasOverlayMeta *overlay_meta = NULL;
  GstVideoMeta *vmeta = NULL;
  GstMapInfo x_info, y_info;
  GstVvas_XOfAnalyticsPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "received buffer %" GST_PTR_FORMAT, buf);
  of_meta =
      (GstVvasOFMeta *) gst_buffer_get_meta (buf,
      gst_vvas_of_meta_api_get_type ());
  if (!of_meta) {
    GST_ERROR_OBJECT (self, "optical flow metadata not present");
    return GST_FLOW_ERROR;
  }

  infer_meta =
      (GstInferenceMeta *) gst_buffer_get_meta (buf,
      gst_inference_meta_api_get_type ());
  if (infer_meta == NULL) {
    GST_DEBUG_OBJECT (self, "Infer metadata not present. Hence ignoring the frame.");

    return GST_FLOW_OK;
  }

  overlay_meta =
      (GstVvasOverlayMeta *) gst_buffer_get_meta (buf,
      gst_vvas_overlay_meta_api_get_type ());
  if (!overlay_meta) {
    overlay_meta =
        (GstVvasOverlayMeta *) gst_buffer_add_meta (buf,
        gst_vvas_overlay_meta_get_info (), NULL);
    GST_DEBUG_OBJECT (self, "adding overlay meta %p", overlay_meta);
  }
  priv->of_meta = of_meta;
  priv->overlay_meta = overlay_meta;

  gst_buffer_map (of_meta->x_displ, &x_info, GST_MAP_READ);
  priv->x_data = (gfloat *) x_info.data;
  gst_buffer_map (of_meta->y_displ, &y_info, GST_MAP_READ);
  priv->y_data = (gfloat *) y_info.data;

  vmeta = gst_buffer_get_video_meta (buf);
  if (vmeta) {
    priv->width = vmeta->width;
    priv->height = vmeta->height;
    priv->stride = vmeta->stride[0];
  } else {
    priv->width = priv->in_vinfo->width;
    priv->height = priv->in_vinfo->height;
    priv->stride = priv->in_vinfo->stride[0];
    GST_DEBUG_OBJECT (self, "video meta not present in buffer");
  }

  g_node_traverse (infer_meta->prediction->predictions, G_PRE_ORDER,
      G_TRAVERSE_ALL, -1, prepare_obj_motion_info, self);

  gst_buffer_unmap (of_meta->x_displ, &x_info);
  gst_buffer_unmap (of_meta->y_displ, &y_info);

  return GST_FLOW_OK;
}

static gboolean
vvas_xofanalytics_init (GstPlugin * vvas_xofanalytics)
{
  return gst_element_register (vvas_xofanalytics, "vvas_xofanalytics",
      GST_RANK_NONE, GST_TYPE_VVAS_XOFANALYTICS);
}

#ifndef PACKAGE
#define PACKAGE "vvas_xofanalytics"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    vvas_xofanalytics,
    "Xilinx VVAS SDK plugin to analyse optical flow metadata",
    vvas_xofanalytics_init, "1.0", "MIT/X11",
    "Xilinx VVAS SDK plugin", "http://xilinx.com/")
