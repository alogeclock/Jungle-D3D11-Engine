cbuffer ConstantBuffer : register(b0)
{
    row_major float4x4 worldMatrix;       // local -> world
    row_major float4x4 viewMatrix;        // world -> view
    row_major float4x4 projectionMatrix;        // world -> view
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
	
    float4 worldPos = mul(input.position, worldMatrix);
    float4 viewPos = mul(worldPos, viewMatrix);
    float4 projPos = mul(viewPos, projectionMatrix);
    output.position = projPos;
    output.color = input.color;
	
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return input.color;
}