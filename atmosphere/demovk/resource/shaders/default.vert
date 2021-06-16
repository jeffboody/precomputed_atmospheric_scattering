#version 450

layout(location=0) in vec4 vertex;

layout(std140, set=0, binding=0) uniform uniformMvp
{
	mat4 mvp;
};

layout(std140, set=0, binding=1) uniform uniformModelFromView
{
	mat4 model_from_view;
};

layout(std140, set=0, binding=2) uniform uniformViewFromClip
{
	mat4 view_from_clip;
};

layout(location=0) out vec3 view_ray;

void main()
{
	view_ray = (model_from_view*
	           vec4((view_from_clip*vertex).xyz, 0.0)).xyz;
	gl_Position = mvp*vertex;
}
