const float PANE_REFLECTANCE = 0.08;
const float GLAZING_SPILLED_SUN = 0.03;
const float ROOM_SPILLED_SUN = 0.1;

struct Pane {
	vec3 room;
	vec3 film;
	float reflectance;
	float daylight;
	float sunbeam;
	vec3 lamp;
	vec3 sun;
	float interior;
	vec3 rim;
	float vista;
	float aperture;
};

vec3 PaneLight(Pane pane, vec3 reflection, vec3 normal, vec3 view, float roughness, float occlusion, Lighting lighting)
{
	float mirrored = SurroundingReflectance(Coat(roughness, pane.reflectance), max(dot(normal, view), 0.0));
	vec3 through = Diffuse(pane.room, pane.sun * (mix(GLAZING_SPILLED_SUN * pane.daylight, ROOM_SPILLED_SUN, pane.interior) + pane.sunbeam) + lighting.ambient * (occlusion * pane.aperture) * pane.daylight + pane.lamp, mirrored);
	vec3 surroundings = mix(pane.rim, reflection, pane.vista) * mirrored * occlusion;
	return through + pane.film * (surroundings + lighting.sun * Highlight(lighting.glare, pane.reflectance));
}
