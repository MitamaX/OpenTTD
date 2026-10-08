const float SKY_DEPTH = 1.0;

/* Brightness as the eye ranks it, squeezed so bright highlights do not swamp the edges between darker tones. */
float PerceivedLuma(vec3 colour)
{
	float luminance = Luminance(colour);
	return sqrt(luminance / (1.0 + luminance));
}

/* A point of the screen in view space, the eye at the origin looking down negative z. */
vec3 ViewPoint(vec2 uv, float depth)
{
	float ahead = AheadOf(depth);
	vec2 clip = uv * 2.0 - 1.0;
	return vec3(clip.x / u_projection[0][0] * ahead, clip.y / u_projection[1][1] * ahead, -ahead);
}

vec3 FetchViewPoint(sampler2D depths, ivec2 texel)
{
	vec2 uv = (vec2(texel) + 0.5) / vec2(textureSize(depths, 0));
	return ViewPoint(uv, texelFetch(depths, texel, 0).r);
}

/* Of the two neighbours along an axis, the one on the same surface: the nearer in depth. */
vec3 SurfaceStep(vec3 centre, vec3 before, vec3 after)
{
	vec3 forward = after - centre;
	vec3 backward = centre - before;
	return abs(forward.z) < abs(backward.z) ? forward : backward;
}

/* The surface's normal in view space, from the depths around a texel, facing the eye. */
vec3 ViewNormal(sampler2D depths, ivec2 texel)
{
	vec3 centre = FetchViewPoint(depths, texel);
	vec3 across = SurfaceStep(centre, FetchViewPoint(depths, texel - ivec2(1, 0)), FetchViewPoint(depths, texel + ivec2(1, 0)));
	vec3 up = SurfaceStep(centre, FetchViewPoint(depths, texel - ivec2(0, 1)), FetchViewPoint(depths, texel + ivec2(0, 1)));
	vec3 normal = normalize(cross(across, up));
	return dot(normal, centre) > 0.0 ? -normal : normal;
}
