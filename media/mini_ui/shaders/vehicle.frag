uniform int u_way;

in vec3 v_position;
in vec3 v_normal;
in vec4 v_colour;
in float v_trait;

out vec4 frag_colour;

const vec3 CLEAR_FILM = vec3(1.0);
const float DAYLIT_CABIN = 1.0;
const float SHADED_CABIN = 0.0;
const vec3 UNLIT_CABIN = vec3(0.0);
const float SEALED_CABIN = 0.0;
const vec3 NO_RIM = vec3(0.0);
const float CLEAR_VISTA = 1.0;
const float OPEN_CABIN = 1.0;

Pane CabinBehind(vec3 cabin, vec3 sun)
{
	return Pane(cabin, CLEAR_FILM, PANE_REFLECTANCE, DAYLIT_CABIN, SHADED_CABIN, UNLIT_CABIN, sun, SEALED_CABIN, NO_RIM, CLEAR_VISTA, OPEN_CABIN);
}

void main()
{
	vec3 normal = normalize(v_normal);
	vec3 view = ToEye(v_position);
	vec3 albedo = Linear(Kept(v_colour.rgb, u_way));
	Coat coat = CoatOf(max(v_trait, 0.0));
	Lighting lighting = LightingOf(normal, view, coat.roughness, Sunlight(normal, FacingSunVisibility(v_position, normal)));
	vec3 reflection = Surroundings(reflect(-view, normal));
	vec3 light;
	if (v_trait >= PANE_GLOSS) light = PaneLight(CabinBehind(albedo, lighting.sun), reflection, normal, view, coat.roughness, v_colour.a, lighting);
	else light = SurfaceLight(albedo, reflection, normal, view, coat, v_colour.a, lighting, lighting.sun) + Glowing(albedo, v_trait);
	frag_colour = vec4(light, AmbientShare(lighting.ambient, lighting.sun));
}
