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


typedef struct _App1Priv
{
  GstElement *pipeline, *app_source, *app_sink;
  GMainLoop *main_loop;         /* GLib's Main Loop */

  guint sourceid;               /* To control the GSource */
  UsrData *usrpriv;
  UsrInferenceData *infer_result;
  GstAllocator *dmabuf_alloc;

  mqd_t input_mq;
  mqd_t output_mq;
} App1Priv;


void
vvas_appsrcsink_start_buffer_feed (GstElement * source, guint size, App1Priv * priv);
void
vvas_appsrcsink_stop_buffer_feed (GstElement * source, App1Priv * priv);
GstFlowReturn
vvas_appsrcsink_appsrc_new_sample (GstElement * sink, App1Priv * priv);

GstBuffer *
vvas_appsrcsink_get_buffer_from_usr (App1Priv * priv);

void
vvas_appsrcsink_send_data_to_user (App1Priv * priv, GstSample *sample);
