#version 410

layout( location = 0 ) in vec3 Position;
layout( location = 1 ) in vec2 UV;

layout( location = 0 ) out vec2 TexCoord;

uniform mat4 View;
uniform mat4 Model;

void main() {
	TexCoord = UV;

	vec3 P = Position * vec3(1,-1,1);

	gl_Position = View * Model * vec4(P,1);
}