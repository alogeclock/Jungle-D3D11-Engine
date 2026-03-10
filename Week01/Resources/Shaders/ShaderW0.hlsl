cbuffer constants : register(b0)
{
    float3 Offset;
    float ScaleX;
    float2 ScreenSize;
    float ScaleY;
    float Angle;
    float ChargeSign;
    float3 Padding;
}

Texture2D SimpleTexture : register(t0);
SamplerState SimpleSampler : register(s0);

struct VS_INPUT
{
    float4 position : POSITION;
    float4 color : COLOR;
    float2 UV : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 UV : TEXCOORD0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    // 적용: Scale 및 Offset
    float4 worldPos = input.position;
    worldPos.x *= ScaleX;
    worldPos.y *= ScaleY;
    worldPos.xyz += Offset;

    // 2D NDC 변환 (Screen Space to NDC)
    // Screen (0,0) -> NDC (-1, 1)
    // Screen (Width, Height) -> NDC (1, -1)
    float x_ndc = (worldPos.x / ScreenSize.x) * 2.0f - 1.0f;
    float y_ndc = 1.0f - (worldPos.y / ScreenSize.y) * 2.0f;

    output.position = float4(x_ndc, y_ndc, 0.f, 1.f);
    output.color = input.color;
    output.UV = input.UV;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 color = SimpleTexture.Sample(SimpleSampler, input.UV);

    // 투명 픽셀은 버림
    if (color.a < 0.01f)
    {
        discard;
    }

    return color;
}
