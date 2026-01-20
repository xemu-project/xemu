struct Bar {
    uint4 a;
    uint b[4];
    uint c;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<Bar> foo;

[shader("compute")]
void main() {
    uint d[4] = {4, 5, 6, 7};
    foo[0].b = d;
}