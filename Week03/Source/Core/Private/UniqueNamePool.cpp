#include "Source/Core/Public/FName.h"
#include <Windows.h>


uint32 UniqueNamePool::AddNameAndCount(uint32 InOuterUUID, uint32 DisplayIndex)
{
    if (!UniqueNameSet.contains(InOuterUUID))
    {
        memset(&(UniqueNameSet[InOuterUUID]), 0, sizeof(TMap<uint32, uint32>));
    }

    TMap<uint32, uint32>& CountSet = UniqueNameSet[InOuterUUID];

    if (!CountSet.contains(DisplayIndex))
        CountSet[DisplayIndex] = 0;

    uint32& Count = CountSet[DisplayIndex];
    OutputDebugStringA("Hello Debug\n");
    return Count++;
}