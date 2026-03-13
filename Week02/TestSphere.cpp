#include "Engine/Source/Runtime/Engine/Public/Rendering/Renderer.h"
#include "Engine/Source/Runtime/Core/Public/Math/Matrix.h"
#include "Engine/Source/Runtime/Core/Public/Math/TranslationMatrix.h"
#include "Engine/Source/Runtime/Core/Public/Math/ScaleMatrix.h"
#include "Engine/Source/Runtime/Core/Public/Math/RotationMatrix.h"


struct FTransform
{
	FVector4<float> Location;
	FVector4<float> Rotation;
    FVector4<float> Scale;
};

class USphere
{
public:
    FTransform Transform;

public:
    void Reset()
    {
        Transform.Location = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
        Transform.Rotation = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
        Transform.Scale = FVector4(0.2f, 0.2f, 0.2f, 1.0f);
    }

    USphere()
    {
        Reset();
    }

    ~USphere()
    {
    }

    void Update(float deltaTime)
    {
    }

    void Render(URenderer& renderer)
    {
		FConstants constants;
		constants.worldMatrix = (GetWorldMatrix(Transform));

        renderer.UpdateConstant(constants);
    }

    FMatrix<float> GetWorldMatrix(FTransform transform)
    {
        FMatrix S = FScaleMatrix<float>::FScaleMatrix(FVector(transform.Scale.X, transform.Scale.Y, transform.Scale.Z));
        FMatrix R = FRotationMatrix<float>::FRotationMatrix(transform.Rotation.X, transform.Rotation.Y, transform.Rotation.Z);
        FMatrix T = FTranslationMatrix<float>::FTranslationMatrix(FVector(transform.Location.X, transform.Location.Y, transform.Location.Z));
        return S * R * T;  // World Matrix
    }

    void SetTransform(FTransform transform) {
        Transform = transform;
    }

};