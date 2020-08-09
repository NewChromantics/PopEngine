LOCAL_PATH := $(call my-dir)
$(warning LOCAL_PATH=$(LOCAL_PATH))	#	debug


# extra ../ as jni is always prepended
SRC_PATH := ../../src
$(warning SRC_PATH=$(SRC_PATH))	#	debug

# gr: get this from env var
APP_MODULE := $(BUILD_TARGET_NAME)

# full speed arm instead of thumb
LOCAL_ARM_MODE  := arm

#include cflags.mk

# This file is included in all .mk files to ensure their compilation flags are in sync
# across debug and release builds.

# NOTE: this is not part of import_vrlib.mk because VRLib itself needs to have these flags
# set, but VRLib's make file cannot include import_vrlib.mk or it would be importing itself.

LOCAL_CFLAGS	:= -DANDROID_NDK
LOCAL_CFLAGS	+= -Werror			# error on warnings
LOCAL_CFLAGS	+= -Wall
LOCAL_CFLAGS	+= -Wextra
#LOCAL_CFLAGS	+= -Wlogical-op		# not part of -Wall or -Wextra
#LOCAL_CFLAGS	+= -Weffc++			# too many issues to fix for now
LOCAL_CFLAGS	+= -Wno-strict-aliasing		# TODO: need to rewrite some code
LOCAL_CFLAGS	+= -Wno-unused-parameter
LOCAL_CFLAGS	+= -Wno-missing-field-initializers	# warns on this: SwipeAction	ret = {}
LOCAL_CFLAGS	+= -Wno-multichar	# used in internal Android headers:  DISPLAY_EVENT_VSYNC = 'vsyn',
LOCAL_CFLAGS	+= -Wno-invalid-source-encoding
#LOCAL_CFLAGS	+= -pg -DNDK_PROFILE # compile with profiling
#LOCAL_CFLAGS	+= -mfpu=neon		# ARM NEON support
LOCAL_CPPFLAGS	:= -Wno-type-limits
LOCAL_CPPFLAGS	+= -Wno-invalid-offsetof

LOCAL_CFLAGS     := -Werror -DANDROID_NDK
LOCAL_CFLAGS	 += -Wno-multichar	# used in internal Android headers:  DISPLAY_EVENT_VSYNC = 'vsyn',

LOCAL_CFLAGS     := -Werror -DTARGET_ANDROID

#ifeq ($(OVR_DEBUG),1)
#LOCAL_CFLAGS	+= -DOVR_BUILD_DEBUG=1 -O0 -g
#else
LOCAL_CFLAGS	+= -O3
#endif

SOY_PATH = $(SRC_PATH)/SoyLib



# Build dependency modules
#--------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_MODULE := jsc
LOCAL_SRC_FILES := $(SRC_PATH)/Android/jscore/libs/$(TARGET_ARCH_ABI)/libjsc.so
LOCAL_EXPORT_C_INCLUDES := $(SRC_PATH)/Android/jscore
include $(PREBUILT_SHARED_LIBRARY)

# tsdk: need to build these libs first... remember to uncomment the shared libraries as well
# include $(CLEAR_VARS)
# LOCAL_MODULE := popH264
# LOCAL_SRC_FILES := $(PROJECT_PATH)/Libs/poph264/$(GITHUB_LIB_PATH)/$(TARGET_ARCH_ABI)/libPopH264.so
# LOCAL_EXPORT_C_INCLUDES := $(PROJECT_PATH)/Libs/poph264/$(GITHUB_LIB_PATH)
# include $(PREBUILT_SHARED_LIBRARY)

# include $(CLEAR_VARS)
# LOCAL_MODULE := popcameradevice
# LOCAL_SRC_FILES := $(PROJECT_PATH)/Libs/poph264/$(GITHUB_LIB_PATH)/$(TARGET_ARCH_ABI)/libPopCameraDevice.so
# LOCAL_EXPORT_C_INCLUDES := $(PROJECT_PATH)/Libs/poph264/$(GITHUB_LIB_PATH)
# include $(PREBUILT_SHARED_LIBRARY)


# now build main module
#--------------------------------------------------------
include $(CLEAR_VARS)
LOCAL_MODULE := $(APP_MODULE)
LOCAL_ARM_MODE  := arm

# src/SoyLib/src works, but try and use paths
LOCAL_C_INCLUDES += \
$(LOCAL_PATH)/$(SOY_PATH)/src \
$(LOCAL_PATH)/$(SOY_PATH)/src/magic_enum/include \
$(LOCAL_PATH)/$(SRC_PATH)/Json11	\
$(LOCAL_PATH)/$(SRC_PATH)/Android/jscore	\
$(LOCAL_PATH)/$(SRC_PATH)/Libs	\
$(LOCAL_PATH)/Libs/poph264/$(ANDROID_ABI)	\
$(LOCAL_PATH)/Libs/popcameradevice/$(ANDROID_ABI)	\



LOCAL_STATIC_LIBRARIES :=
#LOCAL_STATIC_LIBRARIES += android-ndk-profiler

#LOCAL_LDLIBS	+= -lGLESv3			# OpenGL ES 3.0
#LOCAL_LDLIBS	+= -lEGL			# GL platform interface
LOCAL_LDLIBS  	+= -llog			# logging
#LOCAL_LDLIBS  	+= -landroid		# native windows
#LOCAL_LDLIBS	+= -lz				# For minizip
#LOCAL_LDLIBS	+= -lOpenSLES		# audio

# project files
# todo: generate from input from xcode
LOCAL_SRC_FILES  := \
$(SRC_PATH)/JsCoreBind.cpp \
$(SRC_PATH)/PopMain.cpp \
$(SRC_PATH)/PopEngine.cpp \
$(SRC_PATH)/TBind.cpp \
$(SRC_PATH)/TApiCommon.cpp \
$(SRC_PATH)/TApiHttp.cpp \
$(SRC_PATH)/TApiPanopoly.cpp \
$(SRC_PATH)/TApiSocket.cpp \
$(SRC_PATH)/TApiWebsocket.cpp \
$(SRC_PATH)/Json11/json11.cpp \

# disabled until PopH264 & PopCameraDevice are working
#$(SRC_PATH)/TApiMedia.cpp \


$(warning LOCAL_SRC_FILES=$(LOCAL_SRC_FILES))	#	debug


# soy lib files
LOCAL_SRC_FILES  += \
$(SOY_PATH)/src/smallsha1/sha1.cpp \
$(SOY_PATH)/src/memheap.cpp \
$(SOY_PATH)/src/SoyApp.cpp \
$(SOY_PATH)/src/SoyArray.cpp \
$(SOY_PATH)/src/SoyAssert.cpp \
$(SOY_PATH)/src/SoyBase64.cpp \
$(SOY_PATH)/src/SoyDebug.cpp \
$(SOY_PATH)/src/SoyFilesystem.cpp \
$(SOY_PATH)/src/SoyFourcc.cpp \
$(SOY_PATH)/src/SoyHttp.cpp \
$(SOY_PATH)/src/SoyImage.cpp \
$(SOY_PATH)/src/SoyMediaFormat.cpp \
$(SOY_PATH)/src/SoyPixels.cpp \
$(SOY_PATH)/src/SoyPng.cpp \
$(SOY_PATH)/src/SoyProtocol.cpp \
$(SOY_PATH)/src/SoyRef.cpp \
$(SOY_PATH)/src/SoyShellExecute.cpp \
$(SOY_PATH)/src/SoySocket.cpp \
$(SOY_PATH)/src/SoySocketStream.cpp \
$(SOY_PATH)/src/SoyStream.cpp \
$(SOY_PATH)/src/SoyString.cpp \
$(SOY_PATH)/src/SoyThread.cpp \
$(SOY_PATH)/src/SoyTime.cpp \
$(SOY_PATH)/src/SoyTypes.cpp \
$(SOY_PATH)/src/SoyWebSocket.cpp \
$(SOY_PATH)/src/SoyWindow.cpp \


#$(SOY_PATH)/src/SoyOpengl.cpp \
#$(SOY_PATH)/src/SoyOpenglContext.cpp \
#$(SOY_PATH)/src/SoyThread.cpp \
#$(SOY_PATH)/src/SoyEvent.cpp \
#$(SOY_PATH)/src/SoyShader.cpp \
#$(SOY_PATH)/src/SoyUnity.cpp \
#$(SOY_PATH)/src/SoyBase64.cpp \
#$(SOY_PATH)/src/SoyGraphics.cpp \
#$(SOY_PATH)/src/SoyJava.cpp \
#$(SOY_PATH)/src/SoyStream.cpp \



LOCAL_SHARED_LIBRARIES := \
jsc \
# popcameradevice \
# popH264 \

include $(BUILD_SHARED_LIBRARY)



#$(call import-module,android-ndk-profiler)
