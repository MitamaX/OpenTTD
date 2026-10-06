uniform sampler2D u_depth;

in vec4 v_colour;
in float v_inverse_ahead;

out vec4 frag_colour;

const float HIDDEN_SHARE = 0.35;
const float SLACK_TILES = 0.2;
const float SLACK_SHARE = 0.015;

/* Where the world stands in front of the shape by more than the ground's own unevenness, the shape shows only faintly through it. */
void main()
{
	float ahead = 1.0 / v_inverse_ahead;
	float scene = AheadOf(texelFetch(u_depth, ivec2(gl_FragCoord.xy), 0).r);
	bool hidden = ahead > scene + SLACK_TILES + ahead * SLACK_SHARE;
	float alpha = v_colour.a * (hidden ? HIDDEN_SHARE : 1.0);
	frag_colour = vec4(v_colour.rgb * alpha, alpha);
}
