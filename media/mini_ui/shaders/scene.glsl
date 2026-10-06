layout(std140) uniform Scene {
	mat4 u_view;
	mat4 u_projection;
	mat4 u_view_projection;
	vec4 u_eye;
	vec4 u_sun;
	vec4 u_lens;
	vec4 u_world;
	vec4 u_screen;
};

vec2 MapSize()
{
	return u_world.xy;
}

float Peak()
{
	return u_world.z;
}

float LevelRise()
{
	return u_world.w;
}

float Clock()
{
	return u_screen.z;
}

float ReliefShare()
{
	return u_sun.w;
}

vec3 RenderPoint(vec3 world)
{
	return vec3(world.xy, world.z * LevelRise());
}
