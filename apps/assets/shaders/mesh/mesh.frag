#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout(location = 0)in vec3 inNormal;
layout(location = 1)in vec3 inColor;
layout(location = 2)in vec2 inUV;
layout(location = 3)in vec2 in_mv;

layout(location = 4)in float in_depth;
layout(location = 5)in mat4 in_pre_vp;
layout(location = 9)in mat4 in_cur_vp;

layout(location = 0)out vec4 outFragColor;

void main()
{
  float lightValue = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1f);

  vec3 color = inColor * texture(colorTex, inUV).xyz;
  vec3 ambient = color * sceneData.ambientColor.xyz;

  outFragColor = vec4(color * lightValue * sceneData.sunlightColor.w + ambient , 1.0f);

  // outFragColor = vec4(sqrt(in_mv.x), sqrt(in_mv.y), 0.0f, 1.0f);
  // vec2 ss_coord = gl_FragCoord.xy / vec2(1920, 1080) * 2 - 1;
  // vec4 ss_cur_pos = vec4(ss_coord, in_depth, 1);
  // // vec4 ss_cur_pos = vec4(ss_coord, in_depth * 2 - 1, 1);
  // vec4 ss_pre_pos = in_pre_vp * inverse(in_cur_vp) * ss_cur_pos;
  // outFragColor = vec4(ss_pre_pos.xy, 0, 1.0);
}