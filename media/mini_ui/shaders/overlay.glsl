uniform int u_layer;
uniform float u_sink;

/* With a layer shown, everything else sinks to a dim grey. */
vec3 Greyed(vec3 colour)
{
	if (u_layer == LAYER_NONE) return colour;
	return vec3(GREY_FLOOR + dot(colour, LUMA_WEIGHTS) * (1.0 - u_sink));
}

vec3 Accent(int layer)
{
	return layer == LAYER_RAIL ? RAIL_ACCENT : ROAD_ACCENT;
}

/* What belongs to the shown layer keeps its own colour while the rest sinks. */
vec3 Kept(vec3 colour, int layer)
{
	return layer == u_layer ? colour : Greyed(colour);
}

/* What belongs to the shown layer takes its accent, keeping a little of its own shading. */
vec3 Overlaid(vec3 colour, int layer)
{
	if (u_layer == LAYER_NONE) return colour;
	if (layer != u_layer) return Greyed(colour);
	return Accent(layer) * (0.75 + 0.5 * dot(colour, LUMA_WEIGHTS));
}
