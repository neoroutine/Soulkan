//glsl version 4.5
#version 450

//input color
layout (location = 0) in vec4 inColor;

//output write
layout (location = 0) out vec4 outFragColor;

void main()
{
	
	outFragColor = inColor, 1.0f;
}
