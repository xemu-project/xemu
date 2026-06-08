#version 450
layout(set = 0, binding = 0) buffer foo {
    uint a;
    uint b;
    uint c;
    uint d;
} bar[4];

void main() {
    bar[1].b = 0;
    bar[3].d = 0;
}