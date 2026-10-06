uniform sampler2D u_current;
uniform sampler2D u_history;
uniform sampler2D u_depth;
uniform mat4 u_reprojection;
uniform float u_history_weight;

out vec4 frag_colour;

const float CLIP_SPREAD = 1.25;
const float MOTION_PIXELS_FOR_FRESH = 24.0;
const float MOVING_HISTORY_LOSS = 0.15;

vec3 YCoCg(vec3 rgb)
{
	return vec3(dot(rgb, vec3(0.25, 0.5, 0.25)), dot(rgb, vec3(0.5, 0.0, -0.5)), dot(rgb, vec3(-0.25, 0.5, -0.25)));
}

vec3 Rgb(vec3 ycocg)
{
	return vec3(ycocg.x + ycocg.y - ycocg.z, ycocg.x + ycocg.z, ycocg.x - ycocg.y - ycocg.z);
}

/* Bright samples weigh less, so a lone highlight cannot flicker through the blend. */
float Weight(vec3 colour)
{
	return 1.0 / (1.0 + Luminance(colour));
}

/* Where the point seen through this pixel stood on screen last frame. */
vec2 PreviousUv(vec2 uv, float depth)
{
	vec3 sight = SightAt(gl_FragCoord.xy);
	vec4 world = depth >= SKY_DEPTH ? vec4(sight, 0.0) : vec4(Eye() + sight * SightDistance(depth, sight), 1.0);
	vec4 clip = u_reprojection * world;
	return clip.xy / max(clip.w, 1e-6) * 0.5 + 0.5;
}

/* Last frame's picture read with a Catmull-Rom filter through five bilinear taps, which keeps it from softening frame on frame. */
vec3 History(vec2 uv)
{
	vec2 size = vec2(textureSize(u_history, 0));
	vec2 position = uv * size;
	vec2 centre = floor(position - 0.5) + 0.5;
	vec2 f = position - centre;
	vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
	vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
	vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
	vec2 w3 = f * f * (-0.5 + 0.5 * f);
	vec2 w12 = w1 + w2;
	vec2 at0 = (centre - 1.0) / size;
	vec2 at3 = (centre + 2.0) / size;
	vec2 at12 = (centre + w2 / w12) / size;

	vec3 sum = texture(u_history, vec2(at12.x, at0.y)).rgb * (w12.x * w0.y);
	sum += texture(u_history, vec2(at0.x, at12.y)).rgb * (w0.x * w12.y);
	sum += texture(u_history, at12).rgb * (w12.x * w12.y);
	sum += texture(u_history, vec2(at3.x, at12.y)).rgb * (w3.x * w12.y);
	sum += texture(u_history, vec2(at12.x, at3.y)).rgb * (w12.x * w3.y);
	float total = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
	return max(sum / total, 0.0);
}

/* The history is pulled toward the spread of colours around the pixel this frame, so what moved or came into view does not trail behind. */
vec3 Clipped(vec3 history, vec3 mean, vec3 spread)
{
	vec3 offset = YCoCg(history) - mean;
	vec3 reach = abs(offset) / max(spread, vec3(1e-5));
	float most = max(reach.x, max(reach.y, reach.z));
	return Rgb(most > 1.0 ? mean + offset / most : mean + offset);
}

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);
	ivec2 last = textureSize(u_current, 0) - 1;
	vec3 current = texelFetch(u_current, texel, 0).rgb;
	vec3 sum = vec3(0.0);
	vec3 squares = vec3(0.0);
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec3 tap = YCoCg(texelFetch(u_current, clamp(texel + ivec2(x, y), ivec2(0), last), 0).rgb);
			sum += tap;
			squares += tap * tap;
		}
	}
	vec3 mean = sum / 9.0;
	vec3 spread = sqrt(max(squares / 9.0 - mean * mean, 0.0)) * CLIP_SPREAD;

	vec2 uv = (vec2(texel) + 0.5) / vec2(textureSize(u_current, 0));
	vec2 previous = PreviousUv(uv, texelFetch(u_depth, texel, 0).r);
	float kept = u_history_weight;
	if (any(lessThan(previous, vec2(0.0))) || any(greaterThan(previous, vec2(1.0)))) kept = 0.0;
	float motion = length((previous - uv) * vec2(textureSize(u_current, 0)));
	kept *= 1.0 - MOVING_HISTORY_LOSS * clamp(motion / MOTION_PIXELS_FOR_FRESH, 0.0, 1.0);

	vec3 history = Clipped(History(previous), mean, spread);
	float current_weight = (1.0 - kept) * Weight(current);
	float history_weight = kept * Weight(history);
	frag_colour = vec4((current * current_weight + history * history_weight) / max(current_weight + history_weight, 1e-5), 1.0);
}
