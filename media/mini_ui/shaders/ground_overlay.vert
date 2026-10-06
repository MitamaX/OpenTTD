uniform vec2 u_viewport;

layout(location = 0) in vec2 a_screen;
layout(location = 1) in vec4 a_colour;
layout(location = 2) in float a_ahead;

out vec4 v_colour;
out float v_inverse_ahead;

/* Positions are pixels of RmlUi's layer; the reciprocal of distance runs straight across the screen, so it carries the depth. */
void main()
{
	v_colour = a_colour;
	v_inverse_ahead = 1.0 / max(a_ahead, 1e-4);
	gl_Position = vec4(a_screen.x / u_viewport.x * 2.0 - 1.0, 1.0 - a_screen.y / u_viewport.y * 2.0, 0.0, 1.0);
}
