in vec2 fTexCoord;
uniform sampler2D Frame;
uniform vec2 Frame_PixelWidthHeight;


uniform float EdgeMinThreshold = 0.20;




const vec2 unshift = vec2(1.0 / 256.0, 1.0); ///< Value used to unpack 16 bit float data

const float atan0   = 0.414213;  ///< Support value for atan
const float atan45  = 2.414213;  ///< Support value for atan
const float atan90  = -2.414213; ///< Support value for atan
const float atan135 = -0.414213; ///< Support value for atan

/// Fast atan for canny usage.
vec2 atanForCanny(float x) {
	if (x < atan0 && x > atan135) {
		return vec2(1.0, 0.0);
	}
	if (x < atan90 && x > atan45) {
		return vec2(0.0, 1.0);
	}
	if (x > atan135 && x < atan90) {
		return vec2(-1.0, 1.0);
	}
	return vec2(1.0, 1.0);
}

/**
 * Function that performs canny edge detection.
 * @param coords Texture coordinates to analyize
 */
bool cannyEdge(vec2 coords) {
	
	float threshold = EdgeMinThreshold;
	
	float texWidth = 1.0f / Frame_PixelWidthHeight.x;
	float texHeight = 1.0f / Frame_PixelWidthHeight.y;

	vec4 color = texture2D(Frame, coords);
	color.z = dot(color.zw, unshift);
	
	// Thresholding
	if (color.z > threshold) {
		// Restore gradient directions.
		color.x -= 0.5;
		color.y -= 0.5;
		
		vec2 offset = atanForCanny(color.y / color.x);
		offset.x *= texWidth;
		offset.y *= texHeight;
		
		vec4 forward  = texture2D(Frame, coords + offset);
		vec4 backward = texture2D(Frame, coords - offset);
		// Uncompress mag data
		forward.z  = dot(forward.zw, unshift);
		backward.z = dot(backward.zw, unshift);
		
		// Check maximum.
		if (forward.z >= color.z ||
			backward.z >= color.z) {
			//return vec4(0.0, 0.0, 0.0, 1.0);
			return false;
		} else {
			color.x += 0.5; color.y += 0.5;
			//return vec4(1.0, color.x, color.y, 1.0);
			return true;
		}
	}
	return false;
}



bool IsEdge(vec2 uv)
{
	return cannyEdge(uv);
}
	
	
	
void main()
{
	bool Edge = IsEdge( fTexCoord );
	float v = Edge ? 1.0 : 0.0;
	gl_FragColor.x = 0;
	gl_FragColor.y = 0;
	gl_FragColor.z = v;
	gl_FragColor.w = 1.0;
}
