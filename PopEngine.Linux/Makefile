LIBRARY_NAME = PopEngine
TESTAPP_NAME = PopEngineTestApp


# X ?= if X env var isn't set, use this default
compiler ?= g++
CC = ${compiler}

# archTarget is the target architecture
# if unspecified, this becomes Linux_Linux/ pick something better :)
archTarget ?= Linux
BUILD_DIR = ../Build/Linux_$(archTarget)

SRC = ..
SRC_PATH = $(SRC)/src
SOY_PATH = $(SRC_PATH)/SoyLib

LOCAL_C_INCLUDES = \
-I$(SOY_PATH)/src	\
-I$(SOY_PATH)/src/magic_enum/include	\
-I$(SOY_PATH)/src	\
-I$(SRC_PATH)/Json11	\
-I/usr/include/webkitgtk-4.0	\

LOCAL_SRC_FILES  = \
$(SRC_PATH)/Json11/json11.cpp \
$(SRC_PATH)/JsCoreBind.cpp \
$(SRC_PATH)/PopEngine.cpp \
$(SRC_PATH)/PopMain.cpp \
$(SRC_PATH)/TBind.cpp \
$(SRC_PATH)/TApiCommon.cpp \
$(SRC_PATH)/TApiEngine.cpp \
$(SRC_PATH)/TApiHttp.cpp \
$(SRC_PATH)/TApiMedia.cpp \
$(SRC_PATH)/TApiPanopoly.cpp \
$(SRC_PATH)/TApiSocket.cpp \
$(SRC_PATH)/TApiWebsocket.cpp \

#$(SRC_PATH)/TApiEngine.cpp \


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


TESTAPP_SRC_FILES =	\
$(SRC)/Source_App/PopEngineUnitTestMain.cpp	\

# -g stops symbol stripping
# -f[lag] PIC position independent code (for dyanmically loaded code)
# -rpath $ORIGIN makes the shared object runtimepath be same place as executable
CFLAGS = -c -DTARGET_LINUX -DJSAPI_JSCORE  $(LOCAL_C_INCLUDES) -std=gnu++17 -fPIC -g
LDFLAGS = -pthread
LDFLAGS += '-Wl,-rpath,$$ORIGIN'
LDFLAGS += '-Wl,-rpath-link=$(BUILD_DIR)'

LIB_LINK_FLAGS = $(LDFLAGS) -shared
TEST_APP_LINK_FLAGS = $(LDFLAGS)

LIBRARY_LINK_LIBS = \
-ljavascriptcoregtk-4.0	\
-L/usr/lib/aarch64-linux-gnu	\
-lPopH264 \
-L$(SRC_PATH)/Libs/PopH264	\
-lPopCameraDevice \
-L$(SRC_PATH)/Libs/PopCameraDevice	\


LIB_OBJECTS=$(LOCAL_SRC_FILES:.cpp=.o)
TESTAPP_OBJECTS=$(TESTAPP_SRC_FILES:.cpp=.o)

# ARCHITECTURE=${archTarget}
# env var MACHTYPE is this systems arch type, eg; aarch64-unknown-linux-gnu
OUT_LIBRARY=$(BUILD_DIR)/lib${LIBRARY_NAME}.so
OUT_EXECUTABLE=$(BUILD_DIR)/${TESTAPP_NAME}

# gr: automate this somehow so its a list of sources, then OUT_ is just their name with no directory structire
OUT_FILE_SOURCES = \
$(SRC_PATH)/PopEngine.h	\
$(SRC_PATH)/Libs/PopH264/libPopH264.so \
$(SRC_PATH)/Libs/PopCameraDevice/libPopCameraDevice.so \

OUT_FILES = $(addprefix ${BUILD_DIR}/,$(notdir ${OUT_FILE_SOURCES}))
$(info out files...)
$(info ${OUT_FILES})

GithubWorkflow: $(OUT_EXECUTABLE) $(OUT_LIBRARY)
	# gr: this prints out the word echo " should it be $(info ::set-output) ?
	echo "::set-output name=executable:$(OUT_EXECUTABLE)"
	echo "::set-output name=library:$(OUT_LIBRARY)"
.PHONY: GithubWorkflow

exec: $(OUT_EXECUTABLE)
.PHONY: exec

lib: $(OUT_LIBRARY) 
.PHONY: lib

# Copy other output files
$(OUT_FILES): $(OUT_FILE_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(info Copying output files $(OUT_FILES))
	$(foreach SourceFilename,$(OUT_FILE_SOURCES),'cp $(SourceFilename) ${BUILD_DIR}')

$(OUT_EXECUTABLE): $(TESTAPP_OBJECTS) $(OUT_LIBRARY)
	$(info Building test app $(OUT_EXECUTABLE))
	mkdir -p $(BUILD_DIR)
	$(CC) $(TEST_APP_LINK_FLAGS) $(TESTAPP_OBJECTS) -o $@ -L$(BUILD_DIR) -l$(LIBRARY_NAME)

$(OUT_LIBRARY): $(LIB_OBJECTS) $(OUT_HEADER) $(OUT_FILES)
	$(info Building library $(OUT_LIBRARY))
	mkdir -p $(BUILD_DIR)
	$(CC) $(LIB_LINK_FLAGS) -o $@ $(LIB_OBJECTS) $(LIBRARY_LINK_LIBS)
	# ar rcs $@ $(LIB_OBJECTS)
	$(info Listing dependencies)
	ldd $(OUT_LIBRARY)

.cpp.o:
	$(CC) $(CFLAGS) $(LOCAL_C_INCLUDES) $(LIBRARY_C_INCLUDES) $< -o $@

clean:
	# clean build files, anything .o
	$(info Cleaning files)
	rm -f $(TESTAPP_OBJECTS)
	rm -f $(LIB_OBJECTS)	
	rm -f $(OUT_LIBRARY)
	rm -f $(OUT_EXECUTABLE)
