#ifndef SHADERS_H
#define SHADERS_H

const char *const vertex_shader_src =
R"(#version 460 core

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 v_tex_coords;

out vec2 tex_coords;

void main()
{
    gl_Position = vec4(pos, 0.0, 1.0);
    tex_coords = v_tex_coords;
})";

const char *const fragment_shader_src =
R"(#version 460 core

out vec4 frag_color;
in vec2 tex_coords;
uniform sampler2D tex_sampler;

void main()
{
    frag_color = texture(tex_sampler, tex_coords);
})";

#endif