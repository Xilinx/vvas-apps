/*
 * Copyright (C) 2022, Xilinx Inc - All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */


#include <gst/gst.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <gst/app/gstappsrc.h>
#include <gst/vvas/gstinferencemeta.h>
#include <gst/vvas/gstvvasusrmeta.h>
#include <gst/allocators/gstdmabuf.h>
#include <mqueue.h>

#include "vvas_appsrcsink.h"
#include "vvas_appsrcsink_gst_priv.h"

/* Convert VideoFormat to GstVideoFormat */
static GstVideoFormat
vvas_xap1_get_gst_fmt (VideoFormat fmt)
{
  switch (fmt) {
    case APP1_VIDEO_FORMAT_UNKNOWN:
      return GST_VIDEO_FORMAT_UNKNOWN;
    case APP1_VIDEO_FORMAT_NV12:
      return GST_VIDEO_FORMAT_NV12;
    case APP1_VIDEO_FORMAT_RGB:
      return GST_VIDEO_FORMAT_RGB;
    case APP1_VIDEO_FORMAT_ENCODED:
      return GST_VIDEO_FORMAT_ENCODED;
  }

  return GST_VIDEO_FORMAT_UNKNOWN;
}

/* Findout how much to read from file in each read call.
 * In case of RAW video file input, we read the size of one frame.
 * In case of encoded file we read 10k in each read. */
unsigned int
vvas_appsrcsink_get_buffer_size (VideoAlignment *video_align)
{
  GstVideoFormat fmt = vvas_xap1_get_gst_fmt (video_align->format);
  GstVideoInfo vinfo;
  unsigned int size = 0, idx = 0;

  if (fmt == GST_VIDEO_FORMAT_ENCODED) {
    /* If the file is encoded like h264, lets read 10K in one read */
    size = 10240;
  } else {
    gst_video_info_set_format (&vinfo, fmt,
                               video_align->stride[0], video_align->height);
    size = vinfo.size;
  
    /* Lets consider the offset calculated from GstVideoInfo itself */
    for (idx = 0; idx < vinfo.finfo->n_planes; idx++) {
      video_align->offset[idx] = vinfo.offset[idx];
    }
  }

  return size;
}

/* Notification callback when the associated GstBuffer is freed */
static void vvas_appsrcsink_buffer_freed_notification (gpointer data)
{
  UsrBuff *usrbuf = (UsrBuff *)data;
  vvas_appsrcsink_free_queue_release (usrbuf->usr_data->free_queue, data);
  PRINT(APP1_LEVEL_INFO, "Released buffer at index : %ld\n", usrbuf->buf_idx); 
}

/* Notification callback when the associated GstBuffer is freed */
static void vvas_appsrcsink_dma_buffer_freed_notification (gpointer data, GstMiniObject *obj)
{
  UsrBuff *usrbuf = (UsrBuff *)data;
  vvas_appsrcsink_free_queue_release (usrbuf->usr_data->free_queue, data);
  PRINT(APP1_LEVEL_INFO, "Released buffer at index : %ld\n", usrbuf->buf_idx); 
}

/* This function will be called whener "appsrc" asks for more input
 * buffers. This is function where user can do changes to adapt 
 * thier existing architecture as explained in comments below */
GstBuffer *
vvas_appsrcsink_get_buffer_from_usr (App1Priv * priv)
{
  GstBuffer *new_buf;
  GstMemory *mem;
  int size, dma_prime_fd;
  void *inpriv;
  void *usrmeta;
  VideoAlignment *videoalign;
  UsrBuff *usrbuf;

  PRINT (APP1_LEVEL_DEBUG, "Reading from input message queue");

  /* Pop out the input "UsrBuff", enqueued at producer thread */
  usrbuf = g_async_queue_pop (priv->usrpriv->input_queue);

  PRINT
      (APP1_LEVEL_INFO, "sending data to gst :  %p data = %p size = %d usrmeta=%p",
           usrbuf, usrbuf->data, usrbuf->size, usrbuf->usr_meta);
  inpriv = usrbuf->data;
  dma_prime_fd = usrbuf->dma_prime_fd;
  usrmeta = (void *) usrbuf->usr_meta;
  size = usrbuf->size;
  videoalign = &(usrbuf->video_align);

  PRINT (APP1_LEVEL_DEBUG, "Got buffer (%p) of size = %d and usrmeta = %p", inpriv, size,
      usrmeta);
  if (size) {
    /* Check if the buffer is DMA buffer or "malloc" buffer. */
    if (dma_prime_fd > 0) {
      if (!priv->dmabuf_alloc)
        priv->dmabuf_alloc = gst_dmabuf_allocator_new ();

      new_buf = gst_buffer_new ();
      mem = gst_dmabuf_allocator_alloc (priv->dmabuf_alloc, dma_prime_fd, size);
      gst_buffer_insert_memory (new_buf, -1, mem);
      /* Adding a notification callback to know when the buffer is freed */
      gst_mini_object_weak_ref (GST_MINI_OBJECT (new_buf), vvas_appsrcsink_dma_buffer_freed_notification, usrbuf);
    } else {
      /* Adding a notification callback to know when the buffer is freed */
      new_buf = gst_buffer_new_wrapped_full ((GstMemoryFlags) 0, inpriv,
          size, 0, size, usrbuf, vvas_appsrcsink_buffer_freed_notification);
    }
    /* Here we are checking if user has a meta data to be attached with input buffer
     * and adding it.
     * This is just an example for user to show that meta data can be added, 
     * user has to be aware of his Gstreamer pipeline and what meta data 
     * that the plugins being used expect and them accordingly */
    if (usrmeta) {
      gst_buffer_add_vvas_usr_meta ((GstBuffer *) new_buf,
          priv->usrpriv->copy_usr_meta, priv->usrpriv->free_usr_meta, usrmeta);
    }
	/* Adding video meta data to input GstBuffer, these paramers are taken
     * from input JSON configuration file */
    if ((*videoalign).n_plane) {
      PRINT
          (APP1_LEVEL_INFO, "Videometa : n_plane = %d, WXH = %dX%d, stride0/stride1 = %d/%d, offset0/offset1 = %ld/%ld",
          (*videoalign).n_plane, (*videoalign).width, (*videoalign).height,
          (*videoalign).stride[0], (*videoalign).stride[1],
          (*videoalign).offset[0], (*videoalign).offset[1]);

      gst_buffer_add_video_meta_full ((GstBuffer *) new_buf,
          GST_VIDEO_FRAME_FLAG_NONE,
          vvas_xap1_get_gst_fmt ((*videoalign).format),
          (*videoalign).width, (*videoalign).height, (*videoalign).n_plane,
          (*videoalign).offset, (*videoalign).stride);
    }
    /* Return the newly created GstBuffer to be pushed to "appsrc" */
    return new_buf;
  } else {
    /* Returning NULL when the size is zero, inidicating for EOS */
    return NULL;
  }
}

/* Iterate through GstInferenceMeta coming out of ML pipelne */
static gboolean
vvas_appsrcsink_node_foreach (GNode * node, gpointer priv_ptr)
{
  App1Priv *priv = (App1Priv *)priv_ptr;
  UsrInferenceData *infer_result = priv->infer_result;
  GList *classes;
  GstInferencePrediction *prediction = (GstInferencePrediction *) node->data;
  GstInferenceClassification *classification = NULL;

  for (classes = prediction->classifications;
         classes; classes = g_list_next (classes)) {
    classification = (GstInferenceClassification *) classes->data;
    infer_result->classes[infer_result->num_classes].classification_id =
           classification->classification_id;
    infer_result->classes[infer_result->num_classes].class_id =
           classification->class_id;
    infer_result->classes[infer_result->num_classes].class_prob =
           classification->class_prob;
    strcpy (infer_result->classes[infer_result->num_classes].class_label,
             classification->class_label);
    infer_result->num_classes++;

    infer_result->bbox[infer_result->num_bbox].x = prediction->bbox.x;
    infer_result->bbox[infer_result->num_bbox].y = prediction->bbox.y;
    infer_result->bbox[infer_result->num_bbox].width = prediction->bbox.width;
    infer_result->bbox[infer_result->num_bbox].height = prediction->bbox.height;
    infer_result->num_bbox++;
  }

  return FALSE;
}

/*
 * This function will be called when Gstreamer's output is ready and notified from
 * "appsink".
 */
void
vvas_appsrcsink_send_data_to_user (App1Priv * priv, GstSample *sample)
{
  GstMapInfo *map = malloc(sizeof(GstMapInfo));
  GstInferenceMeta *infer_meta = NULL;
  void *usrmeta;
  UsrBuff *usrbuf = (UsrBuff *) calloc (1, sizeof (UsrBuff));
  GstBuffer *buffer = NULL;

  usrbuf->buffer_ptr = gst_sample_get_buffer (sample);
  
  buffer = usrbuf->buffer_ptr;  
  PRINT (APP1_LEVEL_DEBUG, "Got output from GST pipe");

  /* In case of Machine Learning pipeline, the output buffer will have inference results
   * attached as meta data of type "GstInferenceMeta" */
  infer_meta =
      ((GstInferenceMeta *) gst_buffer_get_meta ((GstBuffer *) buffer,
          gst_inference_meta_api_get_type ()));
  if (!infer_meta) {
    PRINT (APP1_LEVEL_DEBUG, "No infermeta priv found");
  }

  if (infer_meta) {
    priv->infer_result = malloc (sizeof (UsrInferenceData));
    priv->infer_result->num_classes = 0;
    priv->infer_result->num_bbox = 0;
    /* Below iterator function (vvas_appsrcsink_node_foreach) is just an example on to iterate through "GstInferenceMeta"
     * User needs to refer to the header "gstinferencemeta.h" in VVAS source repo to understand it and parse as per
     * individual needs. */
    g_node_traverse (infer_meta->prediction->predictions, G_PRE_ORDER,
        G_TRAVERSE_ALL, -1, vvas_appsrcsink_node_foreach, priv);

    usrbuf->infr_meta = priv->infer_result;
  }
  /* Map the output GstBuffer to get data pointer to share with user */
  gst_buffer_map (buffer, map, GST_MAP_READ);

  usrmeta = gst_buffer_get_vvas_usr_meta (buffer);
  /* Populate the "UsrBuff" with all details before sending to user */
  usrbuf->data = map->data;
  usrbuf->size = map->size;
  /* Retrieve the user added meta data and add it to UsrBuff */
  if (usrmeta) {
    usrbuf->usr_meta = ((GstVvasUsrMeta *) usrmeta)->usr_data;
  } else {
    usrbuf->usr_meta = NULL;
  }
  usrbuf->source_buf = sample;
  usrbuf->buff_map_info = map;

  /* Push the output "UsrBuff" to output queue, which will be
   * dequeued by consumer thread */
  PRINT (APP1_LEVEL_DEBUG, "Received data from GST");
  g_async_queue_push (priv->usrpriv->output_queue, usrbuf);
}

/*
 * Function to free up the output buffers from Gstreamer.
 * This has to be called by consumer thread after consuming the output buffer.
 */
void
vvas_app1_free_gst_output (UsrBuff* usrbuf)
{
  GstSample *sample = (GstSample *)usrbuf->source_buf;
  GstBuffer *buffer = usrbuf->buffer_ptr;
  GstMapInfo *map = usrbuf->buff_map_info;

  /* Unmap the output buffer as its been consumed */ 
  gst_buffer_unmap (buffer, map);

  if (sample) {
    gst_sample_unref (sample);
  }

  if (usrbuf->infr_meta) {
    free (usrbuf->infr_meta);
  }
  free (map);
  free (usrbuf);
}
