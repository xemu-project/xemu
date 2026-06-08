struct ColorDesc {
	float 	R;
  float2  RG;
	float3	RGB;
	float4 	RGBA;
};

cbuffer MyParams : register(b0)
{
	float4x4 MvpMatrix;
	float4x4 NotUsedNormalMatrix;
	float3   Offset;
	float 	 ScalarScale;
	
	float2   Vector2ScaleX;
	float2   Vector2ScaleY;
	float2   Vector2ScaleXY;
	
	float2   Vector2ScaleXXXX;	
	float2   Vector2ScaleXYXY;	
	
	float3   Vector3ScaleX;
	float3   Vector3ScaleY;
	float3   Vector3ScaleZ;
	float3   Vector3ScaleXZ;
	float3   Vector3ScaleXYZ;
	
	float3   Vector3ScaleXX;	
	float3   Vector3ScaleYZX;		
	float3   Vector3ScaleZZZZ;		
	
	float4   Vector4ScaleX;
	float4   Vector4ScaleY;
	float4   Vector4ScaleZ;
	float4   Vector4ScaleW;
	float4   Vector4ScaleXY;
	float4   Vector4ScaleXZ;
	float4   Vector4ScaleYZ;
	float4   Vector4ScaleXZW;
	float4   Vector4ScaleYZW;
	float4   Vector4ScaleXYZW;
	
	float    NotUsed1;
	float2   NotUsed2;
	float3   NotUsed3;
	float3   MoreOffset;
	float2   NotUsed4;
	float3   NotUsed5;
	float3   LastOffset;
	float3   NotUsed6;
	
	float    ScalarArray[4];
	float2   Vector2Array[4];
	float3   Vector3Array[4];
	float4   Vector4Array[4];
	
	float2   Vector2ArrayX[4];
	float3   Vector3ArrayX[4];
	float4   Vector4ArrayX[4];	
	
	float2 	 NotUsedVectorArray[4];
	
	ColorDesc	ColorArray[8][7][6][5][4][3][2];
	float	    ScalarMultiDimArray[8][7][6][5][4][3][2];	
	float2    Vector2MultiDimArray[8][7][6][5][4][3][2];
	float3    Vector3MultiDimArray[8][7][6][5][4][3][2];
	float4    Vector4MultiDimArray[8][7][6][5][4][3][2];
	float2    Vector2MultiDimArrayX[8][7][6][5][4][3][2];
	float3    Vector3MultiDimArrayX[8][7][6][5][4][3][2];
	float4    Vector4MultiDimArrayX[8][7][6][5][4][3][2];
	float2    NotUsedVector2MultiDimArrayY[8][7][6][5][4][3][2];
	float3    NotUsedVector3MultiDimArrayY[8][7][6][5][4][3][2];
	float4    NotUsedVector4MultiDimArrayY[8][7][6][5][4][3][2];		
	float3    Vector3MultiDimArrayZ[8][7][6][5][4][3][2];
	float4    Vector4MultiDimArrayZ[8][7][6][5][4][3][2];		

	float2    Vector2MultiDimArrayXYX[8][7][6][5][4][3][2];
	float3    Vector3MultiDimArrayXYX[8][7][6][5][4][3][2];
	float4    Vector4MultiDimArrayXYX[8][7][6][5][4][3][2];
	
}

struct NestedUsedParams {
  float    NotUsed;
	float3   Offset;
	float4x4 NotUsedMatrix;
};

struct NestedNotUsedParams {
	float  NotUsed1;
	float2 NotUsed2;
	float3 NotUsed3;
};

struct UsedParams {
	float3              Position;
	float3              NotUsedColor;
	float3              Normal;
	NestedNotUsedParams NotUsedNested;
	NestedUsedParams    UsedNested;
	float               NotUsed1;
	ColorDesc	          ColorArray[4];	
};

struct NotUsedParams {
	float               NotUsed1;
	float2              NotUsed2;
	float3              NotUsed3;
	NestedNotUsedParams NotUsedNested;
};

struct UsedComponents {
  float3 ScaleByX;
};

struct Params2 {
	float4         PostTransformOffset;
	float          NotUsedScale;
	float3         Mask;
	UsedParams     Used;
	NotUsedParams  NotUsed;
	UsedComponents Components;
};

ConstantBuffer<Params2> MyParams2 : register(b1);

float4 main(float3 Position : Position) : SV_POSITION
{
	float4 result = mul(MvpMatrix, float4(Position + Offset, 1)) + MyParams2.PostTransformOffset;
	//float4 result = (float4)1;

	result.x *= ScalarScale;

	result.y  *= Vector2ScaleX.x;
	result.x  *= Vector2ScaleY.y;
	result.yx *= Vector2ScaleXY.xy;
	
	result.xyzw *= Vector2ScaleXXXX.xxxx;
	result.xyzw *= Vector2ScaleXYXY.xyxy;	
	
	result.z   *= Vector3ScaleX.x;
	result.y   *= Vector3ScaleY.y;	
	result.x   *= Vector3ScaleZ.z;	
	result.xy  *= Vector3ScaleXZ.xz;	
	result.xyz *= Vector3ScaleXYZ.xyz;
	
	result.xy   *= Vector3ScaleXX.xx;	
	result.xyz  *= Vector3ScaleYZX.yzx;		
	result.xyzw *= Vector3ScaleZZZZ.zzzz;		
	
	result.x    *= Vector4ScaleX.w;
	result.y    *= Vector4ScaleY.y;
	result.z    *= Vector4ScaleZ.z;	
	result.w    *= Vector4ScaleW.x;
	result.xy   *= Vector4ScaleXY.xy;
	result.xz   *= Vector4ScaleXZ.xz;
	result.xy   *= Vector4ScaleYZ.yz;
	result.xyz  *= Vector4ScaleXZW.xzw;
	result.yzw  *= Vector4ScaleYZW.yzw;
	result.xyzw *= Vector4ScaleXYZW.xyzw;		
	
	result      *= ScalarArray[0];
	result.xy   *= Vector2Array[1];
	result.xyz  *= Vector3Array[2];
	result.xyzw *= Vector4Array[3];
	
	result.x *= Vector2ArrayX[1].x;
	result.x *= Vector3ArrayX[2].x;
	result.x *= Vector4ArrayX[3].x;

	result.xyz  *= ColorArray[7][6][5][4][3][2][1].RGB;
	result.x    *= ScalarMultiDimArray[7][6][5][4][3][2][1];
	result.xy   *= Vector2MultiDimArray[7][6][5][4][3][2][1];
	result.xyz  *= Vector3MultiDimArray[7][6][5][4][3][2][1];
	result.xyzw *= Vector4MultiDimArray[7][6][5][4][3][2][1];
	result.xy   *= Vector2MultiDimArrayX[7][6][5][4][3][2][1].x;
	result.xyz  *= Vector3MultiDimArrayX[7][6][5][4][3][2][1].x;
	result.xyzw *= Vector4MultiDimArrayX[7][6][5][4][3][2][1].x;
	result.xyz  *= Vector3MultiDimArrayZ[7][6][5][4][3][2][1].z;
	result.xyzw *= Vector4MultiDimArrayZ[7][6][5][4][3][2][1].z;
	

	result.xyz  *= Vector2MultiDimArrayXYX[7][6][5][4][3][2][1].xyx;
	result.xyz  *= Vector3MultiDimArrayXYX[7][6][5][4][3][2][1].xyx;
	result.xyz  *= Vector4MultiDimArrayXYX[7][6][5][4][3][2][1].xyx;	
	
	result.xyz  *= MyParams2.Mask;
	result.xyz  *= MyParams2.Used.Position;
	result.xyz  += MyParams2.Used.Normal;
	result.xyz  += MoreOffset;
	result.xyz  += LastOffset;
	result.xyz  += MyParams2.Used.UsedNested.Offset;
	result.xyz  *= MyParams2.Components.ScaleByX.x;
	result.xyzw += MyParams2.Used.ColorArray[3].RGBA;
	result.y    *= MyParams2.Used.ColorArray[3].RGB.x;

	return result;
}