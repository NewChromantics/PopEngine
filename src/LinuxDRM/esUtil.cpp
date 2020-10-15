//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//

// ESUtil.c
//
//    A utility library for OpenGL ES.  This library provides a
//    basic common framework for the example applications in the
//    OpenGL ES 2.0 Programming Guide.
//

///
//  Includes
//
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include "esUtil.h"
#include <EGL/egl.h>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <iostream>

#include "common.h"
#include "drm-common.h"
#include "drm-legacy.h"

#define THROW(m)                 \
	{                              \
		std::cerr << m << std::endl; \
		return EGL_FALSE;            \
	}

#define THROWEGL() THROW(eglErrorString(eglGetError()))

// DRM local variables
static struct egl egl;				// TODO: This egl is basically an ESContext.  Need to think
static const struct gbm *gbm; // actual object is a static variable in common.c
static const struct drm *drm; // actual object is a static variable in drm-legacy.c

///
//  WinCreate()
//
//      This function initializes the native DRM display and window for EGL
//
EGLBoolean WinCreate(ESContext *esContext, const char *title)
{
	const char *device = "/dev/dri/card1";
	drm = init_drm_legacy(device, "", 0);
	if (!drm)
	{
		printf("Failed to initialize DRM %s\n", device);
		return EGL_FALSE;
	}

	uint64_t modifier = DRM_FORMAT_MOD_LINEAR;
	uint32_t format = DRM_FORMAT_XRGB8888;

	printf("Try to init gbm with fd=%i, h=%i v=%i\n", drm->fd, drm->mode->hdisplay, drm->mode->vdisplay);
	gbm = init_gbm(drm->fd, drm->mode->hdisplay, drm->mode->vdisplay, format, modifier);
	if (!gbm)
	{
		printf("Failed to initialize GBM\n");
		return EGL_FALSE;
	}

	if (init_egl(&egl, gbm, 0))
	{
		return EGL_FALSE;
	}

	esContext->screenWidth = drm->mode->hdisplay;
	esContext->screenHeight = drm->mode->vdisplay;
	esContext->hWnd = (EGLNativeWindowType)egl.surface;
	esContext->eglDisplay = egl.display;
	esContext->eglContext = egl.context;
	esContext->eglSurface = egl.surface;

	// tsdk: Release this thread so that we can change context in the sokol code
	if (eglGetCurrentContext() != NULL)
		eglReleaseThread();

	return EGL_TRUE;
}

///
//  userInterrupt()
//
//
GLboolean userInterrupt(ESContext *esContext)
{
	//      TODO: spin off a thread that monitors the keyboard otherwise this will block
	/* C++ aircode
    char keypress;
    while (std::cin)
    {
        std::cin >> keypress;
        if (esContext->keyFunc != NULL)
        {
            esContext->keyFunc(esContext, keypress, 0, 0);
        }
    }
    */
	return GL_FALSE;
}

//////////////////////////////////////////////////////////////////
//
//  Public Functions
//
//

///
//  esInitContext()
//
//      Initialize ES utility context.  This must be called before calling any other
//      functions.
//
void ESUTIL_API esInitContext(ESContext *esContext)
{
	if (esContext != NULL)
	{
		memset(esContext, 0, sizeof(ESContext));
	}
}

///
//  esCreateWindow()
//
//      title - name for title bar of window
//      width - width of window to create
//      height - height of window to create
//      flags  - bitwise or of window creation flags
//          ES_WINDOW_ALPHA       - specifies that the framebuffer should have alpha
//          ES_WINDOW_DEPTH       - specifies that a depth buffer should be created
//          ES_WINDOW_STENCIL     - specifies that a stencil buffer should be created
//          ES_WINDOW_MULTISAMPLE - specifies that a multi-sample buffer should be created
//
GLboolean ESUTIL_API esCreateWindow(ESContext *esContext, const char *title, GLint width, GLint height, GLuint flags)
{
	if (esContext == NULL)
	{
		return GL_FALSE;
	}

	if (!WinCreate(esContext, title))
	{
		return GL_FALSE;
	}

	// Right now these are set to the screen resolution cached in the Window Creation
	esContext->width = width != 0 ? width : esContext->screenWidth;
	esContext->height = height != 0 ? : esContext->screenHeight;

	return GL_TRUE;
}

//
// esCreateHeadless()
//
GLboolean ESUTIL_API esCreateHeadless(ESContext *esContext, const char *title, GLint width, GLint height)
{
	if (esContext == NULL)
	{
		return GL_FALSE;
	}

	int Width = width != 0 ? width : 640;
	int Height = height != 0 ? width : 480;

	PFNEGLQUERYDEVICESEXTPROC _eglQueryDevicesEXT = NULL;
	PFNEGLQUERYDEVICESTRINGEXTPROC _eglQueryDeviceStringEXT = NULL;
	PFNEGLGETPLATFORMDISPLAYEXTPROC _eglGetPlatformDisplayEXT = NULL;

	// For the physical display this is handled by the function init_egl
	// Clean this up by moving it in there?
	int num_devices = 0, i;
	EGLDeviceEXT *devices = NULL;
	EGLDisplay Display = NULL;
	EGLConfig *configs = NULL;

	int attribs[] = 
	{
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_SURFACE_TYPE,
		EGL_PBUFFER_BIT,
		EGL_COLOR_BUFFER_TYPE,
		EGL_RGB_BUFFER,
		EGL_NONE
	};

	int pbattribs[] = 
	{
		EGL_WIDTH, Width,
		EGL_HEIGHT, Height,
		EGL_NONE
	};

	int major, minor, ret = 0, nc = 0;
	EGLSurface PixelBuffer = 0;
	EGLContext Context = 0;
	unsigned char *buf = NULL;

	if ((_eglQueryDevicesEXT =
					 (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT")) == NULL)
		THROW("eglQueryDevicesEXT() could not be loaded");
	if ((_eglQueryDeviceStringEXT =
					 (PFNEGLQUERYDEVICESTRINGEXTPROC)eglGetProcAddress("eglQueryDeviceStringEXT")) == NULL)
		THROW("eglQueryDeviceStringEXT() could not be loaded");
	if ((_eglGetPlatformDisplayEXT =
					 (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT")) == NULL)
		THROW("eglGetPlatformDisplayEXT() could not be loaded");

	if (!_eglQueryDevicesEXT(0, NULL, &num_devices) || num_devices < 1)
		THROWEGL();
	if ((devices =
					 (EGLDeviceEXT *)malloc(sizeof(EGLDeviceEXT) * num_devices)) == NULL)
		THROW("Memory allocation failure");
	if (!_eglQueryDevicesEXT(num_devices, devices, &num_devices) || num_devices < 1)
		THROWEGL();

	for (i = 0; i < num_devices; i++)
	{
		const char *devstr = _eglQueryDeviceStringEXT(devices[i],
																									EGL_DRM_DEVICE_FILE_EXT);
		printf("Device 0x%.8lx: %s\n", (unsigned long)devices[i],
					 devstr ? devstr : "NULL");
	}

	if ((Display = _eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT,
																			 devices[0], NULL)) == NULL)
		THROWEGL();
	if (!eglInitialize(Display, &major, &minor))
		THROWEGL();
	printf("EGL version %d.%d\n", major, minor);
	if (!eglChooseConfig(Display, attribs, NULL, 0, &nc) || nc < 1)
		THROWEGL();
	if ((configs = (EGLConfig *)malloc(sizeof(EGLConfig) * nc)) == NULL)
		THROW("Memory allocation failure");
	if (!eglChooseConfig(Display, attribs, configs, nc, &nc) || nc < 1)
		THROWEGL();

	if ((PixelBuffer = eglCreatePbufferSurface(Display, configs[0], pbattribs)) == NULL)
		THROWEGL();
	if (!eglBindAPI(EGL_OPENGL_API))
		THROWEGL();
	if ((Context = eglCreateContext(Display, configs[0], NULL, NULL)) == NULL)
		THROWEGL();
	if (!eglMakeCurrent(Display, PixelBuffer, PixelBuffer, Context))
		THROWEGL();

	esContext->screenWidth = Width;
	esContext->screenHeight = Height;
	esContext->eglDisplay = Display;
	esContext->eglContext = Context;
	esContext->eglSurface = PixelBuffer;

	// tsdk: Release this thread so that we can change context in the sokol code
	if (eglGetCurrentContext() != NULL)
		eglReleaseThread();

	return EGL_TRUE;
	/* Need to remember to put this cleanup in the deconstructor
	if(Context && Display) eglDestroyContext(Display, Context);
	if(PixelBuffer && Display) eglDestroySurface(Display, PixelBuffer);
	if(Display) eglTerminate(Display);
	if(configs) free(configs);
	if(devices) free(devices);
		*/
}

///
//  esPaint()
//
//    The Paint loop for the OpenGL ES application

static struct gbm_bo *previous_bo = NULL;
static uint32_t previous_fb;

void ESUTIL_API esPaint(ESContext *esContext)
{
	if (esContext->drawFunc)
		esContext->drawFunc();

	auto test = esContext->hWnd;
	if(esContext->hWnd == NULL)
		glFlush();
	else
	{
		eglSwapBuffers(esContext->eglDisplay, esContext->eglSurface);
		struct gbm_bo *bo = gbm_surface_lock_front_buffer(gbm->surface);
		uint32_t handle = gbm_bo_get_handle(bo).u32;
		uint32_t pitch = gbm_bo_get_stride(bo);
		uint32_t fb;
		uint32_t connectorID = reinterpret_cast<uint32_t>(drm->connector_id);
		drmModeAddFB(drm->fd, esContext->screenWidth, esContext->screenHeight, 24, 32, pitch, handle, &fb);
		drmModeSetCrtc(drm->fd, drm->crtc_id, fb, 0, 0, &connectorID, 1, drm->mode);

		if (previous_bo)
		{
			drmModeRmFB(drm->fd, previous_fb);
			gbm_surface_release_buffer(gbm->surface, previous_bo);
		}
		previous_bo = bo;
		previous_fb = fb;
	}

}

///
//  esRegisterDrawFunc()
//
void ESUTIL_API esRegisterDrawFunc(ESContext *esContext, ESCALLBACK std::function<void()> drawFunc)
{
	esContext->drawFunc = drawFunc;
}

///
//  esRegisterUpdateFunc()
//
void ESUTIL_API esRegisterUpdateFunc(ESContext *esContext, void(ESCALLBACK *updateFunc)(ESContext *, float))
{
	esContext->updateFunc = updateFunc;
}

///
//  esRegisterKeyFunc()
//
void ESUTIL_API esRegisterKeyFunc(ESContext *esContext,
																	void(ESCALLBACK *keyFunc)(ESContext *, unsigned char, int, int))
{
	esContext->keyFunc = keyFunc;
}

///
// esLogMessage()
//
//    Log an error message to the debug output for the platform
//
void ESUTIL_API esLogMessage(const char *formatStr, ...)
{
	va_list params;
	char buf[BUFSIZ];

	va_start(params, formatStr);
	vsprintf(buf, formatStr, params);

	printf("%s", buf);

	va_end(params);
}

///
// esLoadTGA()
//
//    Loads a 24-bit TGA image from a file. This is probably the simplest TGA loader ever.
//    Does not support loading of compressed TGAs nor TGAa with alpha channel. But for the
//    sake of the examples, this is sufficient.
//

char *ESUTIL_API esLoadTGA(char *fileName, int *width, int *height)
{
	char *buffer = NULL;
	FILE *f;
	unsigned char tgaheader[12];
	unsigned char attributes[6];
	unsigned int imagesize;

	f = fopen(fileName, "rb");
	if (f == NULL)
		return NULL;

	if (fread(&tgaheader, sizeof(tgaheader), 1, f) == 0)
	{
		fclose(f);
		return NULL;
	}

	if (fread(attributes, sizeof(attributes), 1, f) == 0)
	{
		fclose(f);
		return 0;
	}

	*width = attributes[1] * 256 + attributes[0];
	*height = attributes[3] * 256 + attributes[2];
	imagesize = attributes[4] / 8 * *width * *height;
	buffer = (char *)malloc(imagesize);
	if (buffer == NULL)
	{
		fclose(f);
		return 0;
	}

	if (fread(buffer, 1, imagesize, f) != imagesize)
	{
		free(buffer);
		return NULL;
	}
	fclose(f);
	return buffer;
}
