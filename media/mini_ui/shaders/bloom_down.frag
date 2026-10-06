uniform sampler2D u_source;
uniform bool u_first;

in vec2 v_uv;

out vec4 frag_colour;

const float BRIGHTEST = 256.0;

vec3 Tap(vec2 uv)
{
	return min(textureLod(u_source, uv, 0.0).rgb, vec3(BRIGHTEST));
}

vec3 Group(vec3 a, vec3 b, vec3 c, vec3 d)
{
	return (a + b + c + d) * 0.25;
}

/* On the first halving each group counts less the brighter it is, so a lone bright pixel cannot spread into a flickering blob. */
float GroupWeight(vec3 mean)
{
	return u_first ? 1.0 / (1.0 + Luminance(mean)) : 1.0;
}

/* Thirteen bilinear taps halve the picture: four overlapping boxes of four around the centre box, weighted so the result stays as bright as the source. */
void main()
{
	vec2 texel = 1.0 / vec2(textureSize(u_source, 0));
	vec3 a = Tap(v_uv + texel * vec2(-2.0, 2.0));
	vec3 b = Tap(v_uv + texel * vec2(0.0, 2.0));
	vec3 c = Tap(v_uv + texel * vec2(2.0, 2.0));
	vec3 d = Tap(v_uv + texel * vec2(-2.0, 0.0));
	vec3 e = Tap(v_uv);
	vec3 f = Tap(v_uv + texel * vec2(2.0, 0.0));
	vec3 g = Tap(v_uv + texel * vec2(-2.0, -2.0));
	vec3 h = Tap(v_uv + texel * vec2(0.0, -2.0));
	vec3 i = Tap(v_uv + texel * vec2(2.0, -2.0));
	vec3 j = Tap(v_uv + texel * vec2(-1.0, 1.0));
	vec3 k = Tap(v_uv + texel * vec2(1.0, 1.0));
	vec3 l = Tap(v_uv + texel * vec2(-1.0, -1.0));
	vec3 m = Tap(v_uv + texel * vec2(1.0, -1.0));

	vec3 groups[5] = vec3[5](Group(j, k, l, m), Group(a, b, d, e), Group(b, c, e, f), Group(d, e, g, h), Group(e, f, h, i));
	const float SHARES[5] = float[5](0.5, 0.125, 0.125, 0.125, 0.125);
	vec3 sum = vec3(0.0);
	float weights = 0.0;
	for (int n = 0; n < 5; n++) {
		float weight = SHARES[n] * GroupWeight(groups[n]);
		sum += groups[n] * weight;
		weights += weight;
	}
	frag_colour = vec4(sum / weights, 1.0);
}
