struct Bar {
    uint4 a;
    uint b[4];
    uint c;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<Bar> foo[8];

[shader("compute")]
void main() {
    foo[4][0].b[3] = 0;
}