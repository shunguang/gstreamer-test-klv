//modified from https://github.com/impleotv/gstreamer-test-klv

/*
testvideoSrc->srcCapsFilter->timeoverla->encoderH264->vidCapsFilter->video_queue->mpegtsmux->filesink
                                                                                ^
                                                                                |                   
                                                                              metadataSrc
*/  
//-------------------------------------------------------------------
// test_gst_klv_enc_min_v1.cpp
// an example to enc metadata wand image by two callback functions
//-------------------------------------------------------------------
#include <iostream>
#include <cassert>
#include <string>
#include <thread>
#include <stdio.h>
#include <unistd.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <dlfcn.h>
#include <unistd.h>

#include "HostYuvFrm.h"


#define MY_VIDEO_W 720
#define MY_VIDEO_H 480
#define MY_VIDEO_FPS 25
#define MY_KLV_FPS 25
#define USE_VIDEO_TEST_SRC 0
#define APP_ASYNC_KLV 0

struct AppCfg{
  AppCfg()
  : imgW(720)
  , imgH(480)
  //, encName("omxh264enc")
  , encName("x264enc")
  {}

  const int imgW{720};
  const int imgH{480};
  const std::string encName{""};
};

struct UserData{
    UserData()
    //: m_hostYuvFrm( new app::HostYuvFrm(MY_VIDEO_W, MY_VIDEO_H, 0))
    : m_hostYuvFrm( new app::HostYuvFrm(MY_VIDEO_W, MY_VIDEO_H, 0, "../../swu-modified/sampleImg.jpg") )
    {
    
      	m_timeDuration_nanoSec = gst_util_uint64_scale_int( 1, GST_SECOND /*1sec = 1e+9 nano sec*/, MY_VIDEO_FPS /*25*/);
      	m_timeDurationKlv_nanoSec = gst_util_uint64_scale_int( 1, GST_SECOND /*1sec = 1e+9 nano sec*/, MY_KLV_FPS /*25*/);
        //m_hostYuvFrm->dump("./", "inputImg");

        m_osClock= gst_system_clock_obtain();  
        lastPcktTime = std::chrono::steady_clock::now();
        lastImgTime = std::chrono::steady_clock::now();
    }

		std::atomic<bool> 	m_isOpened{false};
		std::atomic<bool>   m_mainLoopEnd{false};
		std::atomic<bool>	  m_requestToClose{false};  //request to close
		app::HostYuvFrmPtr	m_hostYuvFrm{nullptr};	  //YUV420  frm 

    //
		int64_t				      m_timeDuration_nanoSec{0};
		int					        m_eosRequestCnt{0};
    GstClockTime        m_timestamp{0};

		int64_t				      m_timeDurationKlv_nanoSec{0};
    GstClockTime        m_timestampKlv{0};

    GstClock *m_osClock {0};  

    GMainLoop *loop{nullptr};
    std::chrono::steady_clock::time_point lastImgTime{};
    std::chrono::steady_clock::time_point lastPcktTime{};
    uint64_t klvCnt{0};
    uint64_t imgCnt{0};
};

struct PcktBuffer
{
  PcktBuffer(const size_t sz, const size_t initVal=0)
    : length (sz)
  {
    buffer= new char[sz];
    //intialized as 'x's
    for(size_t i=0; i<sz; ++i)
      buffer[i]='x';

    //add the str of <initVal> at begining for dubug purpose
    std::string s = std::to_string(initVal);
    size_t L = (sz>s.length()) ? s.length() : sz;
    for (size_t i=0; i<L; i++){
      buffer[i] = s[i];
    }

  }


  ~PcktBuffer(){
    if(buffer){
      delete [] buffer;
    }
    length = 0;
  }


	char* buffer{nullptr};
	int length{0};
};


static void pushKlv(GstElement *src, guint, void *user_data);
static void pushImg(GstElement *src, guint size, void *user_data);

int test_gst_klv_enc_min_v1(int argc, char *argv[])
{


  GstElement *pipeline;
  GstElement *videoSrc, *srcCapsFilter, *dataSrc, *encoder264, *mpegtsmux, *parser, *vidCapsFilter, *videoConvert, *videoScale, *timeoverlay, *avsink, *video_queue, *filesink;
  GstCaps *src_filter_caps, *videoSrc_caps,*dataSrc_caps, *video_filter_caps, *time_filter_caps;
  GstStateChangeReturn ret;
  GstBus *bus;
  GstMessage *msg;
  gboolean terminate = FALSE;

  UserData userData; 
  AppCfg   cfg;

  // copy first argument to fileName  - this is the file we will write to
  std::string ouputFilePath("./tmp1.ts");
  std::cout << "the output video file will be at: " << ouputFilePath << std::endl;

  gst_init(&argc, &argv);


  userData.loop = g_main_loop_new(NULL, false);

#if USE_VIDEO_TEST_SRC
  videoSrc = gst_element_factory_make("videotestsrc", "videoSrc");
#else
  videoSrc =  gst_element_factory_make("appsrc", "videoSrc");
#endif

  srcCapsFilter = gst_element_factory_make("capsfilter", NULL);
  encoder264 = gst_element_factory_make( (const gchar *)cfg.encName.c_str(), NULL);
  mpegtsmux = gst_element_factory_make("mpegtsmux", NULL);
  video_queue = gst_element_factory_make("queue", "video_queue");
  parser = gst_element_factory_make("h264parse", NULL);
  vidCapsFilter = gst_element_factory_make("capsfilter", NULL);
  videoConvert = gst_element_factory_make("videoconvert", NULL);
  videoScale = gst_element_factory_make("videoscale", NULL);
  timeoverlay = gst_element_factory_make("timeoverlay", NULL);
  dataSrc = gst_element_factory_make("appsrc", NULL);
  avsink = gst_element_factory_make("autovideosink", "sink");
  filesink = gst_element_factory_make("filesink", "filesink");

  g_printerr("AAA--------\n");

  pipeline = gst_pipeline_new("encode-pipeline");

  if (!pipeline || !videoSrc || !srcCapsFilter || !dataSrc || !mpegtsmux || !parser 
                || !vidCapsFilter || !videoConvert || !videoScale || !timeoverlay 
                || !avsink || !video_queue || !filesink)
  {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }



  dataSrc_caps = gst_caps_new_simple("meta/x-klv", 
                                     "parsed", G_TYPE_BOOLEAN, TRUE, 
                                     "spare", G_TYPE_BOOLEAN, TRUE,
                                     "is-live", G_TYPE_BOOLEAN, TRUE, 
                                     "framerate", GST_TYPE_FRACTION, MY_KLV_FPS, 1,
                                     NULL);

  g_object_set(G_OBJECT(dataSrc), "caps", dataSrc_caps, NULL);
  g_object_set(G_OBJECT(dataSrc), "format", GST_FORMAT_TIME, NULL);
  g_object_set(G_OBJECT(dataSrc), "do-timestamp", TRUE, NULL);
  g_object_set(G_OBJECT(dataSrc), "max-latency", 1000000000, NULL);

#if USE_VIDEO_TEST_SRC
  g_object_set(videoSrc, "pattern", 0, NULL);
  videoSrc_caps = gst_caps_new_simple("video/x-raw", 
                                    "width", G_TYPE_INT, MY_VIDEO_W, 
                                    "height", G_TYPE_INT, MY_VIDEO_H, 
                                    "framerate", GST_TYPE_FRACTION, MY_VIDEO_FPS, 1, NULL);
#else
  g_object_set(videoSrc, "format", GST_FORMAT_TIME, NULL);
  g_object_set(videoSrc, "do-timestamp", TRUE, NULL);
  videoSrc_caps = gst_caps_new_simple("video/x-raw",
                                    "format", G_TYPE_STRING, "I420",
                                    "width", G_TYPE_INT, MY_VIDEO_W,
                                    "height", G_TYPE_INT, MY_VIDEO_H,
                                    "framerate", GST_TYPE_FRACTION, MY_VIDEO_FPS, 1,NULL);
#endif
  g_object_set(srcCapsFilter, "caps", videoSrc_caps, NULL);
 
  video_filter_caps = gst_caps_from_string("video/x-h264, stream-format=(string)byte-stream");
  g_object_set(vidCapsFilter, "caps", video_filter_caps, NULL);

  g_object_set(G_OBJECT(encoder264), "bitrate", 4000000, NULL);
  g_object_set(G_OBJECT(filesink), "location", ouputFilePath.c_str(), NULL);
  g_printerr("BBB--------\n");

  // Assign appsrc callbacks to push image and metadata
#if USE_VIDEO_TEST_SRC
  g_signal_connect(dataSrc, "need-data", G_CALLBACK(pushKlv), &userData);
#else
  g_signal_connect(videoSrc, "need-data", G_CALLBACK(pushImg), &userData);
  g_signal_connect(dataSrc, "need-data", G_CALLBACK(pushKlv), &userData);
#endif

  gst_bin_add_many(GST_BIN(pipeline), videoSrc, srcCapsFilter, videoConvert, videoScale, timeoverlay, encoder264, 
      vidCapsFilter, parser, mpegtsmux, video_queue, dataSrc, filesink, NULL);

  if (!gst_element_link_many(videoSrc, srcCapsFilter, timeoverlay, encoder264, vidCapsFilter, video_queue, mpegtsmux, NULL) ||
      !gst_element_link_many(dataSrc, mpegtsmux, NULL) ||
      !gst_element_link_many(mpegtsmux, filesink, NULL))
  {
    g_printerr("Elements could not be linked.\n");
    gst_object_unref(pipeline);
    return -1;
  }


  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);

  //---------------------------------------------------
  //swu: todo: remove g_main_loop_run(), 'casue  "CCC------" will never be reached unless quit the loop
  //     either use g_main_loop_run()
  //     or  
  //    gst_element_get_bus()
  //    gst_bus_timed_pop_filtered()
  //    use both is wired!
  //---------------------------------------------------
  g_main_loop_run(userData.loop);

  g_printerr("CCC--------\n");

  /* Listen to the bus */
  // These lines:
  //    gst_element_get_bus()
  //    gst_bus_timed_pop_filtered()
  // will wait until an error occurs or the end of the stream is found. gst_element_get_bus() 
  // retrieves the pipeline's bus, and gst_bus_timed_pop_filtered() will block until you receive either an 
  // ERROR or an EOS (End-Of-Stream) through that bus. Do not worry much about this line, the GStreamer bus 
  // is explained in Basic tutorial 2: GStreamer concepts
  bus = gst_element_get_bus(pipeline);
  do
  {
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
    g_printerr("DDD--------\n");

    /* Parse message */
    if (msg != NULL)
    {
      GError *err;
      gchar *debug_info;

      printf("GST_MESSAGE_TYPE(msg)=%d\n", GST_MESSAGE_TYPE(msg));
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

      case GST_MESSAGE_STATE_CHANGED:
        /* We are only interested in state-changed messages from the pipeline */
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline))
        {
          GstState old_state, new_state, pending_state;
          gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
          g_print("Pipeline state changed from %s to %s:\n",
                  gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
        }
        terminate = TRUE;
        break;
      default:
        /* We should not reach here */
        g_printerr("Unexpected message received.\n");
        terminate = TRUE;
        break;
      }
      gst_message_unref(msg);
    }
  } while (!terminate);

  /* Free resources */
  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);

  /* Clean up allocated resources */
  g_printerr("well done--FF, imgCnt=%lu, klvCnt=%lu\n", userData.imgCnt, userData.klvCnt);

  return 0;
}

/* Callback function for encoding and injection of Klv metadata */
static void pushKlv(GstElement *appsrc, guint, gpointer user_data)
{
  GstFlowReturn ret;
  GstBuffer *buffer;
  bool fInsert = false;

  UserData *pUserData = reinterpret_cast<UserData*>(user_data);

#if USE_VIDEO_TEST_SRC
  // Wait for a frame duration interval since the last inserted packet. As we're inserting ASYNC KLV, it is good enough for the demo...
  while (!fInsert)
  {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    int64_t diff = std::chrono::duration_cast<std::chrono::nanoseconds>(now - pUserData->lastPcktTime).count();
    if (diff >= pUserData->m_timeDurationKlv_nanoSec){
      //printf("KLV: diff=%lu, This->m_timeDurationKlv_nanoSec)=%lu\n", diff, pUserData->m_timeDurationKlv_nanoSec);
      fInsert = true;
    }
    else{
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }
#endif

  PcktBuffer pcktBuf(16, pUserData->klvCnt);
  buffer = gst_buffer_new_allocate(NULL, pcktBuf.length, NULL);

 
 //GST_BUFFER_PTS(buf)
 //Gets the presentation timestamp (pts) in nanoseconds (as a GstClockTime) of the data in the buffer. 
 //This is the timestamp when the media should be presented to the user. 

 //GST_BUFFER_DTS(buf)
 //Gets the decoding timestamp (dts) in nanoseconds (as a GstClockTime) of the data in the buffer. 
 //This is the timestamp when the media should be decoded or processed otherwise. 
#if APP_ASYNC_KLV
  // For ASYNC_KLV, we need to remove timestamp and duration from the buffer
  GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;
#else
  // For SYNC_KLV, we need to add timestamp and duration from the buffer
  GST_BUFFER_PTS(buffer) = pUserData->m_timestampKlv; 
  GST_BUFFER_DTS(buffer) = pUserData->m_timestampKlv;
  GST_BUFFER_DURATION(buffer) = pUserData->m_timeDurationKlv_nanoSec; 
	pUserData->m_timestampKlv += GST_BUFFER_DURATION(buffer);
#endif

  // Fill the buffer with the encoded KLV data
  gst_buffer_fill(buffer, 0, pcktBuf.buffer, pcktBuf.length);

  ret = gst_app_src_push_buffer((GstAppSrc *)appsrc, buffer);
//	g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);

  if (ret != GST_FLOW_OK)
  {
    g_printerr("Flow error");
    g_main_loop_quit(pUserData->loop);
  }

  if( 0==pUserData->klvCnt%50){
    g_print("Klv packet count: %llu.  Buf size: %d \n", pUserData->klvCnt, pcktBuf.length);
  }

  //prepare for next callback
  pUserData->lastPcktTime = std::chrono::steady_clock::now();
  pUserData->klvCnt++;

#if USE_VIDEO_TEST_SRC
  if (pUserData->klvCnt>=500){
    g_main_loop_quit(pUserData->loop);
  }
#endif

}

void pushImg(GstElement *appsrc, guint unused, gpointer user_data)
{
	GstFlowReturn ret;
	
  UserData *pUserData = reinterpret_cast<UserData*>(user_data);

	if (pUserData->m_requestToClose.load()) {
		//from: https://stackoverflow.com/questions/48073759/appsrc-is-stuck-in-preroll-even-when-sending-eos
		//when you want to stop perform following steps : 
		//[1] stop pushing the data to appsrc
		//[2] send eos to oggmux(where you should get eos on bus)   ----wus1: we do not have this
		//[3] send eos to appsrc 
		//The above will make sure everything is closed.� Prabhakar Lad Jan 3 '18 at 10:55
		//When the last byte is pushed into appsrc, you must call gst_app_src_end_of_stream() to make it send an EOS downstream.
		printf("GstRtspStreamer::cb_need_data() -- close stream\n");
		if (pUserData->m_eosRequestCnt < 1) {
			gst_app_src_end_of_stream(GST_APP_SRC(appsrc));
		}
		pUserData->m_eosRequestCnt += 1;
		printf("GstRtspStreamer::cb_need_data(): m_eosRequestCnt=%d", pUserData->m_eosRequestCnt);
		return;
	}

  //get sample image from userData
	app::HostYuvFrmPtr &oriFrm = pUserData->m_hostYuvFrm;

  //allocate a new buf for current frm
	uint8_t* yuvImgBuf = (uint8_t*)malloc(oriFrm->sz_);
	if (yuvImgBuf == NULL) {
    printf("pushImg(): cannot alocated memory!\n");
		assert(false);
	}
  //wrapper <yuvImgBuf> into <curFrm> which type is <app::HostYuvFrm>
  app::HostYuvFrm curFrm( oriFrm->w_, oriFrm->h_, yuvImgBuf, oriFrm->sz_, 0);
  oriFrm->hdCopyTo( &curFrm );
  curFrm.fn_ =  pUserData->imgCnt;
	curFrm.wrtFrmNumOnImg();  //change the data inside <yuvImgBuf>

	GstBuffer *buffer = gst_buffer_new_wrapped((guchar *)yuvImgBuf, gsize(oriFrm->sz_));
  if( 0 == curFrm.fn_%100 ){
    curFrm.dump( "./", "input");
  }
	/* increment the timestamp every 1/MY_FPS second */
	GST_BUFFER_PTS(buffer) = pUserData->m_timestamp;
	GST_BUFFER_DTS(buffer) = pUserData->m_timestamp;
	GST_BUFFER_DURATION(buffer) = pUserData->m_timeDuration_nanoSec;
	pUserData->m_timestamp += GST_BUFFER_DURATION(buffer);

	g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);
	if (ret != GST_FLOW_OK) {
		printf("pushImg(): sth is wrong!  ret=%d, name=%s", (int)ret, gst_flow_get_name(ret));
	}
  
  if( 0==pUserData->imgCnt%50){
    g_print("imgCnt=%lu\n", pUserData->imgCnt);
  }

  //prepare for next callback
  pUserData->imgCnt++;
  pUserData->lastImgTime = std::chrono::steady_clock::now();
  if (pUserData->imgCnt>=500){
    g_main_loop_quit(pUserData->loop);
  }

  //wus1: do not free <yuvImgBuf> at here, it will be freeed inside gst via <buffer>
}
