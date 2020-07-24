LIB_NAME = PopEngine
APP_NAME = PopEngineTestApp


# X ?= if X env var isn't set, use this default
compiler ?= g++
CC = ${compiler}

# osTarget is the target os eg. Nvidia / Pi4
# tsdk: the naming of the build folders are handled by the github actions
# osTarget is still used for if statements in the make file
osTarget ?= Linux
BUILD_DIR = ../Build

CONFIGURATION ?= Release

PROJECT_PATH = ..
SRC_PATH = $(PROJECT_PATH)/src
SOY_PATH = $(PROJECT_PATH)/src/SoyLib

# tsdk: Env var that is set by the github action
GITHUB_LIB_PATH = ${github_lib_dir}

LIB_INCLUDES = \
-I$(SOY_PATH)/src	\
-I$(SOY_PATH)/src/magic_enum/include	\
-I$(SOY_PATH)/src	\
-I$(SRC_PATH)/Json11	\
-I/usr/include/webkitgtk-4.0	\
-I$(PROJECT_PATH)/Libs/poph264/$(GITHUB_LIB_PATH)	\
-I$(PROJECT_PATH)/Libs/popcameradevice/$(GITHUB_LIB_PATH)	\

LIB_SRC_FILES  = \
$(SRC_PATH)/Json11/json11.cpp \
$(SRC_PATH)/JsCoreBind.cpp \
$(SRC_PATH)/PopMain.cpp \
$(SRC_PATH)/PopEngine.cpp \
$(SRC_PATH)/TBind.cpp \
$(SRC_PATH)/TApiCommon.cpp \
$(SRC_PATH)/TApiHttp.cpp \
$(SRC_PATH)/TApiMedia.cpp \
$(SRC_PATH)/TApiPanopoly.cpp \
$(SRC_PATH)/TApiSocket.cpp \
$(SRC_PATH)/TApiWebsocket.cpp \

#$(SRC_PATH)/TApiEngine.cpp \


# soy lib files
LIB_SRC_FILES  += \
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


APP_SRC_FILES =	\
$(PROJECT_PATH)/Source_App/PopEngineUnitTestMain.cpp	\

LIB_DEFINES = \
-DTARGET_LINUX	\
-DJSAPI_JSCORE


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
LDFLAGS = -pthread
LDFLAGS += '-Wl,-rpath,$$ORIGIN'
LDFLAGS += '-Wl,-rpath-link=$(BUILD_DIR)'

LIB_LINK_FLAGS = $(LDFLAGS) -shared
APP_LINK_FLAGS = $(LDFLAGS)

# tsdk: the paths have to be lower case as that is how node imports them
LIB_LINK_LIBS = \
-ljavascriptcoregtk-4.0	\
-L/usr/lib/aarch64-linux-gnu	\
-lPopH264 \
-L$(PROJECT_PATH)/Libs/poph264/$(GITHUB_LIB_PATH)	\
-lPopCameraDevice \
-L$(PROJECT_PATH)/Libs/popcameradevice/$(GITHUB_LIB_PATH)	\


LIB_OBJECTS=$(LIB_SRC_FILES:.cpp=.o)
APP_OBJECTS=$(APP_SRC_FILES:.cpp=.o)

OUT_LIB=$(BUILD_DIR)/lib${LIB_NAME}.so
OUT_APP=$(BUILD_DIR)/${APP_NAME}


# These files (without directory structure) get copied to build dir
OUT_FILE_SOURCES = \
$(SRC_PATH)/PopEngine.h	\
$(PROJECT_PATH)/Libs/poph264/libPopH264.so \
$(PROJECT_PATH)/Libs/popcameradevice/libPopCameraDevice.so \

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

.cpp.o:
	$(CC) $(CFLAGS) $(APP_INCLUDES) $(LIB_INCLUDES) $< -o $@

clean:
	# clean build files, anything .o
	$(info Cleaning files)
	rm -f $(APP_OBJECTS)
	rm -f $(LIB_OBJECTS)
	rm -f $(OUT_LIB)
	rm -f $(OUT_APP)

