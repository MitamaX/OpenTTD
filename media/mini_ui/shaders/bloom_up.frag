uniform sampler2D u_source;

in vec2 v_uv;

out vec4 frag_colour;

/* A three by three tent over the smaller level, added onto the level above. */
void main()
{
	vec2 texel = 1.0 / vec2(textureSize(u_source, 0));
	vec3 sum = textureLod(u_source, v_uv, 0.0).rgb * 4.0;
	sum += (textureLod(u_source, v_uv + vec2(-texel.x, 0.0), 0.0).rgb + textureLod(u_source, v_uv + vec2(texel.x, 0.0), 0.0).rgb) * 2.0;
	sum += (textureLod(u_source, v_uv + vec2(0.0, -texel.y), 0.0).rgb + textureLod(u_source, v_uv + vec2(0.0, texel.y), 0.0).rgb) * 2.0;
	sum += textureLod(u_source, v_uv - texel, 0.0).rgb + textureLod(u_source, v_uv + texel, 0.0).rgb;
	sum += textureLod(u_source, v_uv + vec2(-texel.x, texel.y), 0.0).rgb + textureLod(u_source, v_uv + vec2(texel.x, -texel.y), 0.0).rgb;
	frag_colour = vec4(sum / 16.0, 1.0);
}
