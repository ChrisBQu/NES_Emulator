#version 330 core

// Screen vertex shader. Runs once for each corner of the quad the NES picture is drawn on.
//
// Inputs (given by the emulator):
//   a_position  the corner in clip space, from (-1, -1) at the bottom-left to (1, 1) at the top-right
//   a_texCoord  the matching spot in the NES picture, from (0, 0) at the top-left to (1, 1) at the bottom-right
//
// The quad covers the game picture's area of the window (not the black bars), so the picture fills clip space

in vec2 a_position;
in vec2 a_texCoord;

out vec2 v_texCoord;

void main() {
    v_texCoord = a_texCoord;
    gl_Position = vec4(a_position, 0.0, 1.0);
}