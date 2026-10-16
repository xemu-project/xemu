#version 450

#extension GL_EXT_function_control_attributes : enable

layout(location = 0) out vec4 color;

[[inline]] int inlineFn(int i)
{
    return i + 1;
}

[[noinline]] int noinlineFn(int i)
{
    return i + 2;
}

int backFn(int i) [[inline]]
{
    return i + 3;
}

[[inline]] int prototypeFn (int i);

int prototypeFn(int i)
{
    return i + 4;
}

void main()
{
    int value = inlineFn(1);
    value += noinlineFn(value);
    value += prototypeFn(value);
    color = vec4(float(value));
}
