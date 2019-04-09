#version 410
uniform vec4 VertexRect = vec4(0,0,1,1);
in vec2 TexCoord;
out vec2 uv;

void main()
{
	gl_Position = vec4(TexCoord.x,TexCoord.y,0,1);
	
	float l = VertexRect[0];
	float t = VertexRect[1];
	float r = l+VertexRect[2];
	float b = t+VertexRect[3];
	
	l = mix( -1, 1, l );
	r = mix( -1, 1, r );
	t = mix( 1, -1, t );
	b = mix( 1, -1, b );
	
	gl_Position.x = mix( l, r, TexCoord.x );
	gl_Position.y = mix( t, b, TexCoord.y );
	
	uv = vec2( TexCoord.x, TexCoord.y );
}

