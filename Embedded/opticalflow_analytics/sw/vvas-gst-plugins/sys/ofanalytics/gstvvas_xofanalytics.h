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

#ifndef _GST_VVAS_XOFANALYTICS_H_
#define _GST_VVAS_XOFANALYTICS_H_

#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_VVAS_XOFANALYTICS   (gst_vvas_xofanalytics_get_type())
#define GST_VVAS_XOFANALYTICS(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VVAS_XOFANALYTICS,GstVvas_XOfAnalytics))
#define GST_VVAS_XOFANALYTICS_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VVAS_XOFANALYTICS,GstVvas_XOfAnalyticsClass))
#define GST_IS_VVAS_XOFANALYTICS(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VVAS_XOFANALYTICS))
#define GST_IS_VVAS_XOFANALYTICS_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VVAS_XOFANALYTICS))

typedef struct _GstVvas_XOfAnalytics GstVvas_XOfAnalytics;
typedef struct _GstVvas_XOfAnalyticsClass GstVvas_XOfAnalyticsClass;
typedef struct _GstVvas_XOfAnalyticsPrivate GstVvas_XOfAnalyticsPrivate;

struct _GstVvas_XOfAnalytics
{
  GstBaseTransform parent;
  GstVvas_XOfAnalyticsPrivate *priv;

  gint num_directions;
  gint start_angle_interest;
  gint end_angle_interest;
  guint32 dir_of_interest_color;
  guint32 other_dir_color;
};

struct _GstVvas_XOfAnalyticsClass
{
  GstBaseTransformClass parentclass;
};

GType gst_vvas_xofanalytics_get_type (void);

G_END_DECLS

#endif
