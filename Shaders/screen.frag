#version 330 core

// Screen fragment shader. Runs once for every window pixel the NES picture covers, and decides its color.
// Effects like scanlines, CRT masks and color changes go here.
//
// Uniforms (set by the emulator every frame; leave out any you don't use):
//   u_texture     the NES picture, 256x240, nearest-neighbor filtered
//   u_sourceSize  the NES picture's size in pixels (256, 240)
//   u_outputSize  the size the picture is drawn at in the window, in window pixels
//   u_time        seconds since the emulator started, for animated effects

in vec2 v_texCoord;

out vec4 fragColor;

uniform sampler2D u_texture;
uniform vec2 u_sourceSize;
uniform vec2 u_outputSize;
uniform float u_time;

void main() {
    fragColor = texture(u_texture, v_texCoord);
}