#include "SoyMagicLeap.h"

#include <sstream>
#include "SoyDebug.h"

#include <ml_head_tracking.h>
#include <ml_graphics.h>
#include <ml_input.h>
#include <ml_lifecycle.h>
#include <ml_logging.h>
#include <ml_perception.h>
#include <ml_planes.h>
#include <ml_privileges.h>


namespace MagicLeap
{
	void		IsOkay(MLResult Error,const std::string& Context);
}



void MagicLeap::IsOkay(MLResult Error,const std::string& Context)
{
	if ( Error == MLResult_Ok )
		return;
	
	std::stringstream ErrorStr;
	ErrorStr << "Error " << MLGetResultString(Error) << " in " << Context;
	throw Soy::AssertException(ErrorStr);
}

#define CHECK( Line )	do \
	{\
		auto Result = Line;	\
		IsOkay( Result, __PRETTY_FUNCTION__ );	\
	} while(0)


class MagicLeap::TContext
{
public:
	TContext();
	~TContext();
	
	void		CheckPrivilege(MLPrivilegeID Privilige);
};


class MagicLeap::TOpenglContext
{
public:
	TOpenglContext();
	~TOpenglContext();
};

class MagicLeap::THmd
{
public:
	THmd(TContext& Context);
	~THmd();
	
	void			Iteration();
	
	MLHandle					mHeadTracker = ML_INVALID_HANDLE;
	MLHeadTrackingStaticData	mHeadMeta;
};


class MagicLeap::TSpatial
{
public:
	TSpatial(TContext& Context);
	~TSpatial();
	
	MLHandle	mPlanes = ML_INVALID_HANDLE;
};


class MagicLeap::TInput
{
public:
	TInput(TContext& Context);
	~TInput();
	
	void		Update();
	
	MLHandle	mHandle = ML_INVALID_HANDLE;
};



MagicLeap::TContext::TContext()
{
	//	hook std::Debug
	auto Result = MLLifecycleInit( nullptr, nullptr );
	IsOkay(Result,"MLLifecycleInit");
	
	Result = MLPrivilegesStartup();
	IsOkay(Result,"MLPrivilegesStartup");
	
	//	does this need to be after we've done everything else?
	CHECK(MLLifecycleSetReadyIndication());
}


MagicLeap::TSpatial::TSpatial(TContext& Context)
{
	Context.CheckPrivilege(MLPrivilegeID_WorldReconstruction);
	
	CHECK(MLPlanesCreate(&mPlanes));
	/*
	MLHandle planes_query = ML_INVALID_HANDLE; // invalid unless query running
	MLPlane query_results[16]; // 16 is arbitrary; seemed reasonable
	uint32_t query_nresults = 0;
	bool quit = false; // loop exits when true
	 // ---- GET PLANES
	 // Start a plane query if none is running. We'll grab the head position
	 // and use that to define the bounding box around the query.
	 
	 if (planes_query == ML_INVALID_HANDLE) {
	 
	 MLSnapshot *snapshot;
	 CHECK(MLPerceptionGetSnapshot(&snapshot));
	 
	 MLHeadTrackingState ht_state;
	 MLTransform ht_transform;
	 
	 bool ht_valid =
	 (MLHeadTrackingGetState(head_tracking, &ht_state) == MLResult_Ok) &&
	 (ht_state.error == MLHeadTrackingError_None) &&
	 (ht_state.mode == MLHeadTrackingMode_6DOF) &&
	 (ht_state.confidence > 0.9) &&
	 (MLSnapshotGetTransform(snapshot, &head_sdata.coord_frame_head, &ht_transform) == MLResult_Ok);
	 
	 CHECK(MLPerceptionReleaseSnapshot(snapshot));
	 
	 if (ht_valid) {
	 
	 // Search in a 15 meter cube centered on the headset.
	 MLPlanesQuery query = {};
	 query.bounds_center = ht_transform.position;
	 query.bounds_extents.x = 15;
	 query.bounds_extents.y = 15;
	 query.bounds_extents.z = 15;
	 query.bounds_rotation = ht_transform.rotation;
	 query.flags = MLPlanesQueryFlag_AllOrientations | MLPlanesQueryFlag_Semantic_All;
	 query.max_results = sizeof(query_results) / sizeof(MLPlane);
	 query.min_plane_area = 0.2f;
	 
	 CHECK(MLPlanesQueryBegin(planes, &query, &planes_query));
	 
	 }
	 
	 }
	 
	 // If a plane query is running, retrieve the results if its ready.
	 if (planes_query != ML_INVALID_HANDLE &&
	 MLPlanesQueryGetResultsWithBoundaries(planes, planes_query, query_results, &query_nresults, NULL) == MLResult_Ok)
	 {
	 planes_query = ML_INVALID_HANDLE;
	 }

	 */
}


MagicLeap::THmd::THmd(TContext& Context)
{
	Context.CheckPrivilege(MLPrivilegeID_LowLatencyLightwear);

	MLPerceptionSettings perception_settings;
	CHECK(MLPerceptionInitSettings(&perception_settings));
	CHECK(MLPerceptionStartup(&perception_settings));
	
	CHECK(MLHeadTrackingCreate(&mHeadTracker));
	CHECK(MLHeadTrackingGetStaticData(mHeadTracker, &mHeadMeta));
}

MagicLeap::THmd::~THmd()
{
	CHECK(MLHeadTrackingDestroy(mHeadTracker));
	CHECK(MLPerceptionShutdown());
}

MagicLeap::TInput::TInput(TContext& Context)
{
	Context.CheckPrivilege(MLPrivilegeID_ControllerPose);

	CHECK(MLInputCreate(nullptr, &mHandle));
}

MagicLeap::TInput::~TInput()
{
	CHECK(MLInputDestroy(mHandle));
}

void MagicLeap::TInput::Update()
{
	MLInputControllerState input_states[MLInput_MaxControllers];
	CHECK(MLInputGetControllerState(mHandle, input_states));
	/*
	for (int k = 0; k < MLInput_MaxControllers; ++k)
	{
		if (input_states[k].button_state[MLInputControllerButton_Bumper]) {
			ML_LOG_TAG(Info, APP_TAG, "Bye!");
			quit = true;
			break;
		}
	}
	 */
}


void MagicLeap::THmd::Iteration()
{
	/*
	// ---- RENDER SCENE
	
	MLGraphicsFrameParams frame_params;
	CHECK(MLGraphicsInitFrameParams(&frame_params));
	frame_params.near_clip = 0.1f;
	frame_params.far_clip = 100.0f;
	frame_params.focus_distance = 1.0f;
	frame_params.projection_type = MLGraphicsProjectionType_SignedZ;
	frame_params.protected_surface = false;
	frame_params.surface_scale = 1.0f;
	
	MLHandle frame;
	MLGraphicsVirtualCameraInfoArray cameras;
	MLResult frame_result = MLGraphicsBeginFrame(graphics, &frame_params, &frame, &cameras);
	
	if (frame_result == MLResult_Ok) {
		
		/* How to render things (fixed function pipeline version):
		 *
		 * The scene must be rendered for each virtual camera (left eye, right eye).
		 *
		 * The MLGraphicsVirtualCameraInfoArray structure contains all the info
		 * needed to set up the viewport, projection, and modelview matrices for
		 * each camera, and implicitly encapsulates things like the head position,
		 * visual calibration, etc.
		 *
		 * - Viewport: Common to all cameras and specified in .viewport.
		 * - Projection: Directly specified for each camera in .projection.
		 * - Modelview: The inverse of the camera transform for each camera.
		 *
		 * Rendering to each camera is accomplished by rendering to a specific layer
		 * in a multi-layered texture created by the API. Those texture names are
		 * given to us in .color_id and .depth_id, and the layer number corresponding
		 * to each camera is the integer name of the camera (note: this may not be the
		 * same as the index in the camera array so always use .virtual_camera_name!).
		 */
		/*
		for (int k = 0; k < MLGraphicsVirtualCameraName_Count; ++k) {
			
			// Set render target to the appropriate texture layer for this camera.
			const MLGraphicsVirtualCameraInfo &camera = cameras.virtual_cameras[k];
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
			glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cameras.color_id, 0, camera.virtual_camera_name);
			glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, cameras.depth_id, 0, camera.virtual_camera_name);
			
			// Viewport is common to all cameras and is given.
			glViewport((int)(cameras.viewport.x + 0.5f), (int)(cameras.viewport.y + 0.5f),
					   (GLsizei)(cameras.viewport.w + 0.5f), (GLsizei)(cameras.viewport.h + 0.5f));
			
			// Projection matrix is given directly for each camera.
			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glLoadMatrixf(camera.projection.matrix_colmajor);
			
			// Modelview is inverse of transform for each camera. GLM simplifies this.
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			glLoadMatrixf(glm::value_ptr(glm::inverse(mlToGL(camera.transform))));
			
			// Now we can draw things in the reality coordinate frame. Also, this example
			// renders once to a call list and uses that for remaining cameras.
			if (k == 0) {
				glNewList(calllist, GL_COMPILE_AND_EXECUTE);
				drawPlanes(query_results, query_nresults);
				glEndList();
			} else {
				glCallList(calllist);
			}
			
			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();
			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			
			// Signal that we're done rendering this camera. MLGraphicsEndFrame will
			// fail if you don't trigger the signal for every camera, so even if your
			// app, say, only runs in the left eye, you still need to signal everything.
			MLGraphicsSignalSyncObjectGL(graphics, cameras.virtual_cameras[k].sync_object);
			
		}
		
		// End frame must match begin frame.
		CHECK(MLGraphicsEndFrame(graphics, frame));
		graphics_context.swapBuffers();
		
	} else if (frame_result != MLResult_Timeout) { // sometimes it fails with timeout when device is busy
		
		ML_LOG_TAG(Error, APP_TAG, "MLGraphicsBeginFrame failed with %d", frame_result);
		quit = true;
		
	}
*/
}

