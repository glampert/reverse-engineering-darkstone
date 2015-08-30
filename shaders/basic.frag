
/*
 * GLSL Fragment Shader
 */

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_color;
layout(location = 2) in vec2 v_uv;

out vec4 out_color;

// These match the enum values declared in the app code.
const int RENDER_TEXTURED  = 0;
const int RENDER_WIREFRAME = 1;
const int RENDER_O3D_COLOR = 2;

uniform int u_render_mode_flag;
uniform sampler2D u_color_texture;

//
// Using the "barycentric coordinates" trick shown here:
//   http://codeflow.org/entries/2012/aug/02/easy-wireframe-display-with-barycentric-coordinates/
// To draw a wireframe outline around each solid triangle.
//
// The v_normal vector doesn't really carry a per vertex
// normal, but instead 0 or 1 for each element of each
// point in the triangle.
//
float edge_factor(void) {
	vec3 d  = fwidth(v_normal);
	vec3 a3 = smoothstep(vec3(0.0), d * 1.5, v_normal);
	return min(min(a3.x, a3.y), a3.z);
}

void main(void) {
	// Not a high performance app, so we can afford the branching.
	if (u_render_mode_flag == RENDER_TEXTURED) {
		// Texture map, if any.
		out_color = texture(u_color_texture, v_uv);
	} else if (u_render_mode_flag == RENDER_WIREFRAME) {
		// Dark green wireframe.
		out_color = vec4(0.8, 1.0, 0.8, 1.0);
	} else if (u_render_mode_flag == RENDER_O3D_COLOR) {
		// Vertex color from O3D file.
		out_color = vec4(v_color, 1.0);
	} else { // RENDER_DEFAULT_COLOR
		// This shows an outline around each triangle.
		out_color.rgb = mix(vec3(0.0), vec3(0.5), edge_factor());
		out_color.a = 1.0;
	}
}

