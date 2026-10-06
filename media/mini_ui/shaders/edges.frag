uniform sampler2D u_current;

in vec2 v_uv;

out vec4 frag_colour;

const float EDGE_FLOOR = 0.0312;
const float EDGE_SHARE = 0.125;
const float SUBPIXEL_BLEND = 0.75;
const int SEARCH_STEPS = 10;
const float SEARCH_STRIDES[SEARCH_STEPS] = float[SEARCH_STEPS](1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 4.0, 8.0);

float LumaAt(vec2 uv)
{
	return PerceivedLuma(textureLod(u_current, uv, 0.0).rgb);
}

float LumaBeside(vec2 uv, ivec2 offset)
{
	return PerceivedLuma(textureLodOffset(u_current, uv, 0.0, offset).rgb);
}

/* Fast approximate antialiasing: an edge found across the pixel is followed along both ways to its ends,
 * and the pixel reads from across the edge by how near it lies to the end where the edge steps. */
void main()
{
	vec2 texel = 1.0 / vec2(textureSize(u_current, 0));
	vec3 colour = textureLod(u_current, v_uv, 0.0).rgb;
	float centre = PerceivedLuma(colour);
	float down = LumaBeside(v_uv, ivec2(0, -1));
	float up = LumaBeside(v_uv, ivec2(0, 1));
	float left = LumaBeside(v_uv, ivec2(-1, 0));
	float right = LumaBeside(v_uv, ivec2(1, 0));
	float lowest = min(centre, min(min(down, up), min(left, right)));
	float highest = max(centre, max(max(down, up), max(left, right)));
	float range = highest - lowest;
	if (range < max(EDGE_FLOOR, highest * EDGE_SHARE)) {
		frag_colour = vec4(colour, 1.0);
		return;
	}

	float down_left = LumaBeside(v_uv, ivec2(-1, -1));
	float up_right = LumaBeside(v_uv, ivec2(1, 1));
	float up_left = LumaBeside(v_uv, ivec2(-1, 1));
	float down_right = LumaBeside(v_uv, ivec2(1, -1));
	float down_up = down + up;
	float left_right = left + right;
	float left_corners = down_left + up_left;
	float down_corners = down_left + down_right;
	float right_corners = down_right + up_right;
	float up_corners = up_right + up_left;
	float edge_horizontal = abs(-2.0 * left + left_corners) + 2.0 * abs(-2.0 * centre + down_up) + abs(-2.0 * right + right_corners);
	float edge_vertical = abs(-2.0 * up + up_corners) + 2.0 * abs(-2.0 * centre + left_right) + abs(-2.0 * down + down_corners);
	bool horizontal = edge_horizontal >= edge_vertical;

	float luma_before = horizontal ? down : left;
	float luma_after = horizontal ? up : right;
	float gradient_before = luma_before - centre;
	float gradient_after = luma_after - centre;
	bool before_steepest = abs(gradient_before) >= abs(gradient_after);
	float gradient_scaled = 0.25 * max(abs(gradient_before), abs(gradient_after));
	float step_length = horizontal ? texel.y : texel.x;
	float local_average = 0.5 * ((before_steepest ? luma_before : luma_after) + centre);
	if (before_steepest) step_length = -step_length;

	vec2 along_uv = v_uv;
	if (horizontal) along_uv.y += step_length * 0.5;
	else along_uv.x += step_length * 0.5;
	vec2 stride = horizontal ? vec2(texel.x, 0.0) : vec2(0.0, texel.y);
	vec2 uv1 = along_uv - stride;
	vec2 uv2 = along_uv + stride;
	float end1 = LumaAt(uv1) - local_average;
	float end2 = LumaAt(uv2) - local_average;
	bool reached1 = abs(end1) >= gradient_scaled;
	bool reached2 = abs(end2) >= gradient_scaled;
	for (int i = 1; i < SEARCH_STEPS && !(reached1 && reached2); i++) {
		if (!reached1) {
			uv1 -= stride * SEARCH_STRIDES[i];
			end1 = LumaAt(uv1) - local_average;
			reached1 = abs(end1) >= gradient_scaled;
		}
		if (!reached2) {
			uv2 += stride * SEARCH_STRIDES[i];
			end2 = LumaAt(uv2) - local_average;
			reached2 = abs(end2) >= gradient_scaled;
		}
	}

	float distance1 = horizontal ? v_uv.x - uv1.x : v_uv.y - uv1.y;
	float distance2 = horizontal ? uv2.x - v_uv.x : uv2.y - v_uv.y;
	bool toward1 = distance1 < distance2;
	float nearest = min(distance1, distance2);
	float pixel_offset = 0.5 - nearest / (distance1 + distance2);
	bool steps_here = ((toward1 ? end1 : end2) < 0.0) != (centre < local_average);
	float offset = steps_here ? pixel_offset : 0.0;

	float neighbourhood = (2.0 * (down_up + left_right) + left_corners + right_corners) / 12.0;
	float subpixel = clamp(abs(neighbourhood - centre) / range, 0.0, 1.0);
	subpixel = (-2.0 * subpixel + 3.0) * subpixel * subpixel;
	offset = max(offset, subpixel * subpixel * SUBPIXEL_BLEND);

	vec2 final_uv = v_uv;
	if (horizontal) final_uv.y += offset * step_length;
	else final_uv.x += offset * step_length;
	frag_colour = vec4(textureLod(u_current, final_uv, 0.0).rgb, 1.0);
}
