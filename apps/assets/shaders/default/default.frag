#version 450

layout(binding = 1)uniform sampler2D tex_sampler;

layout(location = 0)in vec3 in_color;
layout(location = 1)in vec2 in_frag_tex_coord;

layout(location = 0)out vec4 out_final_color;

void main()
{
  // out_final_color = vec4(in_frag_tex_coord, 0.0, 1.0);
  out_final_color = texture(tex_sampler, in_frag_tex_coord);
}