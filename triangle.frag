//glsl version 4.5
#version 450

//input color
layout (location = 0) in vec4 inColor;
layout (location = 1) in vec2 texCoords;

//output write
layout (location = 0) out vec4 outFragColor;

void main()
{
	
	outFragColor = vec4(texCoords, 0.5f, 1.0f);
}
