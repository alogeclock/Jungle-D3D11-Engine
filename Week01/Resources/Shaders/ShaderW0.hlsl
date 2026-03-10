// ShaderW0.hlsl
// 스프라이트 렌더링 셰이더 (World-View-Projection + 스프라이트 아틀라스 UV)

// FSpriteConstants와 1:1 매칭되는 상수 버퍼
cbuffer SpriteConstants : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
    float2 sprite_size;
    float2 texture_size;
    float2 sprite_offset;
    float is_mirrored;
    float pad;
}

Texture2D sprite_texture : register(t0);
SamplerState sprite_sampler : register(s0);

struct VertexInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
};

struct PixelInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

PixelInput mainVS(VertexInput input)
{
    PixelInput output;

    output.position = mul(input.position, world);
    output.position = mul(output.position, view);
    output.position = mul(output.position, projection);

    // 좌우 반전 처리
    if (is_mirrored == 1.0f)
    {
        output.uv.x = 1.0f - input.uv.x;
        output.uv.y = input.uv.y;
    }
    else
    {
        output.uv = input.uv;
    }

    // 스프라이트 아틀라스 UV 변환
    output.uv *= sprite_size / texture_size;
    output.uv += sprite_offset / texture_size;

    return output;
}

float4 mainPS(PixelInput input) : SV_TARGET
{
    float4 color = sprite_texture.Sample(sprite_sampler, input.uv);

    // 텍스처가 바인딩되지 않았을 때 (alpha=0) 디버그용 폴백 색상
    // UV 좌표를 색상으로 표시하여 위치 확인 가능
    if (color.a < 0.01f)
    {
        color = float4(input.uv.x, 0.3f, input.uv.y, 1.0f);
    }

    return color;
}
