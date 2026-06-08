#version 450

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
};

void main() {
    float x[4];
    gl_Position = vec4(x[gl_VertexIndex % 4]);
    gl_PointSize = x[gl_InstanceIndex % 4];
}