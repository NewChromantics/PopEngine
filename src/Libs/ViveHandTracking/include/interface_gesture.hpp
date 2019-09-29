#ifndef INTERFACE_GESTURE_HPP
#define INTERFACE_GESTURE_HPP

#ifdef _WIN32
#define INTERFACE_GESTURE_EXPORTS __declspec(dllimport)
#else
#define INTERFACE_GESTURE_EXPORTS
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Enum for selecting computation backend.
enum GestureBackend {
  GestureBackendAuto = 0,  // default backend, use GPU on PC and CPU on Android, Recommended
  GestureBackendCPU = 1,   // use CPU, not supported on PC
  GestureBackendGPU = 2,   // use GPU, supported on PC/Android
};

// Enum for detection mode. Larger mode return more info, but runs more slowly. If a mode is not
// supported on a device, will fallback to previous supported mode.
enum GestureMode {
  GestureMode2DPoint = 0,   // Fastest mode, return one 2d point for hand
  GestureMode3DPoint = 1,   // Return one 3d point for hand, supported on Vive Pro and Focus
  GestureModeSkeleton = 2,  // Return skeleton (21 points) for hand, supported on Vive and Vive Pro
};

struct GestureOption {
  GestureBackend backend = GestureBackendAuto;
  GestureMode mode = GestureModeSkeleton;
};

// Enum for predefined gesture classification
enum GestureType {
  GestureTypeUnknown = 0,  // All other gestures not in predefined set
  GestureTypePoint = 1,
  GestureTypeFist = 2,
  GestureTypeOK = 3,
  GestureTypeLike = 4,
  GestureTypeFive = 5,
};

// Struct containing detection result for one hand
struct GestureResult {
  // Returns if this hand is left/right
  bool isLeft;
  // Returns position of the hand. The meaning of this field is different based on actual mode.
  // Index (3*i, 3*i+1, 3*i+2) composes a (x, y, z) point. There is total 21 points.
  // 2DPoint & 3DPoint3D: Only first point is used as the the position of hand.
  // Skeleton: An array of 21 keypoints of the hand, values (0, 0, 0) indicates invalid point
  // +x is right, +y is up, +z is front. Unit is meter.
  float points[21 * 3];
  // Returns pre-defined gesture type.
  GestureType gesture;
};

// Enum for possible errors in gesture detection
enum GestureFailure {
  GestureFailureNone = 0,        // No error occurs
  GestureFailureOpenCL = -1,     // (Only on Windows) OpenCL is not supported on the machine
  GestureFailureCamera = -2,     // Start camera failed
  GestureFailureInternal = -10,  // Internal errors
  GestureFailureCPUOnPC = -11,   // CPU backend is not supported on Windows
};

/** Start detection with given option, non-blocking.
 * params:
 *   option: (in & out) A pointer to GestureOption. Mode of option may be modified if requirements
 *           is not met on the device. The resulting mode is the actual mode used in detection.
 *           If option is null, default option will be used (auto backend + best mode).
 * return: error code, see GestureFailure
 */
INTERFACE_GESTURE_EXPORTS GestureFailure StartGestureDetection(GestureOption* option);

// Stop the detection. Blocking call until the pipeline is actually stopped.
INTERFACE_GESTURE_EXPORTS void StopGestureDetection();

/**
 * Get detection result in world coordinate. Returns at most one left and one right hand.
 * You should call this function periodically to get latest results. (60/30 FPS on Windows/Android)
 * params:
 *   points: (return value) A pointer to an array of GestureResult. The pointer is valid until next
 *           call to GetGestureResult or StopGestureDetection. The pointer need NOT to be freed.
 *   frameIndex: (return value) A pointer to frame index (can be null). This can be used to check if
 *               results are updated. Set to -1 if detection is not started or stopped due to error.
 * return: number of detections, at most 2.
 * example:
 * const GestureResult* points = NULL;
 * int frameIndex;
 * int size = GetGestureResult(&points, &frameIndex);
 */
INTERFACE_GESTURE_EXPORTS int GetGestureResult(const GestureResult** points, int* frameIndex);

// This function should be called before StartGestureDetection to indicate if caller is providing
// camera transform or not. Default is false. Call it after StartGestureDetection has no use.
// If set to true, the result points are in camera coordinate instead of world coordinate and caller
// is responsible for applying camera transform to the result points. This is useful if the camera
// position is different from OpenVR & WaveVR raw HMD data, useful for cases like teleporting.
// Currently, this is used in Unity & Unreal plugin
INTERFACE_GESTURE_EXPORTS void UseExternalTransform(bool value);

#ifdef __cplusplus
}
#endif

#endif
