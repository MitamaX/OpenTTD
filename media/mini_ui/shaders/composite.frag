uniform sampler2D u_colour;
uniform sampler2D u_depth;

out vec4 frag_colour;

const vec3 ZENITH = vec3(0.36, 0.54, 0.80);
const vec3 HORIZON = vec3(0.80, 0.86, 0.91);
const float SKY_CURVE = 0.55;
const float SHOULDER = 0.8;

/* The unit direction from the eye through a point of the screen, in clip units across and up. */
vec3 SightThrough(vec2 clip)
{
	vec3 right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
	vec3 up = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);
	vec3 back = vec3(u_view[0][2], u_view[1][2], u_view[2][2]);
	return normalize(right * (clip.x / u_projection[0][0]) + up * (clip.y / u_projection[1][1]) - back);
}

vec3 Sky(vec3 sight)
{
	return mix(HORIZON, ZENITH, pow(clamp(sight.z, 0.0, 1.0), SKY_CURVE));
}

/* How far from the eye the depth buffer puts the ground along this sight line. */
float Distance(float depth, vec3 sight)
{
	float near = u_lens.x;
	float far = u_lens.y;
	float ahead = 2.0 * near * far / (far + near - (depth * 2.0 - 1.0) * (far - near));
	vec3 back = vec3(u_view[0][2], u_view[1][2], u_view[2][2]);
	return ahead / max(dot(sight, -back), 1e-4);
}

float Fog(float distance)
{
	float t = clamp((distance - u_lens.z) / max(u_lens.w - u_lens.z, 1e-3), 0.0, 1.0);
	return t * (2.0 - t);
}

/* Colours up to the shoulder pass unchanged; brighter ones roll off toward white instead of clipping. */
vec3 Tonemap(vec3 colour)
{
	vec3 over = max(colour - SHOULDER, 0.0) / (1.0 - SHOULDER);
	vec3 rolled = SHOULDER + (1.0 - SHOULDER) * over / (1.0 + over);
	return mix(colour, rolled, step(SHOULDER, colour));
}

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);
	vec3 sight = SightThrough(gl_FragCoord.xy / u_screen.xy * 2.0 - 1.0);
	float depth = texelFetch(u_depth, texel, 0).r;
	vec3 colour = Sky(sight);
	if (depth < 1.0) colour = mix(texelFetch(u_colour, texel, 0).rgb, HORIZON, Fog(Distance(depth, sight)));
	frag_colour = vec4(Tonemap(colour), 1.0);
}
