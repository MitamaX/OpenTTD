layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_normal;
layout(location = 2) in vec4 a_colour;
layout(location = 3) in vec4 a_glass;
layout(location = 4) in vec2 a_pattern;
layout(location = 5) in vec4 a_surface;

out vec3 v_position;
out vec3 v_normal;
out vec4 v_colour;
out vec3 v_film;
out vec3 v_transmission;
out vec2 v_pattern;
flat out uvec4 v_surface;
flat out uint v_seed;

invariant gl_Position;

const float BYTE_SCALE = 255.0;
const float LEAST_GLASS_LUMINANCE = 1e-4;
const float FILM_TINT = 0.1;
const float COATED_FILM_TINT = 0.2;
const float TRANSMISSION_TINT = 0.3;
const float COATED_TRANSMISSION_TINT = 0.4;
const float PANE_TRANSMITTANCE = 0.75;
const float COATED_TRANSMITTANCE = 0.4;

vec3 Tinted(vec3 hue, float share)
{
	return mix(vec3(1.0), hue, share);
}

void main()
{
	v_position = a_position;
	v_normal = a_normal.xyz;
	v_colour = a_colour;
	v_pattern = a_pattern;
	v_surface = uvec4(round(a_surface * BYTE_SCALE));
	v_seed = uint(round(a_glass.a * BYTE_SCALE));
	vec3 glass = Linear(a_glass.rgb);
	vec3 hue = glass / max(Luminance(glass), LEAST_GLASS_LUMINANCE);
	bool coated = v_surface.x == CLAD_GLASS;
	v_film = Tinted(hue, coated ? COATED_FILM_TINT : FILM_TINT);
	float transmittance = Landscape() == LANDSCAPE_TOYLAND ? 1.0 : coated ? COATED_TRANSMITTANCE : PANE_TRANSMITTANCE;
	v_transmission = Tinted(hue, coated ? COATED_TRANSMISSION_TINT : TRANSMISSION_TINT) * transmittance;
	gl_Position = ClipPosition(a_position);
}
