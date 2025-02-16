#version 460

out VV
{
	vec4 val;
};
out vec4 color;
void main()
{
	val = vec4(0.5);
    color = vec4(1.0);
    gl_Position = vec4(1.0);
}

