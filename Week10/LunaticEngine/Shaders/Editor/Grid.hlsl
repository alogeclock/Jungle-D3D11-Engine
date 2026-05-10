#include "Common/ConstantBuffers.hlsli"

cbuffer GridBuffer : register(b2)
{
	float GridSize;
	float Range;
	float LineThickness;
	float MajorLineInterval;
	
	float MajorLineThickness;
	float MinorIntensity;
	float MajorIntensity;
	float MaxDistance;
	
	float AxisThickness;
	float AxisLength;
	float AxisIntensity;
	float GridPadding0;
	
	float4 GridCenter;
	float4 GridAxisA;
	float4 GridAxisB;
	
	float4 AxisColorA;
	float4 AxisColorB;
	float4 AxisColorN;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float3 WorldPos : TEXCOORD0;
	float2 LocalPos : TEXCOORD1;
	float DrawKind : TEXCOORD2;
};

float ComputeGridLineAlpha(float2 coord, float2 derivative, float lineThickness)
{
	const float2 grid = abs(frac(coord - 0.5f) - 0.5f) / max(derivative, 0.0001f.xx);
	const float lineDistance = min(grid.x, grid.y);
	return saturate(lineThickness - lineDistance);
}

float ComputeAxisSuppressionAlpha(float2 coord, float2 derivative, float lineThickness)
{
	const float2 axisDistance = abs(coord) / max(derivative, 0.0001f.xx);
	return saturate(lineThickness - min(axisDistance.x, axisDistance.y));
}

float ComputeSingleAxisAlpha(float coord, float derivative, float lineThickness)
{
	const float axisDistance = abs(coord) / max(derivative, 0.0001f);
	return saturate(lineThickness - axisDistance);
}

VSOutput VS(uint VertexID : SV_VertexID)
{
	static const float2 Positions[6] =
	{
		float2(-1.0f, -1.0f), float2( 1.0f, -1.0f), float2(-1.0f,  1.0f),
		float2( 1.0f, -1.0f), float2(-1.0f,  1.0f), float2( 1.0f,  1.0f),
	};
	
	VSOutput output;
	
	if (VertexID < 6)
	{
		const float2 localPos = Positions[VertexID];
		const float3 worldPos = GridCenter.xyz  + GridAxisA.xyz * (localPos.x * Range) + GridAxisB.xyz * (localPos.y * Range);
		output.WorldPos = worldPos;
		output.LocalPos = localPos;
		output.DrawKind = 0.0f;
		output.Position = mul(mul(float4(worldPos, 1.0f), View), Projection);
		return output;
	}
	
	const uint axisVertexID = VertexID - 6;
	const uint planeID = axisVertexID / 6;
	const uint localID = axisVertexID % 6;
	
	const float2 q = Positions[localID];
	const float axisT = q.x;
	const float stripHalfWidth = max(GridSize * 0.1f * max(AxisThickness, 1.0f), 0.01f);
	
	float3 worldPos = 0.0f.xxx;
	float axisCoord = 0.0f;
	
	if (planeID == 0)
	{
		worldPos = float3(0.0f, q.y * stripHalfWidth, axisT * AxisLength);
		axisCoord = worldPos.y;
	}
	else
	{
		worldPos = float3(q.y * stripHalfWidth, 0.0f, axisT * AxisLength);
		axisCoord = worldPos.x;
	}
	
	output.WorldPos = worldPos;
	output.LocalPos = float2(axisCoord, axisT);
	output.DrawKind = 1.0f;
	output.Position = mul(mul(float4(worldPos, 1.0f), View), Projection);
	return output;
}

float4 PS(VSOutput input) : SV_TARGET
{
	const float dist = distance(input.WorldPos, CameraWorldPos);
	if (dist < 0.5f)
	{
		discard;
	}
	
	if (input.DrawKind > 0.5f)
	{
		const float derivative = max(fwidth(input.LocalPos.x), 0.0001f);
		const float axisDrawWidth = derivative * max(AxisThickness * 3.0f, 1.0f);
		
		float axisAlpha = 1.0f - smoothstep(0.0f, axisDrawWidth, abs(input.LocalPos.x));
		axisAlpha *= 1.0f - smoothstep(0.85f, 1.0f, abs(input.LocalPos.y));
		
		const float fade = pow(saturate(1.0f - dist / MaxDistance), 2.0f);
		const float finalAlpha = axisAlpha * fade * AxisIntensity;
		
		if (finalAlpha < 0.01f)
		{
			discard;
		}
		
		return float4(AxisColorN.rgb, finalAlpha);
	}
	
	const float2 planeCoord = float2(dot(input.WorldPos, GridAxisA.xyz), dot(input.WorldPos, GridAxisB.xyz));
	const float2 minorCoord = planeCoord / GridSize;
	
	float2 minorDerivative = fwidth(input.LocalPos.xy) * (Range / GridSize);
	minorDerivative = max(minorDerivative, 0.0001f.xx);
	
	float minorAlpha = ComputeGridLineAlpha(minorCoord, minorDerivative, LineThickness);
	if (AxisThickness > 0.0f)
	{
		const float minorAxisSuppression =
			ComputeAxisSuppressionAlpha(minorCoord, minorDerivative, max(LineThickness, AxisThickness));
		minorAlpha *= (1.0f - minorAxisSuppression);
	}
	
	minorAlpha *= MinorIntensity;
	
	const float majorGridSize = GridSize * max(MajorLineInterval, 1.0f);
	const float2 majorCoord = planeCoord / majorGridSize;
	
	float2 majorDerivative = fwidth(input.LocalPos.xy) * (Range / majorGridSize);
	majorDerivative = max(majorDerivative, 0.0001f.xx);
	float majorAlpha = ComputeGridLineAlpha(majorCoord, majorDerivative, MajorLineThickness);
	
	if (AxisThickness > 0.0f)
	{
		const float majorAxisSuppression = ComputeAxisSuppressionAlpha(majorCoord, majorDerivative, max(MajorLineThickness, AxisThickness));
		majorAlpha *= (1.0f - majorAxisSuppression);
	}
	majorAlpha *= MajorIntensity;
	
	const float minorLodFade = saturate(0.8f - max(minorDerivative.x, minorDerivative.y));
	const float majorLodFade = saturate(1.2f - max(majorDerivative.x, majorDerivative.y));
	
	minorAlpha *= minorLodFade;
	majorAlpha *= majorLodFade;
	
	const float fade = pow(saturate(1.0f - dist / MaxDistance), 2.0f);
	const float gridAlpha = max(minorAlpha, majorAlpha) * fade;
	const float3 gridColor = lerp(float3(0.35f, 0.35f, 0.35f), float3(0.55f, 0.55f, 0.55f), saturate(majorAlpha));
	
	float axisAAlpha = 0.0f;
	float axisBAlpha = 0.0f;
	if (AxisThickness > 0.0f)
	{
		axisBAlpha = ComputeSingleAxisAlpha(minorCoord.x, minorDerivative.x, AxisThickness) * fade * AxisIntensity;
		axisAAlpha = ComputeSingleAxisAlpha(minorCoord.y, minorDerivative.y, AxisThickness) * fade * AxisIntensity;
	}
	
	const float finalAlpha = max(gridAlpha, max(axisAAlpha, axisBAlpha));
	if (finalAlpha < 0.01f)
	{
		discard;
	}
	
	const float totalWeight = gridAlpha + axisAAlpha + axisBAlpha;
	float3 color = gridColor;
	if (totalWeight > 0.0001f)
	{
		color = (gridColor * gridAlpha + AxisColorA.rgb * axisAAlpha + AxisColorB.rgb * axisBAlpha) / totalWeight;
	}
	
	return float4(color, finalAlpha);
}
