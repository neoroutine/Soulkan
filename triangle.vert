//we will be using glsl version 4.5 syntax
#version 460
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_scalar_block_layout : enable

struct Vertex
{
	vec3 position;
	vec3 normal;
	vec2 uv;
};

layout(buffer_reference, scalar, buffer_reference_align = 32) readonly buffer Vertices
{
	Vertex v[];
};

layout(buffer_reference, std430, buffer_reference_align = 64) readonly buffer Matrices
{
	mat4 meshMatrices[];
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
	//output the position of each vertex
	mat4 currentMatrix = constants.matrices.meshMatrices[uint(constants.matrixIndex)];

	vec4 vertexPosition = vec4(constants.vertices.v[gl_BaseInstance + gl_VertexIndex].position, 1.f);
	vec4 vertexColor = vec4(constants.vertices.v[gl_BaseInstance + gl_VertexIndex].normal, 1.f);

	gl_Position = currentMatrix * vertexPosition;

	//output normal as color color
	outColor = vertexColor;
}