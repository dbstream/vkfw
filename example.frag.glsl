/**
 * Example fragment shader.
 * Copyright (C) 2024  dbstream
 */

layout(location = 0) in vec4 color_;
layout(location = 0) out vec4 color;

void
main (void)
{
	color = color_;
}
