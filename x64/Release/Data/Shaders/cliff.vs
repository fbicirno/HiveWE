#version 450 core

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec2 vUV;

layout (location = 2) uniform mat4 MVP;
layout (location = 3) uniform vec4 heights;

out vec2 UV;

void main() {
	float bottom = mix(heights[1], heights[0], -(vPosition.x / 128.f));
	float top = mix(heights[3], heights[2], -(vPosition.x / 128.f));
	float value = mix(bottom, top, vPosition.y / 128.f);

	gl_Position = MVP * vec4(vPosition.xy, vPosition.z + value * 128.f, 1);
	UV = vUV;
}