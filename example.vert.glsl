/**
 * Example vertex shader.
 * Copyright (C) 2024  dbstream
 */

layout (location = 0) out vec4 color;

vec4 positions[3] = {
	vec4 (0.0, -0.5, 0.0, 1.0),
	vec4 (0.5, 0.5, 0.0, 1.0),
	vec4 (-0.5, 0.5, 0.0, 1.0)
};

vec4 colors[3] = {
	vec4 (1.0, 0.0, 0.0, 1.0),
	vec4 (0.0, 1.0, 0.0, 1.0),
	vec4 (0.0, 0.0, 1.0, 1.0)
};

void
main (void)
{
	gl_Position = positions[gl_VertexIndex];
	color = colors[gl_VertexIndex];
}
