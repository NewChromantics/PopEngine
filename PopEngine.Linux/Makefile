LIB_NAME = PopEngine
APP_NAME = PopEngineTestApp


# X ?= if X env var isn't set, use this default
compiler ?= g++
CC = ${compiler}

# gr: should always prefix with Linux... tidy up these names!
# 	then add architecture or specific build (nvidia jetson, pi4)
osTarget ?= Linux_Default

# TargetArch is the name of the architectures we use in modules (poph264, popcameradevice)
TargetArch = $(osTarget)

# ../ causes problems, but we also can't execute make in the root dir :/
# so expand it, so we end up putting temp stuff in build/temp/home/graham/code/popengine/src/...
PROJECT_PATH = $(realpath ..)/

# gr: we could setup targets this way,(like templates) or only via make (ENABLE_OPENGL=X etc)
# todo: handle osmesa flag here in a target
ifeq ($(osTarget), Nvidia)
ENABLE_OPENGL=true
ENABLE_MEDIA=true
endif

ifeq ($(osTarget), Pi)
ENABLE_OPENGL=true
ENABLE_MEDIA=true
endif

CONFIGURATION ?= Release

BUILD_DIR = $(PROJECT_PATH)Build/$(osTarget)_$(CONFIGURATION)
BUILD_TEMP_DIR = $(PROJECT_PATH)Build/Temp/$(osTarget)_$(CONFIGURATION)/

SRC_PATH = $(PROJECT_PATH)/src
SOY_PATH = $(PROJECT_PATH)/src/SoyLib


# variables for PopH264 & PopCameraDevice so we can use locally built versions
LIB_PACKAGE_MODULES_PATH = $(PROJECT_PATH)/PopEngine.Package/node_modules/@newchromantics
POPH264_PATH ?= $(LIB_PACKAGE_MODULES_PATH)/com.newchromantics.poph264/Nvidia_arm64/
POPCAMERADEVICE_PATH ?= $(LIB_PACKAGE_MODULES_PATH)/popcameradevice/$(TargetArch)

POPCAMERADEVICE_PATH = /home/graham/Code/PopCameraDevice/Build/Nvidia_Release
POPH264_PATH = /home/graham/Code/PopH264/Build

$(info POPH264_PATH=$(POPH264_PATH))

LIB_INCLUDES = \
-I$(SOY_PATH)/src	\
-I$(SOY_PATH)/src/magic_enum/include	\
-I$(SOY_PATH)/src	\
-I$(SRC_PATH)/Json11	\
-I/usr/include/webkitgtk-4.0	\

LIB_CPP_FILES  = \
$(SRC_PATH)/Json11/json11.cpp \
$(SRC_PATH)/JsCoreBind.cpp \
$(SRC_PATH)/JavascriptConvertImports.cpp \
$(SRC_PATH)/PopMain.cpp \
$(SRC_PATH)/PopEngine.cpp \
$(SRC_PATH)/TBind.cpp \
$(SRC_PATH)/TApiCommon.cpp \
$(SRC_PATH)/TApiHttp.cpp \
$(SRC_PATH)/TApiSocket.cpp \
$(SRC_PATH)/TApiWebsocket.cpp \
$(SRC_PATH)/TApiZip.cpp \
$(SRC_PATH)/TApiGui.cpp	\
$(SRC_PATH)/SoyImageProxy.cpp	\
#$(SRC_PATH)/TApiEngine.cpp \

LIB_CPP_FILES += $(SRC_PATH)/SoyGuiLinux.cpp

LIB_C_FILES = \
$(SRC_PATH)/Zip/src/zip.c



# soy lib files
LIB_CPP_FILES  += \
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
$(SOY_PATH)/src/SoyHttpConnection.cpp \
$(SOY_PATH)/src/SoyImage.cpp \
$(SOY_PATH)/src/SoyMediaFormat.cpp \
$(SOY_PATH)/src/SoyPixels.cpp \
$(SOY_PATH)/src/SoyPlatform.cpp \
$(SOY_PATH)/src/SoyPng.cpp \
$(SOY_PATH)/src/SoyProtocol.cpp \
$(SOY_PATH)/src/SoyRef.cpp \
$(SOY_PATH)/src/SoyShellExecute.cpp \
$(SOY_PATH)/src/SoySocket.cpp \
$(SOY_PATH)/src/SoySocketStream.cpp \
$(SOY_PATH)/src/SoyStream.cpp \
$(SOY_PATH)/src/SoyStreamBuffer.cpp \
$(SOY_PATH)/src/SoyString.cpp \
$(SOY_PATH)/src/SoyThread.cpp \
$(SOY_PATH)/src/SoyTime.cpp \
$(SOY_PATH)/src/SoyTypes.cpp \
$(SOY_PATH)/src/SoyWebSocket.cpp \
$(SOY_PATH)/src/SoyWindow.cpp \




APP_CPP_FILES =	\
$(PROJECT_PATH)/Source_App/PopEngineUnitTestMain.cpp	\

LIB_DEFINES = \
-DTARGET_LINUX=$(osTarget)	\
-DJSAPI_JSCORE	\


ifeq ($(CONFIGURATION),Release)
$(info Building Release)
CONFIGURATION_FLAGS = -O3
else
$(info Building Debug)
CONFIGURATION_FLAGS = -g
endif


# -g stops symbol stripping
# -f[lag] PIC position independent code (for dyanmically loaded code)
# -rpath $ORIGIN makes the shared object runtimepath be same place as executable
CFLAGS = -c $(CONFIGURATION_FLAGS) $(LIB_DEFINES) $(LIB_INCLUDES) -std=gnu++17 -fPIC

# Make a depdencny file (.d) so header changes can be detected
CFLAGS += -MD

LDFLAGS = -pthread
LDFLAGS += '-Wl,-rpath,$$ORIGIN'
LDFLAGS += '-Wl,-rpath-link=$(BUILD_DIR)'
# Don't let symbols get overriden on load	https://stackoverflow.com/questions/6538501/linking-two-shared-libraries-with-some-of-the-same-symbols
LDFLAGS += '-Wl,-Bsymbolic'

LIB_LINK_FLAGS = $(LDFLAGS) -shared
APP_LINK_FLAGS = $(LDFLAGS)

# tsdk: the paths have to be lower case as that is how node imports them
LIB_LINK_LIBS = \
-ljavascriptcoregtk-4.0	\
-L/usr/lib/aarch64-linux-gnu	\
-lm 	# math

# These files (without directory structure) get copied to build dir
OUT_FILE_SOURCES = $(SRC_PATH)/PopEngine.h


# gr: should x11 support be optional?
#LIB_DEFINES += -DENABLE_X11
LIB_LINK_LIBS += -lX11


ifdef ENABLE_OPENGL
# sudo apt-get install libgles2-mesa-dev
$(info Building with OPENGL)
LIB_INCLUDES += -I$(SRC_PATH)/sokol
LIB_INCLUDES += -I$(SRC_PATH)/LinuxDRM
LIB_INCLUDES += -I/usr/include/libdrm
LIB_DEFINES += -DENABLE_OPENGL
LIB_DEFINES += -DENABLE_APISOKOL
LIB_C_FILES += $(SRC_PATH)/LinuxDRM/common.c
LIB_C_FILES += $(SRC_PATH)/LinuxDRM/drm-common.c
LIB_C_FILES += $(SRC_PATH)/LinuxDRM/drm-legacy.c
LIB_CPP_FILES += $(SRC_PATH)/TApiSokol.cpp
LIB_CPP_FILES += $(SRC_PATH)/SoySokol_Linux.cpp
LIB_CPP_FILES += $(SRC_PATH)/LinuxDRM/esUtil.cpp
LIB_CPP_FILES += $(SRC_PATH)/EglContext.cpp
#gr: shouldnt need these as everything goes through sokol
#gr: need PBO
LIB_CPP_FILES += $(SOY_PATH)/src/SoyOpenglPbo.cpp
#LIB_CPP_FILES += $(SOY_PATH)/src/SoyOpengl.cpp
#LIB_CPP_FILES += $(SOY_PATH)/src/SoyOpenglContext.cpp
LIB_LINK_LIBS += -lEGL
#LIB_LINK_LIBS += -ldrm
LIB_LINK_LIBS += -l:libdrm.so.2
#LIB_LINK_LIBS += -ludev # user devices

# sudo apt-get install libgbm-dev
# graphics buffer for mesa, so we can use graphics buffers (GBM) with opengl without X
# this is at least true for pi, testing jetson
LIB_LINK_LIBS += -lgbm 

# flag is an optional parameter set in the Github Action
ifeq ($(flag),osmesa)
$(info Building for Software Driver)
LDFLAGS += '-Wl,-rpath-link=$(SRC_PATH)/Libs/osmesa'
LIB_LINK_LIBS += -L$(SRC_PATH)/Libs/osmesa
LIB_LINK_LIBS += -lOSMesa
else
$(info Building for GPU)
#LIB_LINK_LIBS += -lGLESv2
LIB_LINK_LIBS += -l:libGLESv2.so.2	#	nvidia demos using this

endif
endif

ifdef ENABLE_MEDIA
$(info Building with MEDIA)
#LIB_DEFINES += -DENABLE_APIMEDIA # this currently gets enabled in code if POPH264 or POPCAMERADEVICE is enabled
LIB_CPP_FILES += $(SRC_PATH)/TApiMedia.cpp

LIB_DEFINES += -DENABLE_POPH264
LIB_INCLUDES += -I$(POPH264_PATH)
LIB_LINK_LIBS += -L$(POPH264_PATH) -lPopH264
OUT_FILE_SOURCES += $(POPH264_PATH)/libPopH264.so

LIB_DEFINES += -DENABLE_POPCAMERADEVICE
LIB_INCLUDES += -I$(POPCAMERADEVICE_PATH)
LIB_LINK_LIBS += -L$(POPCAMERADEVICE_PATH) -lPopCameraDevice
OUT_FILE_SOURCES += $(POPCAMERADEVICE_PATH)/libPopCameraDevice.so
endif


# allow multichar constants (eg. int = 'RGBA')
CFLAGS += -Wno-multichar


# macro to turn input files into output files
# add Prefix BUILD_TEMP_DIR <- add suffix .o <- remove ../ <-- get filename without extension
InputFilesToOutputFiles = $(addprefix $(BUILD_TEMP_DIR),$(addsuffix .o,$(basename $(1))))

APP_OBJECTS = $(call InputFilesToOutputFiles,$(APP_CPP_FILES))
LIB_OBJECTS = $(call InputFilesToOutputFiles,$(LIB_CPP_FILES))
LIB_OBJECTS += $(call InputFilesToOutputFiles,$(LIB_C_FILES))
#$(info LIB_OBJECTS=$(LIB_OBJECTS))
#$(info APP_OBJECTS=$(APP_OBJECTS))

OUT_LIB=$(BUILD_DIR)/lib${LIB_NAME}.so
OUT_APP=$(BUILD_DIR)/${APP_NAME}



# convert to Build/xxx target files
OUT_FILES = $(addprefix ${BUILD_DIR}/,$(notdir ${OUT_FILE_SOURCES}))
#$(info out files...)
#$(info ${OUT_FILES})

GithubWorkflow: $(OUT_APP) $(OUT_LIB)
	$(info ::set-output name=executable:$(OUT_APP))
	$(info ::set-output name=library:$(OUT_LIB))
.PHONY: GithubWorkflow

exec: $(OUT_APP)
.PHONY: exec

lib: $(OUT_LIB) 
.PHONY: lib

# Copy other output files
$(OUT_FILES): $(OUT_FILE_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(info Copying output files $(OUT_FILES))
	cp $(OUT_FILE_SOURCES) $(BUILD_DIR)

$(OUT_APP): $(APP_OBJECTS) $(OUT_LIB) 
	$(info Building test app $(OUT_APP))
	mkdir -p $(BUILD_DIR)
	$(CC) $(APP_LINK_FLAGS) $(APP_OBJECTS) -o $@ -L$(BUILD_DIR) -l$(LIB_NAME)

$(OUT_LIB): $(LIB_OBJECTS) $(OUT_FILES)
	$(info Building library $(OUT_LIB))
	$(CC) $(LIB_LINK_FLAGS) -o $@ $(LIB_OBJECTS) $(LIB_LINK_LIBS)
	$(info Listing dependencies of $(OUT_LIB))
	ldd $(OUT_LIB)

$(BUILD_TEMP_DIR)%.o: %.c
	mkdir -p $(dir $@)
	$(CC) -x c $(CFLAGS) $(APP_INCLUDES) $(LIB_INCLUDES) $< -o $@

$(BUILD_TEMP_DIR)%.o: %.cpp
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(APP_INCLUDES) $(LIB_INCLUDES) $< -o $@


clean:
	# clean build files, anything .o
	$(info Cleaning files...)
	find $(PROJECT_PATH) -name "*.o" -type f -delete
	find $(PROJECT_PATH) -name "*.d" -type f -delete

	rm -f $(APP_OBJECTS)
	rm -f $(LIB_OBJECTS)
	rm -f $(OUT_LIB)
	rm -f $(OUT_APP)

# for every .o file, include a .d file, which has makefile rules to detect headers
-include $(LIB_OBJECTS:.o=.d)
-include $(APP_OBJECTS:.o=.d)
