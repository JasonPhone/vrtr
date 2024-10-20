#version 450

layout(location = 0)in vec3 inColor;

// Corresponding to render attachments.
layout(location = 0)out vec4 outFragColor;

void main()
{
  //return red
  outFragColor = vec4(inColor, 1.0f);
}