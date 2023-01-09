#version 410

uniform sampler2D PointCloudColour;

layout( location = 0 ) in vec2 TexCoord;

out vec4 FragColour;

void main() {
	FragColour = vec4(texture( PointCloudColour, TexCoord ).rgb,1);
}