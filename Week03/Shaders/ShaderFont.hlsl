cbuffer FConstants : register(b0)
{
    row_major float4x4 MVPMatrix;
};

struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.Position = mul(float4(input.Position, 1.0f), MVPMatrix);
    output.TexCoord = input.TexCoord;
    return output;
}

Texture2D FontTexture : register(t0);
SamplerState FontSampler : register(s0);

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 Color = FontTexture.Sample(FontSampler, input.TexCoord);
    clip(Color.a - 0.1f); // 완전 투명 픽셀 제거
    return Color;
}