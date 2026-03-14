
//int32 FUObjectArray::AllocateSerialNumber(int32 Index)
//{
//	FUObjectItem* ObjectItem = IndexToObject(Index);
//	checkSlow(ObjectItem);
//
//	volatile int32* SerialNumberPtr = &ObjectItem->SerialNumber;
//	int32 SerialNumber = *SerialNumberPtr;
//	if (!SerialNumber)
//	{
//		// Open around PrimarySerialNumber. If we fail/abort a transaction, we don't need to undo this; we simply allow it to grow for the next use.
//		// Disable validation for this open as we're performing an explicitly
//		// recorded write to SerialNumber which the memory validator will treat
//		// as a false-positive mixed open / closed write.
//		UE_AUTORTFM_OPEN_NO_VALIDATION
//		{
//			SerialNumber = PrimarySerialNumber.Increment();
//			UE_CLOG(SerialNumber <= START_SERIAL_NUMBER, LogUObjectArray, Fatal, TEXT("UObject serial numbers overflowed (trying to allocate serial number %d)."), SerialNumber);
//			AutoRTFM::RecordOpenWrite(const_cast<int32*>(SerialNumberPtr)); // const_cast to remove volatile
//			int32 ValueWas = FPlatformAtomics::InterlockedCompareExchange((int32*)SerialNumberPtr, SerialNumber, 0);
//			if (ValueWas != 0)
//			{
//				// Someone else got it first; use their value.
//				SerialNumber = ValueWas;
//			}
//		};
//	}
//	checkSlow(SerialNumber > START_SERIAL_NUMBER);
//	return SerialNumber;
//}