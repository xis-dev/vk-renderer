#version 450

// Switch to vertex pulling later
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aCol;

layout (location = 0) out vec3 vCol;
void main() {

    gl_Position = vec4(aPos, 1.0);
    vCol = aCol;
}