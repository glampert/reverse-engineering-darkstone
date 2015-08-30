
/*
 * GLSL Vertex Shader
 */

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_color;
layout(location = 3) in vec2 a_uv;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec3 v_color;
layout(location = 2) out vec2 v_uv;

uniform mat4 u_mvp_matrix;

void main(void) {
	v_normal    = a_normal;
	v_color     = a_color;
	v_uv        = a_uv;
	gl_Position = vec4(u_mvp_matrix * vec4(a_position, 1.0));
}

