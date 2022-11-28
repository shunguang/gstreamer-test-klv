
#todo: remove not used libs and INC folders

PROJ_NAME=test

APP_ROOT=/home/nvidia/pkg/gstreamer-test-klv
SDIR_ROOT=$(APP_ROOT)/swu-modified
SDIR_PROJ=$(APP_ROOT)/swu-modified

ODIR_ROOT=$(APP_ROOT)/build
ODIR_OBJ=$(ODIR_ROOT)/$(PROJ_NAME)
ODIR_LIB=$(ODIR_ROOT)/libs
ODIR_BIN=$(ODIR_ROOT)/bin

#include and lib paths of the platform
PLTF_INC=/usr/include
PLTF_LIB=/usr/lib
BOOST_INC=/usr/include
BOOST_LIB=/usr/lib

CV_INC=/usr/local/include/opencv4
CV_LIB=/usr/local/lib

JETSON_INFER_INC=/usr/local/include/jetson-inference
JETSON_UTIL_INC=/usr/local/include/jetson-utils
JETSON_LIB=/usr/local/lib

I_GST_INC=-I/usr/include/gstreamer-1.0 -I/usr/include/glib-2.0 -I/usr/lib/aarch64-linux-gnu/glib-2.0/include
GST_LIB=/usr/lib/aarch64-linux-gnu/gstreamer-1.0

CC = /usr/bin/gcc
CXX = /usr/bin/g++

#DEBUG = -g
DEBUG = -DNDEBUG -g
#DEBUG = -DDEBUG -g


$(info $$SDIR_PROJ is [${SDIR_PROJ}])
$(info $$ODIR_ROOT is [${ODIR_ROOT}])
$(info $$ODIR_OBJ is [${ODIR_OBJ}])
$(info $$ODIR_LIB is [${ODIR_LIB}])


#the target binary name
TARGETFILE=$(ODIR_BIN)/test.out

#include flags
CFLAGS  := -Wall -c $(DEBUG) -DqDNGDebug=1 -D__xlC__=1 -DNO_FCGI_DEFINES=1 -DqDNGUseStdInt=0 -DUNIX_ENV=1 -D__LITTLE_ENDIAN__=1 -DqMacOS=0 -DqWinOS=0 -std=gnu++11 \
        -I$(SDIR_PROJ) -I$(SDIR_ROOT) -I$(CUDA_INC) $(I_GST_INC) -I$(JETSON_UTIL_INC) -I$(CV_INC) -I$(BOOST_INC) -I$(PLTF_INC)

#link flags and lib searching paths
LFLAGS  := -Wall $(DEBUG) -L$(CV_LIB) -L$(ODIR_LIB) -L$(GST_LIB) -L$(BOOST_LIB) -L$(CUDA_LIB) -L/usr/lib/aarch64-linux-gnu/tegra -L/usr/lib/aarch64-linux-gnu -L$(PLTF_LIB)

#-L/usr/lib/aarch64-linux-gnu/tegra

# link libs
LIBS    := \
        -lboost_timer -lboost_filesystem -lboost_system -lboost_date_time -lboost_regex \
        -lboost_chrono -lboost_thread -lncurses -pthread \
        -lopencv_ml -lopencv_shape -lopencv_video -lopencv_calib3d -lopencv_features2d \
        -lopencv_highgui -lopencv_videoio -lopencv_flann -lopencv_imgcodecs -lopencv_imgproc -lopencv_core \
        -ljetson-utils \
        -lgthread-2.0 -lgstbase-1.0 -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -lgstapp-1.0 -lglib-2.0 -lpng -lz -lv4l2 \
        -lv4l2 -lEGL -lGLESv2 -lX11 -lnvbuf_utils -lnvjpeg \
        -lavcodec -lavformat -lavutil -lswresample -lswscale -llzma -ldl -lm -lpthread -lrt

#        -lopencv_stitching -lopencv_superres -lopencv_videostab \
#       -lavcodec -lavformat -lavutil -lswresample -lswscale -llzma -ldl -lm -lpthread -lrt
#       -lopencv_cudaarithm -lopencv_cudaimgproc -lopencv_cudafeatures2d -lopencv_cudawarping \
#        -lnppc_static -lnppif_static -lnppig_static -lnppial_static -lnppicc_static -lnppisu_static -lnppidei -lculibos -lcublas_static -lcudart_static \

OBJS = \
        $(ODIR_OBJ)/HostYuvFrm.o \
        $(ODIR_OBJ)/test_gst_enc_H264_wo_klv.o \
        $(ODIR_OBJ)/test_gst_klv_enc_min_v1.o \
        $(ODIR_OBJ)/test_gst_klv_dec_v1.o \
        $(ODIR_OBJ)/main.o

default:  directories $(TARGETFILE)

directories:
	mkdir -p $(ODIR_OBJ)
	mkdir -p $(ODIR_LIB)
	mkdir -p $(ODIR_BIN)

#the output binary file name is <$(TARGETFILE)>
$(TARGETFILE)   :       $(OBJS)
	$(CXX) $(LFLAGS) $(OBJS) $(LIBS) $(LIBS) -o $(TARGETFILE)


$(ODIR_OBJ)/main.o      :       $(SDIR_PROJ)/main.cpp
	$(CXX) -o $(ODIR_OBJ)/main.o $(CFLAGS) $(SDIR_PROJ)/main.cpp


$(ODIR_OBJ)/HostYuvFrm.o       :       $(SDIR_PROJ)/HostYuvFrm.cpp
	$(CXX) -o $(ODIR_OBJ)/HostYuvFrm.o $(CFLAGS) $(SDIR_PROJ)/HostYuvFrm.cpp

$(ODIR_OBJ)/test_gst_klv_enc_min_v1.o       :       $(SDIR_PROJ)/test_gst_klv_enc_min_v1.cpp
	$(CXX) -o $(ODIR_OBJ)/test_gst_klv_enc_min_v1.o $(CFLAGS) $(SDIR_PROJ)/test_gst_klv_enc_min_v1.cpp

$(ODIR_OBJ)/test_gst_klv_dec_v1.o       :       $(SDIR_PROJ)/test_gst_klv_dec_v1.cpp
	$(CXX) -o $(ODIR_OBJ)/test_gst_klv_dec_v1.o $(CFLAGS) $(SDIR_PROJ)/test_gst_klv_dec_v1.cpp

$(ODIR_OBJ)/test_gst_enc_H264_wo_klv.o :       $(SDIR_PROJ)/test_gst_enc_H264_wo_klv.cpp
	$(CXX) -o $(ODIR_OBJ)/test_gst_enc_H264_wo_klv.o $(CFLAGS) $(SDIR_PROJ)/test_gst_enc_H264_wo_klv.cpp

clean:
	\rm $(ODIR_OBJ)/*.o $(TARGETFILE)

rm_wami:
	\rm $(TARGETFILE)
