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


#include <stdio.h>
#include <glib.h>

#define VIDEO_MAX_PLANES 4
#define NV12 GST_VIDEO_FORMAT_NV12
#define RGB GST_VIDEO_FORMAT_RGB
#define MAX_INFERENCE_RESULTS 20
#define MAX_LABEL_LENGTH 1024

typedef struct _UsrData UsrData;
typedef struct _UsrClassification UsrClassification;
typedef struct _UsrBoundingBox UsrBoundingBox;
typedef struct _UsrInferenceData UsrInferenceData;

typedef enum {
  APP1_LEVEL_NONE = 0,
  APP1_LEVEL_ERROR = 1,
  APP1_LEVEL_WARNING = 2,
  APP1_LEVEL_INFO = 3,
  APP1_LEVEL_DEBUG = 4,
} App1DebugLevel;

extern App1DebugLevel debug_min;

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define PRINT(level, ...) {\
  do {\
      if (level <= debug_min) {\
        printf("[%s:%d]: ", __func__, __LINE__);\
        printf(__VA_ARGS__);\
        printf("\n");\
      }\
  } while (0); \
}

typedef enum
{
  APP1_VIDEO_FORMAT_UNKNOWN = 0,
  APP1_VIDEO_FORMAT_NV12 = 1,
  APP1_VIDEO_FORMAT_RGB = 2,
  APP1_VIDEO_FORMAT_ENCODED = 3
} VideoFormat;

typedef struct _VideoAlignment
{
  long unsigned int offset[VIDEO_MAX_PLANES];
  int stride[VIDEO_MAX_PLANES];
  int n_plane;
  int width;
  int height;
  int format;
}VideoAlignment;

/* CopyUsrMeta : called when user meta data need to be copied between GstBuffer
 * usr_meta : pointer to user metadata
 */
typedef void *(*CopyUsrMeta) (void *usr_meta);

/* FreeUsrMeta : called when user meta data need to be freed
 * usr_meta : pointer to user metadata
 */
typedef void (*FreeUsrMeta) (void *usr_meta);

typedef struct _usrbuff
{
  /* Data pointer for the media buffer */
  void *data;
  /* Size of the buffer */
  int size;
  /* dmabuf file descriptor */ 
  int dma_prime_fd;
#ifdef ENABLE_DRM_BUFF
  /* dma handle */ 
  unsigned int dma_handle;
#endif
  /* index to track array */
  long buf_idx;
  /* Pointer to user meta to be tagged with buffer */
  void *usr_meta;
  /* Meta data to inference structure in case of ML pipeline */
  UsrInferenceData *infr_meta;

  /* Buffer management and clean up purpose */
  void *source_buf;
  void *buffer_ptr;
  void *buff_map_info;
  UsrData *usr_data;

  /* Alignment details of the input video file */
  VideoAlignment video_align;
} UsrBuff;


struct _UsrData
{
  char *pipe;                   /* pipe to run */
  void *priv;                   /* user private */

  /* Producer to Gstreamer thread */
  GAsyncQueue *input_queue;
  /* Gstreamer to Consumer thread */
  GAsyncQueue *output_queue;
  /* Free queue to be picked up by producer */
  GAsyncQueue *free_queue;

  pthread_cond_t cons_end;
  pthread_mutex_t cons_lock;
  int cons_signal_done;

  /* called when user meta need to copy/transform b/w plugins */
  CopyUsrMeta copy_usr_meta;
  /* called when user metadata need to free */
  FreeUsrMeta free_usr_meta;
};

struct _UsrClassification
{
  long classification_id;
  int class_id;
  float class_prob;
  char class_label[MAX_LABEL_LENGTH];
};

struct _UsrBoundingBox
{
  int x;
  int y;
  unsigned int width;
  unsigned int height;
};

struct _UsrInferenceData
{
  unsigned int num_bbox;
  UsrBoundingBox bbox[MAX_INFERENCE_RESULTS];
  unsigned int num_classes;
  UsrClassification classes[MAX_INFERENCE_RESULTS];
};

void *
vvas_appsrcsink_copy_usr_meta (void *usr_meta);

void
vvas_appsrcsink_free_usr_meta (void *usr_meta);

/* Start the pipe mentioned in usrdata 
 * Pipeline will be launched in a different thread */ 
void vvas_app1_start_gst_pipeline (UsrData * usrdata);

/* Free up the output from Gstreamer after being consumed by consumer  */
void vvas_app1_free_gst_output (UsrBuff* usrbuf);

/* Methods to maintain free queue of the buffers in pool */
GAsyncQueue *vvas_appsrcsink_free_queue_create (void);

void *vvas_appsrcsink_free_queue_acquire (GAsyncQueue *usr_buff_pool);

void vvas_appsrcsink_free_queue_release (GAsyncQueue *usr_buff_pool, void* data);

void vvas_appsrcsink_free_queue_delete (GAsyncQueue *usr_buff_pool);

/* Findout how much to read from file in each read call.
 * In case of RAW video file input, we read the size of one frame.
 * In case of encoded file we read 10k in each read. */
unsigned int
vvas_appsrcsink_get_buffer_size (VideoAlignment *video_align);
