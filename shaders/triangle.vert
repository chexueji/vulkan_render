#version 450 core
layout(location = 0) in vec2 position;
layout(location = 1) in vec3 in_color;
layout(location = 0) out vec3 out_color;

void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    out_color = in_color;
}
