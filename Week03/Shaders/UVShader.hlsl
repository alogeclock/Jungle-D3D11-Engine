cbuffer CBTransform : register(b0)
{
    matrix MVP;
};

Texture2D DiffuseTexture : register(t0);
SamplerState LinearSampler : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.Position = mul(float4(input.Position, 1.0f), MVP);
    output.UV = input.UV;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 texColor = DiffuseTexture.Sample(LinearSampler, input.UV);
    
    if (texColor.r <= 0.1f && texColor.g <= 0.1f && texColor.b <= 0.1f)
    {
        discard; // 검은색은 날림
    }
    
    return texColor;
}