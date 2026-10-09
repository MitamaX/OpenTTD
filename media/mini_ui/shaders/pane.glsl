const float PANE_REFLECTANCE = 0.08;
const float ROOM_SUNLIT_SHARE = 0.12;

struct Pane {
	vec3 room;
	vec3 film;
	float reflectance;
	float daylight;
	vec3 lamp;
};

vec4 PaneRadiance(Pane pane, vec3 reflection, vec3 normal, vec3 view, float roughness, float occlusion, float sunlit)
{
	Coat coat = Coat(roughness, pane.reflectance);
	vec3 sun = Sunlight(normal, sunlit);
	vec3 ambient = AmbientLight(normal);
	float mirrored = SurroundingReflectance(coat, max(dot(normal, view), 0.0));
	vec3 through = Diffuse(pane.room, (sun * ROOM_SUNLIT_SHARE + ambient * occlusion) * pane.daylight + pane.lamp, mirrored);
	vec3 surroundings = reflection * mirrored * occlusion;
	return vec4(through + pane.film * (surroundings + sun * Highlight(normal, view, SunDirection(), coat)), AmbientShare(ambient, sun));
}
