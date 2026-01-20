struct Bar1 {
    uint4 a;
    uint b[4];
    uint c;
};

struct Bar2 {
    uint4 d;
    uint e[4];
    uint f;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<Bar1> foo1;

[[vk::binding(0, 1)]]
RWStructuredBuffer<Bar2> foo2;

[shader("compute")]
void main() {
    foo1[0].b = foo2[0].e;
}