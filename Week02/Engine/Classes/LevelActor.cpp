//#include "Level.h"
//#include "Engine.h"
//
//AActor* UWorld::SpawnActor(UClass* Class, DVector const* Location, DRotator const* Rotation, const FActorSpawnParameters& SpawnParameters)
//{
//	DTransform Transform;
//	if (Location)
//	{
//		Transform.SetLocation(*Location);
//	}
//	if (Rotation)
//	{
//		Transform.SetRotation(FQuat(*Rotation));
//	}
//
//	return SpawnActor(Class, &Transform, SpawnParameters);
//}
//
//
//
//
//#include "GameFramework/SpawnActorTimer.h"
//
//AActor* UWorld::SpawnActor(UClass* Class, FTransform const* UserTransformPtr, const FActorSpawnParameters& SpawnParameters)
//{
//	ULevel* CurrentLevel = PersistentLevel;
//
//	ULevel* LevelToSpawnIn = SpawnParameters.OverrideLevel;
//
//	// Use class's default actor as a template if none provided.
//	AActor* Template = SpawnParameters.Template ? SpawnParameters.Template : Class->GetDefaultObject<AActor>();
//
//	FName NewActorName = SpawnParameters.Name;
//
//	UPackage* ExternalPackage = nullptr;
//
//	FTransform const UserTransform = UserTransformPtr ? *UserTransformPtr : FTransform::Identity;
//
//	// Choose the collision handling method. In order of increasing priority: actor template setting (often class default), collision handling override, bNoFail
//	ESpawnActorCollisionHandlingMethod CollisionHandlingMethod = Template->SpawnCollisionHandlingMethod;
//
//	// Adopt collision handling override if any was set
//	if (SpawnParameters.SpawnCollisionHandlingOverride != ESpawnActorCollisionHandlingMethod::Undefined)
//	{
//		CollisionHandlingMethod = SpawnParameters.SpawnCollisionHandlingOverride;
//	}
//
//	EObjectFlags ActorFlags = SpawnParameters.ObjectFlags;
//
//	// actually make the actor object
//	AActor* const Actor = NewObject<AActor>(LevelToSpawnIn, Class, NewActorName, ActorFlags, Template, false/*bCopyTransientsFromClassDefaults*/, nullptr/*InInstanceGraph*/, ExternalPackage);
//
//	if (SpawnParameters.OverrideParentComponent)
//	{
//		FActorParentComponentSetter::Set(Actor, SpawnParameters.OverrideParentComponent);
//	}
//
//	if (SpawnParameters.CustomPreSpawnInitalization)
//	{
//		SpawnParameters.CustomPreSpawnInitalization(Actor);
//	}
//
//	Actor->PostSpawnInitialize(UserTransform, SpawnParameters.Owner, SpawnParameters.Instigator, SpawnParameters.IsRemoteOwned(), SpawnParameters.bNoFail, SpawnParameters.bDeferConstruction, SpawnParameters.TransformScaleMethod);
//
//	if (GIsEditor)
//	{
//		if (Actor->IsAsset() && Actor->GetPackage()->HasAnyPackageFlags(PKG_NewlyCreated))
//		{
//			FAssetRegistryModule::AssetCreated(Actor);
//		}
//
//		GEngine->BroadcastLevelActorAdded(Actor);
//	}
//
//	return Actor;
//}
