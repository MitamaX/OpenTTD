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

const int CASCADES = 4;

layout(std140) uniform Shadows {
	mat4 u_cascades[CASCADES];
	mat4 u_caster;
	vec4 u_cascade_far;
	vec4 u_cascade_texel;
	vec4 u_shadow_fade;
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

vec3 SunDirection()
{
	return u_sun.xyz;
}

vec3 Eye()
{
	return u_eye.xyz;
}

vec3 RenderPoint(vec3 world)
{
	return vec3(world.xy, world.z * LevelRise());
}

/* A caster program draws into the bound shadow cascade instead of the camera's view. */
vec4 ClipPosition(vec3 render_point)
{
#ifdef SHADOW_CASTER
	return u_caster * vec4(render_point, 1.0);
#else
	return u_view_projection * vec4(render_point, 1.0);
#endif
}

/* The unit direction from the eye through a point of the screen, in clip units across and up. */
vec3 SightThrough(vec2 clip)
{
	vec3 right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
	vec3 up = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);
	vec3 back = vec3(u_view[0][2], u_view[1][2], u_view[2][2]);
	return normalize(right * (clip.x / u_projection[0][0]) + up * (clip.y / u_projection[1][1]) - back);
}

vec3 SightAt(vec2 fragment)
{
	return SightThrough(fragment / u_screen.xy * 2.0 - 1.0);
}

/* How far from the eye the depth buffer puts a surface along this sight line. */
float SightDistance(float depth, vec3 sight)
{
	float near = u_lens.x;
	float far = u_lens.y;
	float ahead = 2.0 * near * far / (far + near - (depth * 2.0 - 1.0) * (far - near));
	vec3 back = vec3(u_view[0][2], u_view[1][2], u_view[2][2]);
	return ahead / max(dot(sight, -back), 1e-4);
}
