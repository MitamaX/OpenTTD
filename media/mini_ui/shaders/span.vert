layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_normal;
layout(location = 2) in vec4 a_colour;
layout(location = 3) in vec4 a_girth;

out vec3 v_position;
out vec3 v_normal;
out vec4 v_colour;
out float v_trait;

const float LEAST_HALF_PIXELS = 0.25;

/* A member thinner than half a pixel is drawn half a pixel wide, enough for every frame to light pixels along it, so the jittered frames blend it into a faint unbroken line rather than specks. */
void main()
{
	float least_half = LEAST_HALF_PIXELS / TilePixelsAt(distance(Eye(), a_position));
	v_position = a_position + a_girth.xyz * max(least_half - a_girth.w, 0.0);
	v_normal = a_normal.xyz;
	v_colour = a_colour;
	v_trait = a_normal.w;
	gl_Position = ClipPosition(v_position);
}
