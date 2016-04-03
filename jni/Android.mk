LOCAL_PATH := $(call my-dir)

include $(call all-makefiles-under, $(LOCAL_PATH))


MY_LIBS_PATH := ../ffmpeg/$(BUILD_TYPE)/$(PALTFORM_DIR)/lib
MY_CONFIG_INC_PATH := ../ffmpeg/$(BUILD_TYPE)/$(PALTFORM_DIR)
MY_LIBS_INC_PATH := ../ffmpeg/$(BUILD_TYPE)/$(PALTFORM_DIR)/include

include $(CLEAR_VARS)
LOCAL_MODULE := libavcodec
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/libavcodec.a
include $(PREBUILT_STATIC_LIBRARY)

#include $(CLEAR_VARS)
#LOCAL_MODULE := libavfilter
#LOCAL_SRC_FILES := \
#    $(MY_LIBS_PATH)/libavfilter.a
#include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libavformat
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/libavformat.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libavutil
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/libavutil.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libswresample
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/libswresample.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libswscale
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/libswscale.a
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE := libsplayer
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES :=      audio_device_opensles.cc \
                                        video_render_opengles20.cc \
                                        video_render_android.cc \
                                        player_helper.cc \
					player_file.cc \
					player_log.cc \
                                        player.cc \
					player_hard.cc \
					player_soft.cc \
                                        player_jni_entry.cc


ifeq ($(PALTFORM_DIR),neon)    
LOCAL_CFLAGS := \
    '-D__STDC_CONSTANT_MACROS' \
    '-D__STDC_LIMIT_MACROS' \
    '-O3' \
    '-mfloat-abi=softfp' \
    '-mfpu=neon' \
    '-march=armv7-a' \
    '-mtune=cortex-a8'
else
LOCAL_CFLAGS := \
    '-D__STDC_CONSTANT_MACROS' \
    '-D__STDC_LIMIT_MACROS' \
    '-O3' 
endif

#LOCAL_ARM_MODE := arm

ifeq ($(PALTFORM_DIR),neon)  
LOCAL_ARM_NEON := true
endif

#'-g' \
#'-ggdb'

LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/../ffmpeg/$(BUILD_TYPE)/$(PALTFORM_DIR)/ \
        $(LOCAL_PATH)/../ffmpeg/$(BUILD_TYPE)/$(PALTFORM_DIR)/include
		
#add new
#LOCAL_LDFLAGS += -L$(LOCAL_PATH)/../$(BUILD_TYPE)/lib/
#end

LOCAL_LDLIBS := \
    -llog \
    -lGLESv2 \
    -lOpenSLES \
    -lm \
    -lz 

#    -lmedia \
#    -lutils \
#    -lbinder 
	
#    -lavutil \
#    -lavcodec \
#    -lavfilter \
#    -lswresample \
#    -lswscale

LOCAL_STATIC_LIBRARIES := \
        libavcodec \
        libavformat \
        libswresample \
        libswscale \
        libavutil

# libavfilter \		
include $(BUILD_SHARED_LIBRARY)
