/*
test_gst_klv_dec_v1.cpp
given a ts file, this program build the following pipeline
tsFileSrc->tsDemux|->videoQueue->h264parse->videodec->videoSink      (visualizing video)
                  |                                    or
                  |                                 ->videoConv->vidCapsFilter->yuvSink (dump individual frame in to a png file)
                  |->klvQueue->klvSink  (show binary metadata on console)

*/                 
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>

#include <iostream>
#include <cassert>
#include <string>
#include <thread>


#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include "HostYuvFrm.h"


#define MY_VIDEO_W 720
#define MY_VIDEO_H 480
#define MY_VIDEO_FPS 30


/* Structure to contain all our information, so we can pass it to callbacks */
struct AppElements
{
  GstElement *pipeline;
  GstElement *source;
  GstElement *tsDemux;
  GstElement *videoQueue;
  GstElement *h264parse;
  GstElement *avdec;
  //GstElement *videoSink;
  GstElement *videoConv;
  GstElement *vidCapsFilter;
  GstElement *yuvSink;

  GstElement *klvQueue;
  GstElement *klvSink;
};

struct UserData{
    UserData()
    : m_hostYuvFrm( new app::HostYuvFrm(MY_VIDEO_W, MY_VIDEO_H, 0))
    {
      	m_timeDurationNanoSec = gst_util_uint64_scale_int( 1, GST_SECOND /*1sec = 1e+9 nano sec*/, MY_VIDEO_FPS /*25*/);
        m_hostYuvFrm->setToZeros();
    }

		std::atomic<bool> 	m_isOpened{false};
		std::atomic<bool>   m_mainLoopEnd{false};
		std::atomic<bool>	  m_requestToClose{false};  //request to close
		app::HostYuvFrmPtr	m_hostYuvFrm;	            //YUV frm read(hd copy) from <m_cudaYuvQ4Rtsp>, it will be RTSP to client, 

		int64_t				      m_timeDurationNanoSec{0};
		int					        m_eosRequestCnt{0};
    GstClockTime        m_timestamp{0};
    uint64_t            m_frmNum{0};
};


/* Handler for the pad-added signal */
static void pad_added_handler(GstElement *src, GstPad *pad, AppElements *appEle);
static GstFlowReturn new_klv_sample_cb(GstElement *sink, UserData *appEle);
static GstFlowReturn new_yuv_sample_cb(GstElement *sink, UserData *appEle);
static GstFlowReturn img_eos_cb(GstElement *sink, UserData *appEle);
static GstFlowReturn klv_eos_cb(GstElement *sink, UserData *appEle);

int test_gst_klv_dec_v1(int argc, char *argv[])
{
  AppElements appEle;
  UserData  user_data;
  
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;
  gboolean terminate = FALSE;

  // copy first argument to fileName  - this is the file to be played
  std::string SrcFileName = "./tmp1.ts";
  if (argc>2){
   printf("usage: ./a.out 6 videoFile.ts");
   SrcFileName = std::string(argv[2]);
  }

  /* Initialize GStreamer */
  gst_init(&argc, &argv);

  /* Create the elements */
  appEle.source = gst_element_factory_make("filesrc", "source");
  appEle.tsDemux = gst_element_factory_make("tsdemux", "demux");
  appEle.videoQueue = gst_element_factory_make("queue", "videoQueue");
  appEle.h264parse = gst_element_factory_make("h264parse", "h264parse");
  //appEle.avdec = gst_element_factory_make("x264dec", "avdec");
  appEle.avdec = gst_element_factory_make("omxh264dec", "avdec");
  //appEle.videoSink = gst_element_factory_make("autovideosink", "videoSink");

  appEle.videoConv = gst_element_factory_make("nvvidconv", "videoConv");
  appEle.vidCapsFilter = gst_element_factory_make("capsfilter", NULL);

  appEle.yuvSink   = gst_element_factory_make("appsink", "yuvSink");
  
  appEle.klvQueue = gst_element_factory_make("queue", "klvQueue");
  appEle.klvSink = gst_element_factory_make("appsink", "klvSink");

  /* Create the empty pipeline */
  appEle.pipeline = gst_pipeline_new("decode-pipeline");

  if (!appEle.pipeline || !appEle.source || !appEle.tsDemux || !appEle.videoQueue 
    || !appEle.h264parse || !appEle.avdec || /*!appEle.videoSink*/ !appEle.videoConv || !appEle.vidCapsFilter || !appEle.yuvSink 
    || !appEle.klvQueue || !appEle.klvSink)
  {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }

  //GstCaps* videoCaps = gst_caps_from_string("video/x-raw, format=I420, width=720, height=480");

  GstCaps* videoCaps = gst_caps_new_simple("video/x-raw",
                                    "format", G_TYPE_STRING, "I420",
                                    "width", G_TYPE_INT, MY_VIDEO_W,
                                    "height", G_TYPE_INT, MY_VIDEO_H,
                                     NULL);

  g_object_set( appEle.vidCapsFilter, "caps", videoCaps, NULL);


  /* Build the pipeline. Note that we are NOT linking the source at this point. We will do it later. */
  gst_bin_add_many(GST_BIN(appEle.pipeline), appEle.source, appEle.tsDemux, appEle.videoQueue, 
    appEle.h264parse, appEle.avdec, 
    //appEle.videoSink, 
    appEle.videoConv, appEle.vidCapsFilter, appEle.yuvSink,
    appEle.klvQueue, appEle.klvSink, NULL);

  if (!gst_element_link(appEle.source, appEle.tsDemux))
  {
    g_printerr("Cannot link source to demux.\n");
    gst_object_unref(appEle.pipeline);
    return -1;
  }

  if (!gst_element_link_many(appEle.videoQueue, appEle.h264parse, appEle.avdec, /*appEle.videoSink,*/ appEle.videoConv, appEle.vidCapsFilter, appEle.yuvSink,  NULL))
  {
    g_printerr("Video processing elements could not be linked.\n");
    gst_object_unref(appEle.pipeline);
    return -1;
  }

  if (!gst_element_link(appEle.klvQueue, appEle.klvSink))
  {
    g_printerr("Data processing elements could not be linked.\n");
    gst_object_unref(appEle.pipeline);
    return -1;
  }

  /* Set the URI to play */
  g_object_set(G_OBJECT(appEle.source), "location", SrcFileName.c_str(), NULL);

  /* Configure appsink */
  g_object_set(appEle.klvSink, "emit-signals", TRUE, NULL);
  g_signal_connect(appEle.klvSink, "new-sample", G_CALLBACK(new_klv_sample_cb), &user_data);
  g_signal_connect(appEle.klvSink, "eos", G_CALLBACK(klv_eos_cb), &user_data);

  /* Connect to the pad-added signal */
  g_signal_connect(appEle.tsDemux, "pad-added", G_CALLBACK(pad_added_handler), &appEle);

  g_object_set(appEle.yuvSink, "emit-signals", TRUE, NULL);
  g_signal_connect(appEle.yuvSink, "new_sample", G_CALLBACK(new_yuv_sample_cb), &user_data);
  g_signal_connect(appEle.yuvSink, "eos", G_CALLBACK(img_eos_cb), &user_data);

  /* Start playing */
  ret = gst_element_set_state(appEle.pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE)
  {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(appEle.pipeline);
    return -1;
  }

  /* Listen to the bus */
  bus = gst_element_get_bus(appEle.pipeline);
  do
  {
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    /* Parse message */
    if (msg != NULL)
    {
      GError *err;
      gchar *debug_info;

      switch (GST_MESSAGE_TYPE(msg))
      {
      case GST_MESSAGE_ERROR:
        gst_message_parse_error(msg, &err, &debug_info);
        g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
        g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
        g_clear_error(&err);
        g_free(debug_info);
        terminate = TRUE;
        break;
      case GST_MESSAGE_EOS:
        g_print("End-Of-Stream reached.\n");
        terminate = TRUE;
        break;
      case GST_MESSAGE_STATE_CHANGED:
        /* We are only interested in state-changed messages from the pipeline */
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(appEle.pipeline))
        {
          GstState old_state, new_state, pending_state;
          gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
          g_print("Pipeline state changed from %s to %s:\n",
                  gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
        }
        break;
      default:
        /* We should not reach here */
        g_printerr("Unexpected message received.\n");
        break;
      }
      gst_message_unref(msg);
    }
  } while (!terminate);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  /* Free resources */
  gst_object_unref(bus);
  gst_element_set_state(appEle.pipeline, GST_STATE_NULL);
  gst_object_unref(appEle.pipeline);

  return 0;
}

/* The appsink has received a buffer */
static GstFlowReturn new_klv_sample_cb(GstElement *sink, UserData *user_data)
{
  GstSample *sample;

  //g_print("new_klv_sample_cb--AA\n");
  /* Retrieve the buffer, will wait untill a sample is ready */
  g_signal_emit_by_name(sink, "pull-sample", &sample);
  if (sample)
  {
    GstBuffer *gstBuffer = gst_sample_get_buffer(sample);

    if (gstBuffer)
    {
      auto pts = GST_BUFFER_PTS(gstBuffer);
      auto dts = GST_BUFFER_DTS(gstBuffer);

      gsize bufSize = gst_buffer_get_size(gstBuffer);
      g_print("Klv buffer size %ld. PTS %ld   DTS %ld\n", bufSize, pts, dts);

      GstMapInfo map;
      gst_buffer_map(gstBuffer, &map, GST_MAP_READ);

      const char *p = (char *)map.data;
      for(int i=0; i<map.size; ++i){
          printf("%c", p[i]);
      }
      printf("\n");

      //char *jsonPckt = decode601Pckt((char *)map.data, map.size);
      //g_print("%s \n", jsonPckt);

      gst_sample_unref(sample);
      return GST_FLOW_OK;
    }
  }

  return GST_FLOW_ERROR;
}

/* This function will be called by the pad-added signal */
static void pad_added_handler(GstElement *src, GstPad *new_pad, AppElements *appEle)
{
  GstElement *sink;
  GstPad *sink_pad = NULL;
  GstPadLinkReturn ret;
  GstCaps *new_pad_caps = NULL;
  GstStructure *new_pad_struct = NULL;
  const gchar *new_pad_type = NULL;

  /* Check the new pad's type */
  new_pad_caps = gst_pad_get_current_caps(new_pad);
  new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
  new_pad_type = gst_structure_get_name(new_pad_struct);

  g_print("Received new pad '%s' from '%s' of type '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src), new_pad_type);

  if (g_str_has_prefix(new_pad_type, "video/x-h264")){
    sink_pad = gst_element_get_static_pad(appEle->videoQueue, "sink");
  }
  else if (g_str_has_prefix(new_pad_type, "meta/x-klv")){
    sink_pad = gst_element_get_static_pad(appEle->klvQueue, "sink");
  }
  else
  {
    sink = gst_element_factory_make("fakesink", NULL);
    gst_bin_add(GST_BIN(appEle->pipeline), sink);
    sink_pad = gst_element_get_static_pad(sink, "sink");
    gst_element_sync_state_with_parent(sink);    
  }

  if (gst_pad_is_linked(sink_pad) || sink_pad == NULL)
  {
    g_print("We are already linked. Ignoring.\n");
    goto exit;
  }

  /* Attempt the link */
  ret = gst_pad_link(new_pad, sink_pad);
  if (GST_PAD_LINK_FAILED(ret))
    g_print("Type is '%s' but link failed.\n", new_pad_type);
  else
    g_print("Link succeeded (type '%s').\n", new_pad_type);

exit:
  /* Unreference the new pad's caps, if we got them */
  if (new_pad_caps != NULL)
    gst_caps_unref(new_pad_caps);

  /* Unreference the sink pad */
  if (sink_pad != NULL)
    gst_object_unref(sink_pad);


  g_print("pad_added_handler(): well done!\n");
}

static GstFlowReturn new_yuv_sample_cb(GstElement *sink, UserData *user_data)
{
   printf("new_yuv_sample_cb()\n");

	//gateway to access this->xyz
	UserData *pThis = reinterpret_cast<UserData*>(user_data);

	GstSample *sample = NULL;

  //sync "pull-sample", wait utill the sampel is ready
  //async "try-pull-sample", do not wait 
	g_signal_emit_by_name(sink, "pull-sample", &sample, NULL);
	if (sample){
		GstBuffer *buffer = NULL;
		GstCaps   *caps = NULL;
		GstMapInfo map = { 0 };

		caps = gst_sample_get_caps(sample);
		if (!caps){
			printf("CapSaveRtspH264::new_sample_cb(): could not get snapshot format.\n");
		}
		gst_caps_get_structure(caps, 0);
		buffer = gst_sample_get_buffer(sample);
		gst_buffer_map(buffer, &map, GST_MAP_READ);

		if( pThis->m_hostYuvFrm->sz_ == map.size ){
		  //local dump and debug 
		  pThis->m_hostYuvFrm->hdCopyFrom(map.data, map.size, pThis->m_frmNum);
      if(0==pThis->m_frmNum%50){
		    pThis->m_hostYuvFrm->dump(".", "yuv_h");
      }
	    printf( "new_yuv_sample_cb(): mapSize=%lu, fn=%lu\n", map.size, pThis->m_frmNum);
    }
    else{
      printf("CapSaveRtspH264::new_sample_cb(): size does not match, m_hostYuvFrm->sz_=%d, map.size=%d\n", pThis->m_hostYuvFrm->sz_, map.size);
    }

		gst_buffer_unmap(buffer, &map);
		gst_sample_unref(sample);

		pThis->m_frmNum++;
	}
	else{
		printf("CapSaveRtspH264::new_sample_cb(): could not make snapshot\n");
	}

	return GST_FLOW_OK; 
}

static GstFlowReturn img_eos_cb(GstElement *sink, UserData *user_data)
{
  printf("img_eos_cb()\n");
}

static GstFlowReturn klv_eos_cb(GstElement *sink, UserData *user_data)
{
  printf("klv_eos_cb()\n");
}
