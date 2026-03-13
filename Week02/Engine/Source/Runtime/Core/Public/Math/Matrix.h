#pragma once

struct FMatrix {
  float M[4][4];
  static const FMatrix Identity;
};

struct FVector {
  float X;
  float Y;
  float Z;

  float Dot(const FVector &rhs);
  FVector Cross(const FVector &rhs);
  float Length() const;
  void Normalize();
};

struct FVector4 {
  float X;
  float Y;
  float Z;
  float W;

  float Dot(const FVector4 &Other);
  float Length();
  float Length3();
};
