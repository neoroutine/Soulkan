//we will be using glsl version 4.5 syntax
#version 460
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_ARB_gpu_shader_int64 : enable

struct Vertex
{
	vec4 position;
	vec4 color;
};

layout(buffer_reference, std430, buffer_reference_align = 32) readonly buffer Vertices
{
	Vertex v[];
};

layout(buffer_reference, std430, buffer_reference_align = 64) readonly buffer Matrices
{
	mat4 meshMatrices[];//Transform into array and use glBaseInstance to use each object own's matrix
};

layout(push_constant, std430) uniform Constants
{
	Vertices vertices;
	Matrices matrices;
	uint64_t matrixIndex;
} constants;

layout (location = 0) out vec4 outColor;

void main()
{
	//const array of positions for the triangle
	const vec4 positions[3] = vec4[3](
		vec4(1.f, 1.f, 0.f, 1.f),
		vec4(-1.f, 1.f, 0.f, 1.f),
		vec4(0.f, -1.f, 0.f, 1.f)
	);

	const vec4 colors[3] = vec4[3](
		vec4(1.f, 0.f, 0.f, 1.f),
		vec4(0.f, 1.f, 0.f, 1.f),
		vec4(0.f, 0.f, 1.f, 1.f)
	);

	//output the position of each vertex
	gl_Position = constants.matrices.meshMatrices[uint(constants.matrixIndex)] * constants.vertices.v[gl_BaseInstance + gl_VertexIndex].position;

	//output normal as color
	outColor = constants.vertices.v[gl_BaseInstance + gl_VertexIndex].color;
}