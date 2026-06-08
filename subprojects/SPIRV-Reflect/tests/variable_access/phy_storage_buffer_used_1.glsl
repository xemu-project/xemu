#version 450
#extension GL_EXT_buffer_reference : enable

layout(buffer_reference) buffer Node;

layout(buffer_reference, std430) buffer Node {
    Node next;
    uint payload;
};

layout(buffer_reference) buffer BadNode;
layout(buffer_reference, std430) buffer BadNode {
    BadNode bad_next;
    uint bad_payload;
};

layout(set = 0, binding = 0, std430) buffer SSBO {
    Node first_node;
    BadNode bad_node; // used
    uint placeholder;
} x;

void main() {
    x.placeholder = 0;
    x.first_node.next.payload = 3;
}
