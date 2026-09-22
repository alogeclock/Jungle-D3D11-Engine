//#pragma once
//
//template<typename T>
//struct TTransform
//{
//protected:
//	TQuat<T>	Rotation;
//	FVector<T>	Translation;
//	FVector<T>	Scale3D;
//
//public:
//	TMatrix<T> ToMatrixWithScale() const
//	void SetLocation(const FVector<T>& Origin);
//	void SetRotation(const TQuat<T>& NewRotation);
//};
//
//template<typename T>
//inline TMatrix<T> TTransform<T>::ToMatrixWithScale() const
//{
//
//}
//
//template<typename T>
//inline void TTransform<T>::SetLocation(const FVector<T>& Origin)
//{
//	Translation = Origin;
//}
//
//template<typename T>
//inline void TTransform<T>::SetRotation(const TQuat<T>& NewRotation)
//{
//	Rotation = NewRotation;
//}
//
//
//
////using FTransform = TTransform<float>;
//using DTransform = TTransform<double>;