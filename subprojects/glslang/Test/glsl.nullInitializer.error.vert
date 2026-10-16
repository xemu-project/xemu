#version 460
#extension GL_EXT_null_initializer : enable

// An empty initializer list ({}) is only valid as a whole null initializer.
// When one appears nested as an array element initializer there are no elements
// to size the array or derive its element type from, so it must be rejected
// rather than reading past the empty initializer sequence.

void main()
{
    int a[1][3] = { {} };    // ERROR, empty initializer list for an array element
}
