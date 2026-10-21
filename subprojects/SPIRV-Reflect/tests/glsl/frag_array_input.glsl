#version 450 core

struct structX
{
    int x1_;
    int x2_;
};

struct structY
{
    int y1_;
    structX y2_[2];
};

struct structZ
{
    structX z_[2][2];
};

layout(location = 0) flat in structY in_a;

layout(location = 5) flat in structX int_b[2];

layout(location = 10) in inC {
    structX c_[2];
} in_c;

layout(location = 14) flat in structX int_d[2][2];

layout(location = 22) in inE {
    structZ e_[2];
} in_e;

layout(location = 38) flat  in int in_f[2][2];

void main() { }
