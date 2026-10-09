uniform int u_way;

in vec3 v_position;
in vec3 v_normal;
in vec4 v_colour;
in float v_trait;

out vec4 frag_colour;

const vec3 CLEAR_FILM = vec3(1.0);
const float DAYLIT_CABIN = 1.0;
const vec3 UNLIT_CABIN = vec3(0.0);

vec4 Glazed(vec3 cabin, vec3 normal, vec3 view, float roughness, float sunlit)
{
	Pane pane = Pane(cabin, CLEAR_FILM, PANE_REFLECTANCE, DAYLIT_CABIN, UNLIT_CABIN);
	return PaneRadiance(pane, Surroundings(reflect(-view, normal)), normal, view, roughness, v_colour.a, sunlit);
}

void main()
{
	vec3 normal = normalize(v_normal);
	vec3 view = ToEye(v_position);
	vec3 albedo = Linear(Kept(v_colour.rgb, u_way));
	Coat coat = CoatOf(max(v_trait, 0.0));
	float sunlit = SunVisibility(v_position, normal);
	if (v_trait >= PANE_GLOSS) frag_colour = Glazed(albedo, normal, view, coat.roughness, sunlit);
	else frag_colour = SunlitRadiance(albedo, normal, view, coat, v_colour.a, 0.0, sunlit) + vec4(Glowing(albedo, v_trait), 0.0);
}
