// ShaderW0.hlsl

cbuffer constants : register(b0)
{
    float3 Offset;
    float Scale;
    float Angle;
    float ChargeSign; // +1, -1, or 0 (자력 비활성화 시)
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

    // Z축 기준 2D 회전
    float cosA = cos(Angle);
    float sinA = sin(Angle);
    float3 rotated;
    rotated.x = input.position.x * cosA - input.position.y * sinA;
    rotated.y = input.position.x * sinA + input.position.y * cosA;
    rotated.z = input.position.z;

    output.position = float4(rotated * Scale + Offset, 1.0f);
    output.color = input.color;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 color = input.color;

    // 자력 극성에 따른 색상 틴트 (N극: 붉은 계열, S극: 푸른 계열)
    if (ChargeSign > 0.5f)
        color.rgb = color.rgb * 0.4f + float3(0.6f, 0.15f, 0.1f);
    else if (ChargeSign < -0.5f)
        color.rgb = color.rgb * 0.4f + float3(0.1f, 0.15f, 0.6f);

    return color;
}
