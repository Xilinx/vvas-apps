/*
 * Copyright 2022 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "gstvvasoverlaymeta.h"
#include <stdio.h>


GType
gst_vvas_overlay_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] =
      { GST_META_TAG_VIDEO_STR, GST_META_TAG_VIDEO_SIZE_STR,
    GST_META_TAG_VIDEO_ORIENTATION_STR, NULL
  };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstVvasOverlayMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }

  return type;
}

static gboolean
gst_vvas_overlay_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstVvasOverlayMeta *vvasmeta = (GstVvasOverlayMeta *) meta;

  vvasmeta->num_rects = 0;
  vvasmeta->num_text = 0;
  vvasmeta->num_lines = 0;
  vvasmeta->num_arrows = 0;
  vvasmeta->num_circles = 0;
  vvasmeta->num_polys = 0;
  return TRUE;
}

static gboolean
gst_vvas_overlay_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  return TRUE;
}

static void
copy_color_params (VvasColorMetadata src_color, VvasColorMetadata * dst_color)
{
  dst_color->red = src_color.red;
  dst_color->green = src_color.green;
  dst_color->blue = src_color.blue;
}

static void
copy_rect_params (vvas_rect_params * src_rects, vvas_rect_params * dst_rects,
    int count)
{
  int i;

  for (i = 0; i < count; i++) {
    dst_rects[i].offset.x = src_rects[i].offset.x;
    dst_rects[i].offset.y = src_rects[i].offset.y;
    dst_rects[i].width = src_rects[i].width;
    dst_rects[i].height = src_rects[i].height;
    dst_rects[i].thickness = src_rects[i].thickness;
    dst_rects[i].apply_bg_color = src_rects[i].apply_bg_color;

    if (dst_rects[i].apply_bg_color)
      copy_color_params (src_rects[i].bg_color, &dst_rects[i].bg_color);
    else
      copy_color_params (src_rects[i].rect_color, &dst_rects[i].rect_color);
  }
}

static void
copy_font_params (vvas_font_params src_font, vvas_font_params * dst_font)
{
  dst_font->font_num = src_font.font_num;
  dst_font->font_size = src_font.font_size;

  copy_color_params (src_font.font_color, &dst_font->font_color);
}

static void
copy_text_params (vvas_text_params * src_text, vvas_text_params * dst_text,
    int count)
{
  int i;

  for (i = 0; i < count; i++) {
    dst_text[i].offset.x = src_text[i].offset.x;
    dst_text[i].offset.y = src_text[i].offset.y;

    strcpy (dst_text[i].disp_text, src_text[i].disp_text);

    if (dst_text[i].apply_bg_color)
      copy_color_params (src_text[i].bg_color, &dst_text[i].bg_color);

    copy_font_params (src_text[i].text_font, &dst_text[i].text_font);
  }
}

static void
copy_line_params (vvas_line_params * src_lines, vvas_line_params * dst_lines,
    int count)
{
  int i;

  for (i = 0; i < count; i++) {
    dst_lines[i].start_pt.x = src_lines[i].start_pt.x;
    dst_lines[i].start_pt.y = src_lines[i].start_pt.y;
    dst_lines[i].end_pt.x = src_lines[i].end_pt.x;
    dst_lines[i].end_pt.y = src_lines[i].end_pt.y;

    dst_lines[i].thickness = src_lines[i].thickness;
    copy_color_params (src_lines[i].line_color, &dst_lines[i].line_color);
  }
}

static void
copy_arrow_params (vvas_arrow_params * src_arrows,
    vvas_arrow_params * dst_arrows, int count)
{
  int i;

  for (i = 0; i < count; i++) {
    dst_arrows[i].start_pt.x = src_arrows[i].start_pt.x;
    dst_arrows[i].start_pt.y = src_arrows[i].start_pt.y;
    dst_arrows[i].end_pt.x = src_arrows[i].end_pt.x;
    dst_arrows[i].end_pt.y = src_arrows[i].end_pt.y;

    dst_arrows[i].thickness = src_arrows[i].thickness;
    dst_arrows[i].arrow_direction = src_arrows[i].arrow_direction;
    dst_arrows[i].tipLength = src_arrows[i].tipLength;
    copy_color_params (src_arrows[i].line_color, &dst_arrows[i].line_color);
  }
}

static void
copy_circle_params (vvas_circle_params * src_circles,
    vvas_circle_params * dst_circles, int count)
{
  int i;

  for (i = 0; i < count; i++) {
    dst_circles[i].center_pt.x = src_circles[i].center_pt.x;
    dst_circles[i].center_pt.y = src_circles[i].center_pt.y;
    dst_circles[i].radius = src_circles[i].radius;
    dst_circles[i].thickness = src_circles[i].thickness;

    copy_color_params (src_circles[i].circle_color,
        &dst_circles[i].circle_color);
  }
}

static void
copy_poly_params (vvas_polygon_params * src_polys,
    vvas_polygon_params * dst_polys, int count)
{
  int i, j;

  for (i = 0; i < count; i++) {
    dst_polys[i].num_pts = src_polys[i].num_pts;
    for (j = 0; j < dst_polys[i].num_pts; j++) {
      dst_polys[i].poly_pts[j].x = src_polys[i].poly_pts[j].x;
      dst_polys[i].poly_pts[j].y = src_polys[i].poly_pts[j].y;
    }

    dst_polys[i].thickness = src_polys[i].thickness;

    copy_color_params (src_polys[i].poly_color, &dst_polys[i].poly_color);
  }
}

static gboolean
gst_vvas_overlay_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstVvasOverlayMeta *dmeta, *smeta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    smeta = (GstVvasOverlayMeta *) meta;
    dmeta = gst_buffer_add_vvas_overlay_meta (dest);

    if (!dmeta) {
      GST_ERROR ("Unable to add meta to buffer");
      return FALSE;
    }

    dmeta->num_rects = smeta->num_rects;
    dmeta->num_text = smeta->num_text;
    dmeta->num_lines = smeta->num_lines;
    dmeta->num_arrows = smeta->num_arrows;
    dmeta->num_circles = smeta->num_circles;
    dmeta->num_polys = smeta->num_polys;

    if (dmeta->num_rects)
      copy_rect_params (smeta->rects, dmeta->rects, dmeta->num_rects);

    if (dmeta->num_text)
      copy_text_params (smeta->text, dmeta->text, dmeta->num_text);

    if (dmeta->num_lines)
      copy_line_params (smeta->lines, dmeta->lines, dmeta->num_lines);

    if (dmeta->num_arrows)
      copy_arrow_params (smeta->arrows, dmeta->arrows, dmeta->num_arrows);

    if (dmeta->num_circles)
      copy_circle_params (smeta->circles, dmeta->circles, dmeta->num_circles);

    if (dmeta->num_polys)
      copy_poly_params (smeta->polygons, dmeta->polygons, dmeta->num_polys);
  } else if (GST_VIDEO_META_TRANSFORM_IS_SCALE (type)) {
    GST_LOG ("Scaling of overlay metadata is not supported");
    return FALSE;
  }
  return TRUE;
}

const GstMetaInfo *
gst_vvas_overlay_meta_get_info (void)
{
  static const GstMetaInfo *vvas_overlay_meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & vvas_overlay_meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_VVAS_OVERLAY_META_API_TYPE, "GstVvasOverlayMeta",
        sizeof (GstVvasOverlayMeta),
        (GstMetaInitFunction) gst_vvas_overlay_meta_init,
        (GstMetaFreeFunction) gst_vvas_overlay_meta_free,
        gst_vvas_overlay_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & vvas_overlay_meta_info,
        (GstMetaInfo *) meta);
  }
  return vvas_overlay_meta_info;
}
