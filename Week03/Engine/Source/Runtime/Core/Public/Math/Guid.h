#pragma once

#include "CoreTypes.h"

struct FGuid
{
public:
	FGuid()
		: A(0)
		, B(0)
		, C(0)
		, D(0)
	{
	}

	FGuid(uint32 InA, uint32 InB, uint32 InC, uint32 InD)
		: A(InA), B(InB), C(InC), D(InD)
	{
	}

	static FGuid NewGuid();

public:

	/** Holds the first component. */
	uint32 A;

	/** Holds the second component. */
	uint32 B;

	/** Holds the third component. */
	uint32 C;

	/** Holds the fourth component. */
	uint32 D;
};