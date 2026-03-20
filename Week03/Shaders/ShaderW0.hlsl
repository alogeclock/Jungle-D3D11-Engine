cbuffer ConstantBuffer : register(b0)
{
    row_major float4x4 MVPMatrix;
}

cbuffer ConstantBufferColor : register(b1)
{
    float4 totalColor;
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
	
    float4 MVPPos = mul(input.position, MVPMatrix);
    output.position = MVPPos;
    output.color = input.color;

    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 finalColor = lerp(input.color, totalColor, totalColor.a);
    return finalColor;
}