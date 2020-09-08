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
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include "esUtil.h"

#include "common.h"
#include "drm-common.h"
#include "drm-legacy.h"

// DRM local variables
static struct egl egl;   // TODO: This egl is basically an ESContext.  Need to think
static const struct gbm *gbm;  // actual object is a static variable in common.c
static const struct drm *drm;  // actual object is a static variable in drm-legacy.c

///
//  WinCreate()
//
//      This function initializes the native DRM display and window for EGL
//
EGLBoolean WinCreate(ESContext *esContext, const char *title)
{
    const char *device = "/dev/dri/card1";
    drm = init_drm_legacy(device, "", 0);
    if (!drm) {
        printf("Failed to initialize DRM %s\n", device);
        return EGL_FALSE;
    }

    uint64_t modifier = DRM_FORMAT_MOD_LINEAR;
    uint32_t format = DRM_FORMAT_XRGB8888;

    printf("Try to init gbm with fd=%i, h=%i v=%i\n", drm->fd, drm->mode->hdisplay, drm->mode->vdisplay);
    gbm = init_gbm(drm->fd, drm->mode->hdisplay, drm->mode->vdisplay, format, modifier);
	if (!gbm) {
        printf("Failed to initialize GBM\n");
        return EGL_FALSE;
	}

    if (init_egl(&egl, gbm, 0))
    {
		return EGL_FALSE;
    }

    esContext->screenWidth = drm->mode->vdisplay;
    esContext->screenHeight = drm->mode->hdisplay;
    esContext->hWnd = (EGLNativeWindowType)egl.surface;
    esContext->eglDisplay=egl.display;
    esContext->eglContext=egl.context;
    esContext->eglSurface=egl.surface;
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
void ESUTIL_API esInitContext ( ESContext *esContext )
{
   if ( esContext != NULL )
   {
      memset( esContext, 0, sizeof( ESContext) );
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
GLboolean ESUTIL_API esCreateWindow ( ESContext *esContext, const char* title, GLint width, GLint height, GLuint flags )
{
   if ( esContext == NULL )
   {
      return GL_FALSE;
   }

   if ( !WinCreate ( esContext, title) )
   {
      return GL_FALSE;
   }

   // Right now these are set to the screen resolution cached in the Window Creation
   esContext->width = esContext->screenWidth;
   esContext->height = esContext->screenHeight;

   return GL_TRUE;
}


///
//  esMainLoop()
//
//    Start the main loop for the OpenGL ES application
//

void ESUTIL_API esMainLoop ( ESContext *esContext )
{
    struct timeval t1, t2;
    struct timezone tz;
    float deltatime;
    float totaltime = 0.0f;
    unsigned int frames = 0;

	fd_set fds;
	drmEventContext evctx = {
			.version = 2,
			.page_flip_handler = page_flip_handler,
	};
	struct gbm_bo *bo;
	struct drm_fb *fb;
	uint32_t i = 0;
	int ret;

    eglSwapBuffers(esContext->eglDisplay, esContext->eglSurface);
	bo = gbm_surface_lock_front_buffer(gbm->surface);
	if (!bo) {
		fprintf(stderr, "Failed to get a new framebuffer BO\n");
		return;
	}

	fb = drm_fb_get_from_bo(bo);
	if (!fb) {
		fprintf(stderr, "Failed to get a new framebuffer from BO\n");
		return;
	}

	/* set mode: */
	ret = drmModeSetCrtc(drm->fd, drm->crtc_id, fb->fb_id, 0, 0,
			const_cast<uint32_t*>(&drm->connector_id), 1, drm->mode);
	if (ret) {
		printf("failed to set mode: %s\n", strerror(errno));
		return;
	}

    gettimeofday ( &t1 , &tz );

    while(userInterrupt(esContext) == GL_FALSE)
    {
        gettimeofday(&t2, &tz);
        deltatime = (float)(t2.tv_sec - t1.tv_sec + (t2.tv_usec - t1.tv_usec) * 1e-6);
        t1 = t2;

		struct gbm_bo *next_bo;
		int waiting_for_flip = 1;

        if (esContext->updateFunc != NULL)
            esContext->updateFunc(esContext, deltatime);
        if ( esContext->drawFunc )
            esContext->drawFunc();

		//egl.draw(i++);

        eglSwapBuffers(esContext->eglDisplay, esContext->eglSurface);
		next_bo = gbm_surface_lock_front_buffer(gbm->surface);
		fb = drm_fb_get_from_bo(next_bo);
		if (!fb) {
			fprintf(stderr, "Failed to get a new framebuffer BO\n");
			return;
		}

		/*
		 * Here you could also update drm plane layers if you want
		 * hw composition
		 */

		ret = drmModePageFlip(drm->fd, drm->crtc_id, fb->fb_id,
				DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip);
		if (ret) {
			printf("failed to queue page flip: %s\n", strerror(errno));
			return;
		}

		while (waiting_for_flip) {
			FD_ZERO(&fds);
			FD_SET(0, &fds);
			FD_SET(drm->fd, &fds);

			ret = select(drm->fd + 1, &fds, NULL, NULL, NULL);
			if (ret < 0) {
				printf("select err: %s\n", strerror(errno));
				return;
			} else if (ret == 0) {
				printf("select timeout!\n");
				return;
			} else if (FD_ISSET(0, &fds)) {
				printf("user interrupted!\n");
				return;
			}
			drmHandleEvent(drm->fd, &evctx);
		} 

		/* release last buffer to render on again: */
		gbm_surface_release_buffer(gbm->surface, bo);
		bo = next_bo;

        totaltime += deltatime;
        frames++;
        if (totaltime >  2.0f)
        {
            printf("%4d frames rendered in %1.4f seconds -> FPS=%3.4f\n", frames, totaltime, frames/totaltime);
            totaltime -= 2.0f;
            frames = 0;
        }
    }
}


///
//  esRegisterDrawFunc()
//
void ESUTIL_API esRegisterDrawFunc ( ESContext *esContext, ESCALLBACK std::function<void()> drawFunc )
{
   esContext->drawFunc = drawFunc;
}

///
//  esRegisterUpdateFunc()
//
void ESUTIL_API esRegisterUpdateFunc ( ESContext *esContext, void (ESCALLBACK *updateFunc) ( ESContext*, float ) )
{
   esContext->updateFunc = updateFunc;
}


///
//  esRegisterKeyFunc()
//
void ESUTIL_API esRegisterKeyFunc ( ESContext *esContext,
                                    void (ESCALLBACK *keyFunc) (ESContext*, unsigned char, int, int ) )
{
   esContext->keyFunc = keyFunc;
}


///
// esLogMessage()
//
//    Log an error message to the debug output for the platform
//
void ESUTIL_API esLogMessage ( const char *formatStr, ... )
{
    va_list params;
    char buf[BUFSIZ];

    va_start ( params, formatStr );
    vsprintf ( buf, formatStr, params );
    
    printf ( "%s", buf );
    
    va_end ( params );
}


///
// esLoadTGA()
//
//    Loads a 24-bit TGA image from a file. This is probably the simplest TGA loader ever.
//    Does not support loading of compressed TGAs nor TGAa with alpha channel. But for the
//    sake of the examples, this is sufficient.
//

char* ESUTIL_API esLoadTGA ( char *fileName, int *width, int *height )
{
    char *buffer = NULL;
    FILE *f;
    unsigned char tgaheader[12];
    unsigned char attributes[6];
    unsigned int imagesize;

    f = fopen(fileName, "rb");
    if(f == NULL) return NULL;

    if(fread(&tgaheader, sizeof(tgaheader), 1, f) == 0)
    {
        fclose(f);
        return NULL;
    }

    if(fread(attributes, sizeof(attributes), 1, f) == 0)
    {
        fclose(f);
        return 0;
    }

    *width = attributes[1] * 256 + attributes[0];
    *height = attributes[3] * 256 + attributes[2];
    imagesize = attributes[4] / 8 * *width * *height;
    buffer = (char*)malloc(imagesize);
    if (buffer == NULL)
    {
        fclose(f);
        return 0;
    }

    if(fread(buffer, 1, imagesize, f) != imagesize)
    {
        free(buffer);
        return NULL;
    }
    fclose(f);
    return buffer;
}
