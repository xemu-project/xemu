#version 450
#extension GL_EXT_buffer_reference : enable

layout(buffer_reference, std430) buffer Node {
    uint payload;
};

layout(buffer_reference, std430) buffer BadNode {
    uint bad_payload;
};

layout(set = 0, binding = 0, std430) buffer SSBO {
    Node first_node;
    BadNode bad_node; // used
    uint placeholder;
} x;

void main() {
    x.placeholder = 0;
    x.first_node.payload = 3;
}
