#pragma once
template<typename T>
struct TQuat
{
public:
	/** The quaternion's X-component. */
	T X;

	/** The quaternion's Y-component. */
	T Y;

	/** The quaternion's Z-component. */
	T Z;

	/** The quaternion's W-component. */
	T W;
};

using FQuat = TQuat<float>;
using DQuat = TQuat<double>;