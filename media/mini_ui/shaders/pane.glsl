const float PANE_REFLECTANCE = 0.08;
const float ROOM_SUNLIT_SHARE = 0.03;

struct Pane {
	vec3 room;
	vec3 film;
	float reflectance;
	float daylight;
	float sunbeam;
	vec3 lamp;
};

vec3 PaneLight(Pane pane, vec3 reflection, vec3 normal, vec3 view, float roughness, float occlusion, Lighting lighting)
{
	float mirrored = SurroundingReflectance(Coat(roughness, pane.reflectance), max(dot(normal, view), 0.0));
	vec3 through = Diffuse(pane.room, lighting.sun * (ROOM_SUNLIT_SHARE * pane.daylight + pane.sunbeam) + lighting.ambient * occlusion * pane.daylight + pane.lamp, mirrored);
	vec3 surroundings = reflection * mirrored * occlusion;
	return through + pane.film * (surroundings + lighting.sun * Highlight(lighting.glare, pane.reflectance));
}
