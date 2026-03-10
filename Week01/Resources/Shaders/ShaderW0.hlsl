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
    float2 UV : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
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
    float x_ndc = (worldPos.x / ScreenSize.x) * 2.0f - 1.0f;
    float y_ndc = 1.0f - (worldPos.y / ScreenSize.y) * 2.0f;

    output.position = float4(x_ndc, y_ndc, 0.f, 1.f);
    output.UV = input.UV;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 color = SimpleTexture.Sample(SimpleSampler, input.UV);

    // 테스트를 위해 투명 픽셀 유기는 주석처리
    /*if (color.a < 0.01f)
    {
        discard;
    }*/

    return color;
}
