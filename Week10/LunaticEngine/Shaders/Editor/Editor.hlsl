#include "Common/ConstantBuffers.hlsli"
#include "Common/VertexLayouts.hlsli"

PS_Input_ColorWorld VS(VS_Input_PC input)
{
    PS_Input_ColorWorld output;

    float3 worldPos = input.position.xyz;
    output.worldPos = worldPos;

    float4 viewPos = mul(View, float4(worldPos, 1.0f));
    output.position = mul(Projection, viewPos);

    output.color = input.color;

    return output;
}

float4 PS(PS_Input_ColorWorld input) : SV_TARGET
{
    return input.color;
}
