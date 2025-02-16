#version 460

in vec4 unsetColor;
in vec4 colorOut;
out vec4 fragColor;

in Vertex
{
	vec2 val;
} iVert;

in Vertex2
{
	vec2 val2;
};

in vec4 vv2Val;

void main()
{
    fragColor = colorOut + unsetColor * vec4(iVert.val.xxyy) * vec4(val2.xxyy) * 
				vv2Val;
}

