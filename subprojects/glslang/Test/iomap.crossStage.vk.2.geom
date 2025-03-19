#version 460

layout(points) in;
layout(triangle_strip, max_vertices=3) out;

// Not written by vertex shader
in vec4 vgo1[];

in vec4 color[];

out vec4 colorOut;

in VV
{
	vec4 val;
} vv[];

out VV2
{
	vec4 vv2Val;
};

void main()
{
    for (int i = 0; i < 3; i++) {
        colorOut = color[i] * vv[i].val;
		vv2Val = vec4(1.0);
        EmitVertex();
    }
    EndPrimitive();
}

