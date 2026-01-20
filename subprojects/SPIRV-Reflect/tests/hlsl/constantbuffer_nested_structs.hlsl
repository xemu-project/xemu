struct Nested5 {
  float   d00;
  float   d01;
  float   d02;
  float   d03;
  float   d04;
  float   d05;
  float   d06;
  float   d07;
  float   d08;
  float   d09;
};


struct Nested4 {
  float   c00;
  float   c01;
  float   c02;
  float   c03;
  float   c04;
  Nested5 nested5_00;
  float   c05;
  float   c06;
  float   c07;
  float   c08;
  float   c09;
  Nested5 nested5_01;
};


struct Nested3 {
  float   b00;
  float   b01;
  float   b02;
  float   b03;
  float   b04;
  float   b05;
  float   b06;
  float   b07;
  float   b08;
  float   b09;
  Nested4 nested4_00;
};

struct Nested2 {
  Nested3 nested3_00;
  float   a00;
  float	  a01;
  float   a02;
  float   a03;
  float   a04;
  float   a05;
  float   a06;
  float   a07;
  float   a08;
  float   a09;
  Nested4 nested4_00;
};

struct Nested1 {
  Nested2 nested2_00;
  float   word00;
  float   word01;
  float   word02;
  float   word03;
  float   word04;
  float   word05;
  float   word06;
  float   word07;
  float   word08;
  float   word09;  
  Nested2 nested2_01;
};

struct Constants {
  float   var00;
  float2  var01;
  Nested1 nested1;
  float   Time;
};

ConstantBuffer<Constants> MyConstants : register(b0);

float4 main(float4 pos : POSITION) : SV_Position
{
  return float4(MyConstants.Time, 0, 0, 0);
}