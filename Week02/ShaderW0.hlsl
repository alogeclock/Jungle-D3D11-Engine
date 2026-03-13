struct VS_INPUT
{
	float4 position : POSITION;
	float4 color : COLOR;
};

struct PS_INPUT
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

cbuffer constants : register(b0)
{
    float3 Offset;
    float Scale;
}

PS_INPUT mainVS(VS_INPUT input)
{
	PS_INPUT output;
	
  float3 finalPos = (input.position.xyz * Scale) + Offset;
  output.position = float4(finalPos, 1.0f);
	output.color = input.color;
	
	return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
	return input.color;
}