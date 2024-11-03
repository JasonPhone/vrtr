#version 450

layout(binding = 0)uniform SceneData {
  mat4 view;
  mat4 proj;
  mat4 model;
} scene_data;

layout(location = 0)in vec3 in_pos;
layout(location = 1)in vec3 in_color;
layout(location = 2)in vec2 in_tex_coord;

layout(location = 0)out vec3 out_frag_color;
layout(location = 1)out vec2 out_frag_tex_coord;

void main() {
  gl_Position = scene_data.proj * scene_data.view * scene_data.model * vec4(in_pos, 1.0);
  out_frag_color = in_color;
  out_frag_tex_coord = in_tex_coord;
}