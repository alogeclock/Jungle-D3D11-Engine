cbuffer ConstantBuffer : register(b0)
{
    row_major float4x4 MVPMatrix;
};

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

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;

    output.position = mul(input.position, MVPMatrix);
    output.color = input.color;

    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    return input.color;
}