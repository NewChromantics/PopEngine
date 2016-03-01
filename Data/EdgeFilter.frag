in vec2 fTexCoord;
uniform sampler2D Frame;
uniform vec2 Frame_PixelWidthHeight;


uniform float EdgeMinThreshold = 0.10;
uniform float MagicMult = 1.8;


// averaged pixel intensity from 3 color channels
float avg_intensity(in vec4 pix) {

	//	gr: currently using luma plane only
	//	return (pix.r + pix.g + pix.b)/3.0;
	return pix.r;
}

vec4 get_pixel(in vec2 coords, in float dx, in float dy)
{
	return texture2D(Frame,coords + vec2(dx, dy));
}

// returns pixel color
bool IsEdge(in vec2 coords){

	float Border = 0.01f;
	if ( coords.x < Border || coords.x > 1.0f-Border )
		return false;
	if ( coords.y < Border || coords.y > 1.0f-Border )
		return false;
	
	float dxtex = 1.0 / Frame_PixelWidthHeight.x;
	float dytex = 1.0 / Frame_PixelWidthHeight.y;
	float pix[9];
	int k = -1;
	float delta;
	
	// read neighboring pixel intensities
	for (int i=-1; i<2; i++) {
		for(int j=-1; j<2; j++) {
			k++;
			pix[k] = avg_intensity(get_pixel(coords,float(i)*dxtex,
											 float(j)*dytex));
		}
	}
	
	// average color differences around neighboring pixels
	delta = (abs(pix[1]-pix[7])+
			 abs(pix[5]-pix[3]) +
			 abs(pix[0]-pix[8])+
			 abs(pix[2]-pix[6])
			 )/4.0;
	
	
	float Score = MagicMult*delta;
	if ( Score < EdgeMinThreshold )
		return false;
	
	return true;
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
