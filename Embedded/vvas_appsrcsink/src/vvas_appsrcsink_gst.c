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

/* Gstreamer bus handler to handle GstMessage posted on the pipeline bus */
static gboolean
vvas_appsrcsink_bus_handler (GstBus * bus, GstMessage * msg, gpointer data)
{
  App1Priv * priv = (App1Priv *)data;

  switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_EOS: {
	  UsrBuff *usrbuf = (UsrBuff *) calloc (1, sizeof (UsrBuff));

	  printf("Got End-Of-Stream message\n");
	  /* Send out a UsrBuff with data set to NULL and size to 0 to notify
	   * consumer that we have hit the end of stream */
	  usrbuf->data = NULL;
	  usrbuf->size = 0;
	  usrbuf->infr_meta = NULL;
	  usrbuf->usr_meta = NULL;
	  usrbuf->source_buf = NULL;

	  /* Clenup the dma allocator, doing it here because after the eos callback
	   * it's NULL but still leaking */
	  if (priv->dmabuf_alloc) {
		g_object_unref (priv->dmabuf_alloc);
	  }

	  g_async_queue_push (priv->usrpriv->output_queue, usrbuf);

	  g_main_loop_quit (priv->main_loop);
      break;
	}
	case GST_MESSAGE_ERROR: {
	  GError *err;
	  gchar *debug_info;

      printf("Got Error message from pipeline\n");

	  gst_message_parse_error (msg, &err, &debug_info);
	  g_printerr ("Error received from element %s: %s\n",
		  GST_OBJECT_NAME (msg->src), err->message);
	  g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
	  g_clear_error (&err);
	  g_free (debug_info);

	  g_main_loop_quit (priv->main_loop);
      break;
	}
    case GST_MESSAGE_PROGRESS: {
      GstProgressType type;
      gchar *code, *text;

      gst_message_parse_progress (msg, &type, &code, &text);

      printf("Progress: (%s) %s\n", code, text);
      g_free (code);
      g_free (text);
      break;
    }
    case GST_MESSAGE_STATE_CHANGED: {
      GstState old_state, new_state;
      /* Report what state the pipeline has readed */
      gst_message_parse_state_changed (msg, &old_state, &new_state, NULL);
      if (!strcmp(GST_OBJECT_NAME (msg->src), "pipeline0")) {
		g_print ("Pipeline changed state from %s to %s.\n",
			gst_element_state_get_name (old_state),
			gst_element_state_get_name (new_state));
      }
      break;
    }
    default:
      break;
  }
  return TRUE;
}

/*
 * This function is called iteratively when "need-data" signal from
 * "appsrc" is triggered */
static gboolean
vvas_appsrcsink_appsrc_read_data (App1Priv * priv)
{
  GstBuffer *buffer;
  GstFlowReturn ret;

  buffer = vvas_appsrcsink_get_buffer_from_usr (priv);
  if (!buffer) {
    /* we are EOS, send end-of-stream and remove the source */
    g_signal_emit_by_name (priv->app_source, "end-of-stream", &ret);
    return FALSE;
  }

  /* Push the buffer into the appsrc */
  g_signal_emit_by_name (priv->app_source, "push-buffer", buffer, &ret);

  /* Free the buffer now that we are done with it */
  gst_buffer_unref (buffer);

  if (ret != GST_FLOW_OK) {
    /* We got some error, stop sending priv */
    return FALSE;
  }

  return TRUE;
}

/* This callback will be triggered when appsink is ready with
 * a output buffer */
GstFlowReturn
vvas_appsrcsink_appsrc_new_sample (GstElement * sink, App1Priv * priv)
{
  GstSample *sample;

  /* Retrieve the buffer */
  g_signal_emit_by_name (sink, "pull-sample", &sample);
  if (sample) {
    /* Send output buffer to user, retain sample, will be unreffed after user
     * consumes the buffer */
    vvas_appsrcsink_send_data_to_user (priv, sample);

    return GST_FLOW_OK;
  }

  return GST_FLOW_ERROR;
}

/* This callback is triggered when appsrc has got enough input buffers and we can stop sending.
 * We remove the idle handler from the mainloop */
void
vvas_appsrcsink_stop_buffer_feed (GstElement * source, App1Priv * priv)
{
  if (priv->sourceid != 0) {
    PRINT (APP1_LEVEL_DEBUG, "Stop feeding appsrc");
    g_source_remove (priv->sourceid);
    priv->sourceid = 0;
  }
}

/* This signal callback triggers when appsrc needs data. Here, we add an idle handler
 * to the mainloop to start pushing data into the appsrc */
void
vvas_appsrcsink_start_buffer_feed (GstElement * source, guint size, App1Priv * priv)
{
  if (priv->sourceid == 0) {
    PRINT (APP1_LEVEL_DEBUG, "Start feeding appsrc");
    priv->sourceid = g_idle_add ((GSourceFunc) vvas_appsrcsink_appsrc_read_data, priv);
  }
}

/* This function creates the Gstreamer pipeline as given by he user from 
 * config file */
void
vvas_app1_start_gst_pipeline (UsrData * usrpriv)
{
  GstBus *bus;
  guint bus_watch_id = 0;
  gchar *string = NULL;
  App1Priv *priv = g_new0 (App1Priv, 1);

  /* Initialize app priv structure */
  memset (priv, 0, sizeof (App1Priv));
  priv->usrpriv = usrpriv;

  /* Initialize GStreamer */
  gst_init (NULL, NULL);

  /* Create a GLib Main Loop and set it to run */
  priv->main_loop = g_main_loop_new (NULL, FALSE);

  string =
      g_strdup_printf
      ("appsrc name=usrsrc ! queue ! %s ! queue ! appsink name=usrsink",
      priv->usrpriv->pipe);
  printf ("\nrunning pipe = %s\n\n", string);

  priv->pipeline = gst_parse_launch (string, NULL);
  g_free (string);

  /* Configure appsrc, add callbacks to control input buffers */
  priv->app_source = gst_bin_get_by_name (GST_BIN (priv->pipeline), "usrsrc");
  g_signal_connect (priv->app_source, "need-data", G_CALLBACK (vvas_appsrcsink_start_buffer_feed),
      priv);
  g_signal_connect (priv->app_source, "enough-data", G_CALLBACK (vvas_appsrcsink_stop_buffer_feed),
      priv);

  /* Configure appsink, add callbacks to get outputs */
  priv->app_sink = gst_bin_get_by_name (GST_BIN (priv->pipeline), "usrsink");
  g_object_set (priv->app_sink, "emit-signals", TRUE, NULL);
  g_signal_connect (priv->app_sink, "new-sample", G_CALLBACK (vvas_appsrcsink_appsrc_new_sample),
      priv);

  /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
  bus = gst_element_get_bus (priv->pipeline);
  bus_watch_id = gst_bus_add_watch (bus, vvas_appsrcsink_bus_handler, priv);

  g_object_unref (priv->app_source);
  g_object_unref (priv->app_sink);

  printf("Setting pipeline to PLAYING state\n");
  /* Start playing the pipeline */
  gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);

  /* Start the Glib event loop */
  g_main_loop_run (priv->main_loop);

  /* Lets wait for consumer to end and then cleanup Gstreamer to avoid
   * Gstreamer finalizing resources before cleaning up resources by consumer */
  if (!usrpriv->cons_signal_done) {
    pthread_mutex_lock(&usrpriv->cons_lock);
    pthread_cond_wait (&usrpriv->cons_end, &usrpriv->cons_lock);
    pthread_mutex_unlock(&usrpriv->cons_lock);
  }

  /* Free resources */
  gst_element_set_state (priv->pipeline, GST_STATE_NULL);
  gst_object_unref (priv->pipeline);
  g_source_remove (bus_watch_id);
  gst_object_unref (bus);
  g_main_loop_unref (priv->main_loop);

  g_free (priv);

  gst_deinit ();
}
