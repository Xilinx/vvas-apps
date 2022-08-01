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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <mqueue.h>
#include <jansson.h>
#include "vvas_appsrcsink.h"

#ifdef ENABLE_DRM_BUFF
#include <sys/ioctl.h>
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#include <linux/videodev2.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/mman.h>
#endif


#define NOT_USED 0

App1DebugLevel debug_min;

#define BUF_POOL_SIZE 50 

/* UsrConfig read from the JSON configuration */
typedef struct _UsrConfig
{
  VideoAlignment video_align;
  char *pipeline;
  App1DebugLevel debug_level;
  unsigned int buffer_size; 
}UsrConfig;

typedef struct _CustomData
{
  char *input_file;
  FILE *in_fptr, *out_fptr;
  UsrBuff buf_pool[BUF_POOL_SIZE];
  UsrData usrdata;
  long buf_idx;

  UsrConfig usr_config;

  pthread_t prod_thread;
  pthread_t cons_thread;
  mqd_t prod_mq;
  mqd_t cons_mq;
  int producer_sig_done;

  pthread_cond_t cond_var;
  pthread_mutex_t lock_var;
#ifdef ENABLE_DRM_BUFF
  int drmfd;
#endif
} CustomData;

typedef struct _UsrMeta
{
  long buf_idx;
  /* add more info */
} UsrMeta;

/* Parse the input JSON config file from user */
static int
vvas_appsrcsink_parse_config (UsrConfig *usr_config, char *config_file)
{
  json_t *root = NULL, *val = NULL, *stride = NULL;
  json_error_t error;
  int idx = 0;

  /* get root json object */
  root = json_load_file (config_file, JSON_DECODE_ANY, &error);
  if (!root) {
    PRINT (APP1_LEVEL_ERROR, "failed to load json file. reason %s", error.text);
    goto error;
  }
  /* Get the format of input file, as per "VideoFormat" in vvas_appsrcsink.h*/
  val = json_object_get (root, "format");
  if (!val) {
    PRINT (APP1_LEVEL_ERROR, "Format is not mentioned in the config file\n");
    goto error;
  } else if (!json_is_integer (val)) {
    PRINT(APP1_LEVEL_ERROR, "Format must be an integer\n");
    goto error;
  } else {
    usr_config->video_align.format = json_integer_value (val);
  }

  /* Get width of input file */
  val = json_object_get (root, "width");
  if (!val) {
    PRINT(APP1_LEVEL_ERROR, "Width is not mentioned in the config file\n");
    goto error;
  } else if (!json_is_integer (val)) {
    PRINT(APP1_LEVEL_ERROR, "Width must be an integer\n");
    goto error;
  } else {
    usr_config->video_align.width = json_integer_value (val);
  }

  /* Get height of input file */   
  val = json_object_get (root, "height");
  if (!val) {
    PRINT(APP1_LEVEL_ERROR, "Height is not mentioned in the config file\n");
    goto error;
  } else if (!json_is_integer (val)) {
    PRINT(APP1_LEVEL_ERROR, "Height must be an integer\n");
    goto error;
  } else {
    usr_config->video_align.height = json_integer_value (val);
  }

  /* Get the number of planes in input file, matter when the input file is RAW video file */   
  val = json_object_get (root, "num_planes");
  if (!val) {
    PRINT(APP1_LEVEL_ERROR, "Number of planes is not mentioned in the config file\n");
    goto error;
  } else if (!json_is_integer (val)) {
    PRINT(APP1_LEVEL_ERROR, "Number of planes must be an integer\n");
    goto error;
  } else {
    usr_config->video_align.n_plane = json_integer_value (val);
  }

  /* Get stride of input file */
  stride = json_object_get (root, "stride");
  if (!stride) {
    PRINT(APP1_LEVEL_ERROR, "Stride is not mentioned in the config file\n");
    goto error;
  } else if (!json_is_array (stride)) {
    PRINT(APP1_LEVEL_ERROR, "Stride must be an array of integers of size num_planes\n");
    goto error;
  } else {
    for(idx = 0; idx < json_array_size (stride); idx++) {
      usr_config->video_align.stride[idx] =
                 json_integer_value (json_array_get (stride, idx));
    }
  }

  /* Get the debug log level */
  val = json_object_get (root, "debug_level");
  if (!val) {
    PRINT(APP1_LEVEL_ERROR, "Debug level is not mentioned in the config file\n");
    goto error;
  } else if (!json_is_integer (val)) {
    PRINT(APP1_LEVEL_ERROR, "Debug level must be an integer\n");
    goto error;
  } else {
    usr_config->debug_level = json_integer_value (val);
  }

  val = json_object_get (root, "pipeline");
  if (!val) {
    PRINT(APP1_LEVEL_ERROR, "Gstreamer pipeline is not mentioned in the config file\n");
    goto error;
  } else if (!json_is_string (val)) {
    PRINT(APP1_LEVEL_ERROR, "Gstreamer pipeline must be a string\n");
    goto error;
  } else {
    usr_config->pipeline = g_strdup (json_string_value (val));
  }

  if (root)
    json_decref (root);

  return 0;
error:
  if (root)
    json_decref (root);

  return -1;
}

#ifdef ENABLE_DRM_BUFF
/* ref : "man 7 drm-memory" */
static int
init_dma_buffer_pool (CustomData * cd)
{
  int i;
  int ret;
  struct drm_mode_create_dumb creq;
  UsrBuff *usrbuf;

  PRINT (APP1_LEVEL_DEBUG, "Creating a DMA buffer pool");

  cd->drmfd = drmOpen ("xlnx", NULL);
  if (cd->drmfd < 0) {
    PRINT (APP1_LEVEL_ERROR, "drmOpen failed\n");
    return 0;
  }

  for (i = 0; i < BUF_POOL_SIZE; i++) {
    usrbuf = &(cd->buf_pool[i]);
    memset (&(usrbuf->video_align), 0, sizeof (VideoAlignment));
    memset (&(creq), 0, sizeof (creq));
    creq.width = cd->usr_config.video_align.width;
    creq.height = cd->usr_config.video_align.height;
    creq.bpp = 32;
    creq.size = creq.width * creq.height * 3;
    usrbuf->size = cd->usr_config.buffer_size;
    usrbuf->buf_idx = i;
    usrbuf->usr_data = &cd->usrdata;
    ret = ioctl (cd->drmfd, DRM_IOCTL_MODE_CREATE_DUMB, &(creq));
    if (ret) {
      /* buffer creation failed */
      /* Not able to allocate required buffers so exit */
      PRINT (APP1_LEVEL_ERROR, "Not able to allocate required buffers so exit error (%s)\n",
          strerror (errno));
      return 0;
    }
    /* creq.pitch, creq.handle and creq.size are filled by this ioctl with
     * the requested values and can be used now. */
    usrbuf->dma_handle = creq.handle;
    /* Release all buffes to free queue, as we just created the pool */
    vvas_appsrcsink_free_queue_release (cd->usrdata.free_queue, usrbuf);
  }

  return 1;
}

static int
deinit_dma_buffer_pool (CustomData * cd)
{
  int i;
  int ret;
  UsrBuff *usrbuf;
  struct drm_mode_destroy_dumb dreq;

  for (i = 0; i < BUF_POOL_SIZE; i++) {
    usrbuf = &(cd->buf_pool[i]);

    close (usrbuf->dma_prime_fd);

    memset (&(dreq), 0, sizeof (dreq));
    dreq.handle = usrbuf->dma_handle;
    ret = drmIoctl (cd->drmfd, DRM_IOCTL_MODE_DESTROY_DUMB, &(dreq));
    if (ret) {
      PRINT (APP1_LEVEL_ERROR, "Not able to deallocate required buffers (%s)\n",
          strerror (errno));
      /* nothing now */
    }
  }
  return 1;
}
#endif

/* Initialize the pool of "UsrBuff" and allocate memory for its
 * data member. 
 * Allocate using "malloc" in case of DC platform.
 * Allocate using either of "malloc" or DMA buffer based on compile time macro ENABLE_DRM_BUFF */
static int
init_buffer_pool (CustomData * cd)
{
#ifndef ENABLE_DRM_BUFF
  int i;
#endif
  cd->usr_config.buffer_size = 
            vvas_appsrcsink_get_buffer_size (&cd->usr_config.video_align);
  PRINT(APP1_LEVEL_INFO, "Buffer size to read : %d",
                                        cd->usr_config.buffer_size);
#ifdef ENABLE_DRM_BUFF
  return init_dma_buffer_pool (cd);
#else
  for (i = 0; i < BUF_POOL_SIZE; i++) {
    cd->buf_pool[i].data = (void *) calloc (1, cd->usr_config.buffer_size);
    if (cd->buf_pool[i].data == NULL) {
      /* Not able to allocate required buffers so exit */
      return 0;
    }
    cd->buf_pool[i].dma_prime_fd = -1;
    cd->buf_pool[i].size = cd->usr_config.buffer_size;
    cd->buf_pool[i].buf_idx = i;
    cd->buf_pool[i].usr_data = &cd->usrdata;
    memset (&cd->buf_pool[i].video_align, 0, sizeof (VideoAlignment));
    /* Release all buffes to free queue, as we just created the pool */
    vvas_appsrcsink_free_queue_release (cd->usrdata.free_queue, &(cd->buf_pool[i]));
  }

  return 1;
#endif
}

/* De-initialize the pool of "UsrBuff" */
static int
deinit_buffer_pool (CustomData * cd)
{
#ifdef ENABLE_DRM_BUFF
  return deinit_dma_buffer_pool (cd);
#else
  int i;
  for (i = 0; i < BUF_POOL_SIZE; i++) {
    if(cd->buf_pool[i].data) {
      free (cd->buf_pool[i].data);
    }
  }

  return 1;
#endif
}

/* APIs to manager free queue */
GAsyncQueue *vvas_appsrcsink_free_queue_create (void)
{
  return g_async_queue_new ();
}

void *vvas_appsrcsink_free_queue_acquire (GAsyncQueue *usr_buff_pool)
{
  return g_async_queue_pop (usr_buff_pool);
}

void vvas_appsrcsink_free_queue_release (GAsyncQueue *usr_buff_pool, void* data)
{
  g_async_queue_push (usr_buff_pool, data);
}

void vvas_appsrcsink_free_queue_delete (GAsyncQueue *usr_buff_pool)
{
  if (usr_buff_pool)
    g_async_queue_unref (usr_buff_pool);
}

/* Fill in "usrmeta" of "UsrBuff" with user's meta data */ 
static void *
vvas_appsrcsink_add_meta_data (long buf_idx)
{
  UsrMeta *usrmeta = (UsrMeta *) malloc (sizeof (UsrMeta));
  usrmeta->buf_idx = buf_idx;
  PRINT (APP1_LEVEL_DEBUG, "usrmeta = %p\n", usrmeta);
  return (void *) usrmeta;
}

/* This function will be called when there is a copy of 
 * GstBuffer across plugins in the pipeline, so that user meta data is not lost */
void *
vvas_appsrcsink_copy_usr_meta (void *usr_meta)
{
  UsrMeta *usrmeta = (UsrMeta *) usr_meta;
  UsrMeta *newmeta = NULL;

  if (!usr_meta)
    return NULL;

  newmeta = (UsrMeta *) malloc (sizeof (UsrMeta));
  newmeta->buf_idx = usrmeta->buf_idx;
  PRINT (APP1_LEVEL_DEBUG, "usrmeta = %p\n", newmeta);
  return (void *) newmeta;
}

/* This function will be called when user meta data can be released */
void
vvas_appsrcsink_free_usr_meta (void *usr_meta)
{
  if (usr_meta)
    free (usr_meta);
}

/* Consumer thread which waits on the output queue.
 * Pops the output "UsrBuff" and write its "data" member to file */
static void *
vvas_appsrcsink_consumer_thread (void *ptr)
{
  UsrBuff *usrbuf;

  CustomData *cd = (CustomData *) ptr;
  printf ("Starting Consumer thread\n");
  cd->usrdata.cons_signal_done = 0;
  if (!cd->out_fptr)
    if ((cd->out_fptr = fopen ("output.bin", "wb")) == NULL) {
      PRINT (APP1_LEVEL_ERROR, "Error! opening file");

      /* Program exits if the file pointer returns NULL.*/
      exit (1);
    }

  while (1) {
    PRINT (APP1_LEVEL_INFO, "Consumer wait for data");
    /* Pop out the outupt "UsrBuff" */
    usrbuf = g_async_queue_pop (cd->usrdata.output_queue);

    /* Receiving a "UsrBuff" with data as NULL signifies that End Of Stream is reached */
    if (!usrbuf->data) {
      free(usrbuf);
	  /* Signal the Gstreamer event loop that it can clean up the pipeline */
      pthread_cond_signal(&cd->usrdata.cons_end);
      cd->usrdata.cons_signal_done = 1;
      break;
    }
    /* Write the output buffer into file */
    PRINT (APP1_LEVEL_INFO, "Consumer got data of size %d", usrbuf->size);
    fwrite (usrbuf->data, usrbuf->size, 1, cd->out_fptr);

    if (usrbuf->usr_meta)
      PRINT (APP1_LEVEL_DEBUG, "Consumer got user meta %p %ld", usrbuf->usr_meta,
          ((UsrMeta *) (usrbuf->usr_meta))->buf_idx);

    if (usrbuf->infr_meta)
      PRINT (APP1_LEVEL_DEBUG, "Consumer infer infermeta %p", usrbuf->infr_meta);

    /* Call out for freeing the output buffer as its consumed */
    vvas_app1_free_gst_output (usrbuf);
  }
  return NULL;
}

/* Producer thread which reads from input file and pushed the
 * populated "UsrBuff" to input queue */
static void *
vvas_appsrcsink_producer_thread (void *ptr)
{
  UsrBuff *usrbuf;
  int read;
  CustomData *cd = (CustomData *) ptr;
  printf ("Starting Producer thread\n");

  if (!cd->in_fptr) {
    if ((cd->in_fptr = fopen (cd->input_file, "rb")) == NULL) {
      PRINT (APP1_LEVEL_ERROR, "Error! opening file");

      /* Program exits if the file pointer returns NULL.*/
      exit (1);
    } else {
      cd->buf_idx = 1;
    }
  }

  while (1) {
    /* Get the free "UsrBuff" from the pool */
    usrbuf = vvas_appsrcsink_free_queue_acquire (cd->usrdata.free_queue);
    PRINT(APP1_LEVEL_INFO, "Acquired buff at index : %ld", usrbuf->buf_idx);
#ifdef ENABLE_DRM_BUFF
    {
      int ret;
      struct drm_mode_map_dumb mreq;
      struct drm_prime_handle preq;
      memset (&preq, 0, sizeof (preq));
      preq.handle = usrbuf->dma_handle;
      ret = ioctl (cd->drmfd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &(preq));
      if (ret) {
        printf ("Not able to allocate required buffers so exit error (%s)",
            strerror (errno));
        return 0;
      } else
        usrbuf->dma_prime_fd = preq.fd;

      /* prepare buffer for memory mapping */
      memset (&mreq, 0, sizeof (mreq));
      mreq.handle = usrbuf->dma_handle;
      ret = drmIoctl (cd->drmfd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
      if (ret) {
        /* DRM buffer preparation failed */
        PRINT (APP1_LEVEL_ERROR, "Not able to allocate required buffers so exit error (%s)\n",
            strerror (errno));
        return 0;
      }
      /* mreq.offset now contains the new offset that can be used with mmap() */

      /* perform actual memory mapping */
      usrbuf->data = mmap (0, usrbuf->size, PROT_READ | PROT_WRITE, MAP_SHARED,
          cd->drmfd, mreq.offset);
      if (usrbuf->data == MAP_FAILED) {
        /* memory-mapping failed */
        printf ("Not able to allocate required buffers so exit error (%s)\n",
            strerror (errno));
        return 0;
      }
    }
#endif
    /* Read from input file */
    read = fread (usrbuf->data, 1, usrbuf->size, cd->in_fptr);
    if (read) {
      usrbuf->usr_meta = (void *) vvas_appsrcsink_add_meta_data (cd->buf_idx);
      usrbuf->size = read;
      /* Only if user has data alignment */
      {
        usrbuf->video_align.width = cd->usr_config.video_align.width;
        usrbuf->video_align.height = cd->usr_config.video_align.height;
        usrbuf->video_align.format = cd->usr_config.video_align.format;
        usrbuf->video_align.n_plane = cd->usr_config.video_align.n_plane;
        usrbuf->video_align.offset[0] = cd->usr_config.video_align.offset[0];
        usrbuf->video_align.offset[1] = cd->usr_config.video_align.offset[1];
        usrbuf->video_align.offset[2] = NOT_USED;
        usrbuf->video_align.offset[3] = NOT_USED;
        usrbuf->video_align.stride[0] = cd->usr_config.video_align.stride[0];
        usrbuf->video_align.stride[1] = cd->usr_config.video_align.stride[1];
        usrbuf->video_align.stride[2] = NOT_USED;
        usrbuf->video_align.stride[3] = NOT_USED;
      }
      cd->buf_idx++;
#ifdef ENABLE_DRM_BUFF
      munmap (usrbuf->data, usrbuf->size);
#endif
    } else {
	  /* Fill "UsrBuff" with empty and NULL ptr to mark the End Of Stream */
      usrbuf->dma_prime_fd = -1;
      usrbuf->usr_meta = NULL;
      usrbuf->size = 0;
      {
        usrbuf->video_align.n_plane = 0;
        usrbuf->video_align.offset[0] = 0;
        usrbuf->video_align.offset[1] = 0;
        usrbuf->video_align.offset[2] = 0;
        usrbuf->video_align.offset[3] = 0;
        usrbuf->video_align.stride[0] = 0;
        usrbuf->video_align.stride[1] = 0;
        usrbuf->video_align.stride[2] = 0;
        usrbuf->video_align.stride[3] = 0;
      }
    }

    PRINT (APP1_LEVEL_INFO, "Producer has data buf_idx = %ld size =%d usr_meta = %p",
        usrbuf->buf_idx, usrbuf->size, usrbuf->usr_meta);
    /* Push the prepared "UsrBuff" to input queue */
    g_async_queue_push (cd->usrdata.input_queue, usrbuf);

    /* Signal to create Gstreamer pipeline */
    pthread_cond_signal(&cd->cond_var);
    cd->producer_sig_done = 1;

    if (!usrbuf->size)          /* EOS */
      break;
  }

  return NULL;
}


int
main (int argc, char *argv[])
{
  int ret;
  CustomData *cd = (CustomData *) calloc (1, sizeof (CustomData));

  if (argc < 2) {
    printf ("usage : %s <Input File> <JSON Config>\n", argv[0]);
    exit (-1);
  }

  cd->input_file = argv[1];
  cd->in_fptr = NULL;
  cd->out_fptr = NULL;

  /* Let's go with error level before parsing config */
  debug_min = APP1_LEVEL_ERROR;
  vvas_appsrcsink_parse_config (&cd->usr_config, argv[2]);

  /* Switch to log level mentioned in the config */
  debug_min = cd->usr_config.debug_level;
  /* Take a copy of Gstreamer pipeline that user has entered in JSON input file*/
  cd->usrdata.pipe = (char *) malloc (strlen (cd->usr_config.pipeline) + 1);
  strcpy (cd->usrdata.pipe, cd->usr_config.pipeline);
  /* Create the input queue which will be enqueued by producer thread and 
   * dequeued when Gstreamer asks for more input */
  cd->usrdata.input_queue = g_async_queue_new ();
  /* Create the output queue which will be enqueued by Gstreamer output and
   * dequeued by consumer thread.  */
  cd->usrdata.output_queue = g_async_queue_new ();
  cd->usrdata.free_queue = vvas_appsrcsink_free_queue_create ();
  cd->usrdata.copy_usr_meta = vvas_appsrcsink_copy_usr_meta;
  cd->usrdata.free_usr_meta = vvas_appsrcsink_free_usr_meta;
  cd->usrdata.cons_signal_done = 0;
  cd->usrdata.priv = (void *) cd;
  cd->producer_sig_done = 0;

  /* Initialize the input pool of "UsrBuff", this structure can changed as per need */
  if (!init_buffer_pool (cd)) {
    PRINT (APP1_LEVEL_ERROR, "Failed to allocate buffers for pool");
    return (-1);
  }

  /* Create the producer thread which will start the buffer input feed */
  ret =
      pthread_create (&cd->prod_thread, NULL, vvas_appsrcsink_producer_thread,
      (void *) cd);
  if (ret != 0) {
    exit (-1);
  } else {
    pthread_setname_np (cd->prod_thread, "producer");
  }
 
  /* Create the consumer thread which will wait on the output queue.
   * Output queue will be enqueued as and when we get output from Gstreamer
   * pipeline */ 
  ret =
      pthread_create (&cd->cons_thread, NULL, vvas_appsrcsink_consumer_thread,
      (void *) cd);
  if (ret != 0) {
    exit (-1);
  } else {
    pthread_setname_np (cd->cons_thread, "consumer");
  }

  /* Wait util producer generates one buffer before creating Gstreamer pipeline */
  if (!cd->producer_sig_done) {
    pthread_mutex_lock(&cd->lock_var);
    pthread_cond_wait(&cd->cond_var, &cd->lock_var);
    pthread_mutex_unlock(&cd->lock_var);
  }
  printf("Starting Gstreamer thread \n");
  vvas_app1_start_gst_pipeline (&(cd->usrdata));

  /* Clean up pipe pointer and wait for producer and consufer to finish */
  free(cd->usrdata.pipe);
  pthread_join (cd->prod_thread, NULL);
  pthread_join (cd->cons_thread, NULL);

  /* Clean up all queues */
  vvas_appsrcsink_free_queue_delete (cd->usrdata.free_queue);
  g_async_queue_unref (cd->usrdata.input_queue);
  g_async_queue_unref (cd->usrdata.output_queue);
  /* Clean up the input pool */
  deinit_buffer_pool (cd);

  /* Clean up the CustomData */
  if (cd->usr_config.pipeline)
    free (cd->usr_config.pipeline);
  free(cd);

  return 0;
}
