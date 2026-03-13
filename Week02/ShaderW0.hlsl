cbuffer ConstantBuffer : register(b0)
{
    float3 offset;
    float scale;
}

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

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
	
    float3 finalPos = (input.position.xyz * scale) + offset;
    output.position = float4(finalPos, 1.0f);
    output.color = input.color;
	
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return input.color;
}