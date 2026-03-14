#include "UObjectGlobals.h"

UObject* StaticAllocateObject(const UClass* InClass, UObject* InOuter, FName InName, EObjectFlags InFlags, EInternalObjectFlags InternalSetFlags, bool bCanRecycleSubobjects, bool* bOutRecycledSubobject, UPackage* ExternalPackage, int32 SerialNumber, FRemoteObjectId RemoteId, FGCReconstructionGuard* GCGuard)
{
	{
		LLM_SCOPE(ELLMTag::UObject);
		LLM_SCOPE_BYTAG(UObject_StaticAllocateObject);

		// report detailed UObject classes allocations
#if ENABLE_LOW_LEVEL_MEM_TRACKER && LLM_ALLOW_UOBJECTCLASSES_TAGS
		FString ClassNameString = FString::Printf(TEXT("UObject/%s"), *InClass->GetName());
		FName LLMScope_Name(*ClassNameString);

		LLM_SCOPE_DYNAMIC(LLMScope_Name, ELLMTracker::Default, ELLMTagSet::UObjectClasses, FLLMDynamicTagConstructorStatString(ClassNameString));
#endif // ENABLE_LOW_LEVEL_MEM_TRACKER

		SCOPE_CYCLE_COUNTER(STAT_AllocateObject);
		checkSlow(InOuter != INVALID_OBJECT); // not legal
		check(InClass && InClass->ClassWithin && InClass->ClassConstructor);

		const bool bCreatingCDO = (InFlags & RF_ClassDefaultObject) != 0 && (InFlags & RF_ImmutableDefaultObject) == 0;
		const bool bCreatingArchetype = (InFlags & RF_ArchetypeObject) != 0;
#if WITH_EDITOR
		if (GIsEditor)
		{
			if (StaticAllocateObjectErrorTests(InClass, InOuter, InName, InFlags))
			{
				return NULL;
			}
		}
		else
#endif // WITH_EDITOR
		{
			// In the editor these are handled inside StaticAllocateObjectErrorTests and they may be temporary warnings
			checkf(!FScopedAllowAbstractClassAllocation::IsDisallowedAbstractClass(InClass, InFlags), TEXT("Unable to create new object: %s %s.%s. Creating an instance of an abstract class is not allowed!"),
				*GetNameSafe(InClass), *GetPathNameSafe(InOuter), *InName.ToString());
			check(bCreatingCDO || bCreatingArchetype || !InOuter || InOuter->IsA(InClass->ClassWithin));
			check(InOuter || (InClass == UPackage::StaticClass() && InName != NAME_None)); // only packages can not have an outer, and they must be named explicitly	
		}

		checkf(!IsGarbageCollectingAndLockingUObjectHashTables(), TEXT("Unable to create new object: %s %s.%s. Creating UObjects while Collecting Garbage is not allowed!"),
			*GetNameSafe(InClass), *GetPathNameSafe(InOuter), *InName.ToString());

		if (bCreatingCDO)
		{
			check(InClass->GetClass());
			ensureMsgf(!GIsDuplicatingClassForReinstancing || InClass->HasAnyClassFlags(CLASS_Native), TEXT("GIsDuplicatingClassForReinstancing %d InClass %s"), (int)GIsDuplicatingClassForReinstancing, *InClass->GetPathName());
			InName = InClass->GetDefaultObjectName();
			// never call PostLoad on class default objects
			InFlags &= ~(RF_NeedPostLoad | RF_NeedPostLoadSubobjects);
		}

		UObject* Obj = NULL;
		if (InName == NAME_None)
		{
			UE_AUTORTFM_OPEN
			{
	#if WITH_EDITOR
				if (GOutputCookingWarnings && GetTransientPackage() != InOuter->GetOutermost())
				{
					InName = MakeUniqueObjectName(InOuter, InClass, NAME_UniqueObjectNameForCooking);
				}
				else
	#endif
				{
					InName = MakeUniqueObjectName(InOuter, InClass);
				}
			};
		}
		else
		{
			// See if object already exists.
			Obj = StaticFindObjectFastInternal( /*Class=*/ NULL, InOuter, InName, EFindObjectFlags::ExactClass);

			// It is an error if we are trying to replace an object of a different class
			if (Obj && !Obj->GetClass()->IsChildOf(InClass))
			{
				const TCHAR* ErrorPrefix = TEXT("");

				if (InClass->HasAnyClassFlags(CLASS_PerObjectConfig) && InOuter != nullptr && InOuter->GetOutermost() == GetTransientPackage())
				{
					ErrorPrefix = TEXT("PerObjectConfig object using the transient package, has triggered a name conflict and will now crash.\n"
						"To avoid this, don't use the transient package for PerObjectConfig objects.\n"
						"This has the side effect, of using the full path name for config ini sections. Use 'OverridePerObjectConfigSection' to keep the short name.\n\n");
				}

				// This generally happens when calling NewObject with a specific object name and an object already exists at the same path.
				// If the classes look the same but have different paths, an old version may have been renamed due to plugin unloading or class recompiling.
				// If the object has the garbage flag set, it was marked as ready to destroy but still exists so it cannot be reallocated before garbage collection clears it.
				UE_LOG(LogUObjectGlobals, Fatal,
					TEXT("%sCannot replace existing object of a different class.\n"
						"\tNew Object: %s %s.%s\n"
						"\tExisting Object: %s %s(0x%08x 0x%08x)"),
					ErrorPrefix, *InClass->GetPathName(), InOuter ? *InOuter->GetPathName() : TEXT(""), *InName.ToString(),
					*Obj->GetFullName(nullptr, EObjectFullNameFlags::IncludeClassPackage),
					Obj->HasAnyInternalFlags(EInternalObjectFlags::Garbage) ? TEXT("(garbage) ") : TEXT(""),
					(int32)Obj->GetFlags(), (int32)Obj->GetInternalFlags());
			}
		}

		FLinkerLoad* Linker = nullptr;
		int32			LinkerIndex = INDEX_NONE;
		bool			bWasConstructedOnOldObject = false;
		// True when the object to be allocated already exists and is a subobject.
		bool bSubObject = false;
		int32 TotalSize = InClass->GetPropertiesSize();
		checkSlow(TotalSize);

		int32 OldIndex = -1;
		int32 OldSerialNumber = 0;
#if UE_WITH_REMOTE_OBJECT_HANDLE
		FRemoteObjectId OldRemoteId;

		TryGetRemoteObjectOverrides(InName, InOuter, SerialNumber, RemoteId);
#endif

		if (Obj == nullptr)
		{
			int32 Alignment = FMath::Max(4, InClass->GetMinAlignment());
			Obj = (UObject*)GUObjectAllocator.AllocateUObject(TotalSize, Alignment, GIsInitialLoad);
		}
		else
		{
			// Replace an existing object without affecting the original's address or index.
			check(!Obj->IsUnreachable());

			check(!ObjectRestoreAfterInitProps); // otherwise recursive construction
			ObjectRestoreAfterInitProps = Obj->GetRestoreForUObjectOverwrite();

			// Remember linker, flags, index, and native class info.
			Linker = Obj->GetLinker();
			LinkerIndex = Obj->GetLinkerIndex();
			InternalSetFlags |= (Obj->GetInternalFlags() & (EInternalObjectFlags::Native | EInternalObjectFlags::RootSet | EInternalObjectFlags::LoaderImport));

			if (bCreatingCDO || !!(InFlags & RF_ImmutableDefaultObject))
			{
				check(Obj->HasAllFlags(RF_ClassDefaultObject));
				Obj->SetFlags(InFlags);
				Obj->SetInternalFlags(InternalSetFlags);
				// never call PostLoad on class default objects
				Obj->ClearFlags(RF_NeedPostLoad | RF_NeedPostLoadSubobjects);
			}
			else if (!InOuter || !InOuter->HasAnyFlags(RF_ClassDefaultObject))
			{
#if !UE_BUILD_SHIPPING
				// Handle nested DSOs
				bool bIsOwnedByCDOOrArchetype = false;
				UObject* Iter = InOuter;
				while (Iter)
				{
					if (Iter->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
					{
						bIsOwnedByCDOOrArchetype = true;
						break;
					}
					Iter = Iter->GetOuter();
				}

				// Should only get in here if we're NOT creating a subobject of a CDO.  CDO subobjects may still need to be serialized off of disk after being created by the constructor
				// if really necessary there was code to allow replacement of object just needing postload, but lets not go there unless we have to
				checkf(!Obj->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_ClassDefaultObject) || bIsOwnedByCDOOrArchetype,
					TEXT("Attempting to replace an object that hasn't been fully loaded: %s (Outer=%s, Flags=0x%08x)"),
					*Obj->GetFullName(),
					InOuter ? *InOuter->GetFullName() : TEXT("NULL"),
					(int32)Obj->GetFlags()
				);
#endif//UE_BUILD_SHIPPING
			}
			// Subobjects are always created in the constructor, no need to re-create them here unless their archetype != CDO or they're blueprint generated or they're remote
			if (!bCreatingCDO && (!bCanRecycleSubobjects || !Obj->IsDefaultSubobject()
#if UE_WITH_REMOTE_OBJECT_HANDLE
				|| UE::RemoteObject::Handle::IsRemote(Obj)
#endif // UE_WITH_REMOTE_OBJECT_HANDLE
				))
			{
				OldIndex = GUObjectArray.ObjectToIndex(Obj);
				OldSerialNumber = GUObjectArray.GetSerialNumber(OldIndex);
#if UE_WITH_REMOTE_OBJECT_HANDLE
				OldRemoteId = GUObjectArray.GetRemoteId(OldIndex);
#endif

				// It's not safe to access the object from other threads while it's being destroyed and reconstucted so simulate GC behavior and lock
				if (ensureMsgf(IsInGameThread(), TEXT("GC lock can only be acquired on the game thread. If you hit this ensure then an object is being reconstructed on a worker thread which is not thread-safe")) && GCGuard)
				{
					GCGuard->Lock();
				}

				// Destroy the object.
				SCOPE_CYCLE_COUNTER(STAT_DestroyObject);
				// Check that the object hasn't been destroyed yet.
				if (!Obj->HasAnyFlags(RF_FinishDestroyed))
				{
#if !UE_WITH_REMOTE_OBJECT_HANDLE
					if (FPlatformProperties::RequiresCookedData())
					{
						ensureAlwaysMsgf(!Obj->HasAnyFlags(RF_NeedLoad | RF_WasLoaded),
							TEXT("Replacing a loaded public object is not supported with cooked data: %s (Outer=%s, Flags=0x%08x, InternalObjectFlags=0x%08x)"),
							*Obj->GetFullName(),
							InOuter ? *InOuter->GetFullName() : TEXT("NULL"),
							(int32)Obj->GetFlags(),
							(int32)Obj->GetInternalFlags());
					}
#endif // !UE_WITH_REMOTE_OBJECT_HANDLE

					// Record the memory of the object before destruction, so aborting will return the
					// object to the previous object's state.
					UE_AUTORTFM_OPEN_NO_VALIDATION
					{
						// We disable validation on this write because this spans the entire object's memory
						// including fields that may be accessed only in the open (e.g. object flags which
						// are set atomically), and mixing closed writes with open writes will trigger the
						// memory validator.
	#if UE_STORE_OBJECT_LIST_INTERNAL_INDEX
						// ObjectListInternalIndex is an index into an array of UObjectHash's ClassToObjectListMap.
						// This index must be preserved, even if a reconstructed object is reverted as the
						// table is not instrumented by AutoRTFM.
						char* ObjectStart = reinterpret_cast<char*>(Obj);
						char* InternalIndexStart = reinterpret_cast<char*>(&Obj->GetObjectHashInternalIndex());
						char* InternalIndexEnd = InternalIndexStart + sizeof(Obj->GetObjectHashInternalIndex());
						char* ObjectEnd = ObjectStart + TotalSize;
						AutoRTFM::RecordOpenWriteNoMemoryValidation(ObjectStart, InternalIndexStart - ObjectStart);
						AutoRTFM::RecordOpenWriteNoMemoryValidation(InternalIndexEnd, ObjectEnd - InternalIndexEnd);
	#else
						AutoRTFM::RecordOpenWriteNoMemoryValidation(Obj, TotalSize);
	#endif
					};

					// Get the name before we start the destroy, as destroy renames it
					FString OldName = Obj->GetFullName();

					// Begin the asynchronous object cleanup.
					Obj->ConditionalBeginDestroy();

					bool bPrinted = false;
					double StallStart = 0.0;
					// Wait for the object's asynchronous cleanup to finish.
					while (!Obj->IsReadyForFinishDestroy())
					{
						// If we're not in the editor, and aren't doing something specifically destructive like reconstructing blueprints, this is fatal
						if (!bPrinted && !GIsEditor && FApp::IsGame() && !GIsReconstructingBlueprintInstances)
						{
							StallStart = FPlatformTime::Seconds();
							bPrinted = true;
						}
						FPlatformProcess::Sleep(0);
					}
					if (bPrinted)
					{
						const double ThisTime = FPlatformTime::Seconds() - StallStart;
						UE_LOG(LogUObjectGlobals, Warning, TEXT("Gamethread hitch waiting for resource cleanup on a UObject (%s) overwrite took %6.2fms. Fix the higher level code so that this does not happen."), *OldName, ThisTime * 1000.0);
					}
					// Finish destroying the object.
					Obj->ConditionalFinishDestroy();
				}
				TGuardValue<bool> _(GUObjectArray.bShouldRecycleObjectIndices, false);
				GUObjectArray.LockInternalArray();
				GUObjectArray.FreeUObjectIndex(Obj);
				GUObjectArray.UnlockInternalArray();
				Obj->~UObject();
				bWasConstructedOnOldObject = true;
			}
			else
			{
#if UE_WITH_REMOTE_OBJECT_HANDLE
				if (RemoteId.IsValid() && RemoteId != UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(Obj))
				{
					UE::CoreUObject::Private::FObjectHandleUtils::ChangeRemoteId(Obj, RemoteId);
				}
				if (SerialNumber != 0)
				{
					FUObjectItem* SubobjectItem = GUObjectArray.ObjectToObjectItem(Obj);
					checkf(SubobjectItem->SerialNumber == 0 || SerialNumber == 0 || SerialNumber == SubobjectItem->SerialNumber, TEXT("Attempting to change an existing subobject's %s serial number (%d -> %d)"), *Obj->GetPathName(), SubobjectItem->SerialNumber, SerialNumber);
					SubobjectItem->SerialNumber = SubobjectItem->SerialNumber == 0 ? SerialNumber : SubobjectItem->SerialNumber;
				}
#endif // UE_WITH_REMOTE_OBJECT_HANDLE
				bSubObject = true;
			}
		}

		// If class is transient, non-archetype objects must be transient.
		if (!bCreatingCDO && InClass->HasAnyClassFlags(CLASS_Transient) && !bCreatingArchetype)
		{
			InFlags |= RF_Transient;
		}

		if (!bSubObject)
		{
#if UE_WITH_REMOTE_OBJECT_HANDLE
			// We can't have two different, non-zero serial numbers
			checkf(OldSerialNumber == 0 || SerialNumber == 0 || SerialNumber == OldSerialNumber, TEXT("Attempting to change a new object's (%s %s.%s) serial number (%d -> %d)"),
				*InClass->GetPathName(), *GetPathNameSafe(InOuter), *InName.ToString(), OldSerialNumber, SerialNumber);
			checkf(!OldRemoteId.IsValid() || !RemoteId.IsValid() || OldRemoteId == RemoteId, TEXT("Attempting to change a new object's (%s %s.%s) remote id (%s -> %s)"),
				*InClass->GetPathName(), *GetPathNameSafe(InOuter), *InName.ToString(), *OldRemoteId.ToString(), *RemoteId.ToString());
#endif // UE_WITH_REMOTE_OBJECT_HANDLE

			UE_AUTORTFM_OPEN_NO_VALIDATION
			{
				FMemory::Memzero((void*)Obj, TotalSize);
				new ((void*)Obj) UObjectBase(const_cast<UClass*>(InClass), InFlags | RF_NeedInitialization, InternalSetFlags, InOuter, InName, OldIndex, OldSerialNumber == 0 ? SerialNumber : OldSerialNumber,
	#if UE_WITH_REMOTE_OBJECT_HANDLE
					OldRemoteId.IsValid() ? OldRemoteId :
	#endif
					RemoteId);
			};
			if (!bWasConstructedOnOldObject)
			{
				// If the object was freshly constructed inside a transaction, then
				// mark the AutoRTFMConstructionAborted bit if the transaction is
				// aborted. This is used to prevent aborted objects from being
				// returned from global lists, e.g. ForEachObjectOfClass().
				UE_AUTORTFM_ONABORT(Obj)
				{
					Obj->SetInternalFlags(EInternalObjectFlags::AutoRTFMConstructionAborted);
				};
			}
		}
		else
		{
			// Propagate flags to subobjects created in the native constructor.
			Obj->SetFlags(InFlags);
			Obj->SetInternalFlags(InternalSetFlags);
		}

		// if an external package was specified, assign it to the object
		if (ExternalPackage)
		{
			Obj->SetExternalPackage(ExternalPackage);
		}

		if (bWasConstructedOnOldObject)
		{
			// Reassociate the object with it's linker.
			Obj->SetLinker(Linker, LinkerIndex, false);
			if (Linker)
			{
				check(Linker->ExportMap[LinkerIndex].Object == NULL);
				Linker->ExportMap[LinkerIndex].Object = Obj;
			}
		}

		if (IsInAsyncLoadingThread() || IsInParallelLoadingThread())
		{
			FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
			if (ThreadContext.AsyncPackageLoader)
			{
				LLM_SCOPE(ELLMTag::AsyncLoading);
				ThreadContext.AsyncPackageLoader->NotifyConstructedDuringAsyncLoading(Obj, bSubObject);
			}
		}
		else
		{
			// Sanity checks for async flags.
			// It's possible to duplicate an object on the game thread that is still being referenced 
			// by async loading code or has been created on a different thread than the main thread.
			Obj->ClearInternalFlags(EInternalObjectFlags_AsyncLoading);
			if (Obj->HasAnyInternalFlags(EInternalObjectFlags::Async) && IsInGameThread())
			{
				Obj->ClearInternalFlags(EInternalObjectFlags::Async);
			}
		}


		// Let the caller know if a subobject has just been recycled.
		if (bOutRecycledSubobject)
		{
			*bOutRecycledSubobject = bSubObject;
		}

		return Obj;
	}
}
