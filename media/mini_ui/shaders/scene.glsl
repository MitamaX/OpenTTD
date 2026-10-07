layout(std140) uniform Scene {
	mat4 u_view;
	mat4 u_projection;
	mat4 u_view_projection;
	vec4 u_eye;
	vec4 u_sun;
	vec4 u_lens;
	vec4 u_world;
	vec4 u_screen;
	vec4 u_climate;
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

int Landscape()
{
	return int(u_climate.x);
}

/* The level above which arctic ground lies under snow. */
float SnowLine()
{
	return u_climate.y;
}

/* Whether a tile's ground is the one its climate lays a blanket of snow or sand over. */
bool Blanketed(uint material)
{
	return float(material) == u_climate.z;
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

/* How many screen pixels one tile spans this far from the eye. */
float TilePixelsAt(float distance)
{
	return u_projection[1][1] * u_screen.y * 0.5 / max(distance, u_lens.x);
}

/* A value in [0, 1) per pixel that neighbouring pixels spread evenly over, moving on each frame the picture is jittered so frames blend its steps away. */
float ScreenNoise(vec2 fragment)
{
	vec2 moved = fragment + 5.588238 * u_screen.w;
	return fract(52.9829189 * fract(dot(moved, vec2(0.06711056, 0.00583715))));
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

/* How far ahead of the eye, along the way it looks, the depth buffer puts a surface. */
float AheadOf(float depth)
{
	float near = u_lens.x;
	float far = u_lens.y;
	return 2.0 * near * far / (far + near - (depth * 2.0 - 1.0) * (far - near));
}

/* How far from the eye the depth buffer puts a surface along this sight line. */
float SightDistance(float depth, vec3 sight)
{
	vec3 back = vec3(u_view[0][2], u_view[1][2], u_view[2][2]);
	return AheadOf(depth) / max(dot(sight, -back), 1e-4);
}
