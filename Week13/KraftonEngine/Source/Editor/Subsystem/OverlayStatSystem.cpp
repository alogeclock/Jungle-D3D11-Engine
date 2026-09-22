#include "Editor/Subsystem/OverlayStatSystem.h"

#include "Editor/EditorEngine.h"
#include "Engine/Component/ClothComponent.h"
#include "Engine/Component/ParticleSystemComponent.h"
#include "Engine/Component/SkeletalMeshComponent.h"
#include "Engine/Component/VehicleMovementComponent.h"
#include "Engine/GameFramework/AActor.h"
#include "Engine/GameFramework/World.h"
#include "Engine/Particles/Runtime/ParticleEmitterInstance.h"
#include "Engine/Profiling/Timer.h"
#include "Engine/Profiling/Stats.h"
#include "Engine/Profiling/MemoryStats.h"
#include "Engine/Profiling/ShadowStats.h"
#include "Engine/Profiling/SkinningStats.h"
#include "Engine/Profiling/GPUProfiler.h"
#include "Engine/Render/Types/RenderFeatureSettings.h"
#include "Slate/SWindow.h"
#include "ImGui/imgui.h"
#include <cstdio>
#include <cstring>

// 바이트 수를 적절한 단위(B / KB / MB / GB)로 변환해서 문자열로 만든다
static int FormatBytes(char* Buffer, int32 BufferSize, const char* Label, uint64 Bytes)
{
	const double B = static_cast<double>(Bytes);
	const double KB = B / 1024.0;
	const double MB = KB / 1024.0;
	const double GB = MB / 1024.0;

	if (GB >= 1.0)
		return snprintf(Buffer, BufferSize, "%s : %.2f GB", Label, GB);
	if (MB >= 1.0)
		return snprintf(Buffer, BufferSize, "%s : %.2f MB", Label, MB);
	if (KB >= 1.0)
		return snprintf(Buffer, BufferSize, "%s : %.2f KB", Label, KB);
	return snprintf(Buffer, BufferSize, "%s : %llu B", Label, static_cast<unsigned long long>(Bytes));
}

static int FormatBytesValue(char* Buffer, int32 BufferSize, uint64 Bytes)
{
	const double B = static_cast<double>(Bytes);
	const double KB = B / 1024.0;
	const double MB = KB / 1024.0;
	const double GB = MB / 1024.0;

	if (GB >= 1.0)
		return snprintf(Buffer, BufferSize, "%.2f GB", GB);
	if (MB >= 1.0)
		return snprintf(Buffer, BufferSize, "%.2f MB", MB);
	if (KB >= 1.0)
		return snprintf(Buffer, BufferSize, "%.2f KB", KB);
	return snprintf(Buffer, BufferSize, "%llu B", static_cast<unsigned long long>(Bytes));
}

static const FStatEntry* FindStatEntry(const char* Category, const char* Name)
{
	if (!Category || !Name)
	{
		return nullptr;
	}

	const TArray<FStatEntry>& Snapshot = FStatManager::Get().GetSnapshot();
	for (const FStatEntry& Entry : Snapshot)
	{
		if (!Entry.Name || !Entry.Category)
		{
			continue;
		}

		if (strcmp(Entry.Category, Category) == 0 && strcmp(Entry.Name, Name) == 0)
		{
			return &Entry;
		}
	}

	return nullptr;
}

static const FStatEntry* FindParticleStatEntry(const char* Name)
{
	return FindStatEntry("Particles", Name);
}

static void AddTimingRow(TArray<FStatRow>& Rows, const char* Label, float LastTimeMs, const FStatEntry* Entry)
{
	const float SafeLastTimeMs = LastTimeMs >= 0.0f
		? LastTimeMs
		: static_cast<float>((Entry ? Entry->LastTime : 0.0) * 1000.0);
	const float AverageTimeMs = static_cast<float>((Entry ? Entry->AvgTime : 0.0) * 1000.0);

	FStatRow Row;
	Row.Label = Label;
	char Buf[128] = {};
	snprintf(Buf, sizeof(Buf), "%.3f ms (2s avg %.3f)", SafeLastTimeMs, AverageTimeMs > 0.0f ? AverageTimeMs : SafeLastTimeMs);
	Row.Value = Buf;
	Rows.push_back(std::move(Row));
}

void FOverlayStatSystem::AppendLine(TArray<FOverlayStatLine>& OutLines, float Y, const FString& Text) const
{
	FOverlayStatLine Line;
	Line.Text = Text;
	Line.ScreenPosition = FVector2(Layout.StartX, Y);
	OutLines.push_back(std::move(Line));
}

void FOverlayStatSystem::RecordPickingAttempt(double ElapsedMs)
{
	LastPickingTimeMs = ElapsedMs;
	AccumulatedPickingTimeMs += ElapsedMs;
	++PickingAttemptCount;
}

static void AddRow(TArray<FStatRow>& Rows, const char* Label, const char* ValueFmt, ...)
{
	FStatRow Row;
	Row.Label = Label;
	char Buf[192] = {};
	va_list Args;
	va_start(Args, ValueFmt);
	vsnprintf(Buf, sizeof(Buf), ValueFmt, Args);
	va_end(Args);
	Row.Value = Buf;
	Rows.push_back(std::move(Row));
}

static void AddSectionHeader(TArray<FStatRow>& Rows, const char* Title)
{
	FStatRow Row;
	Row.Label = Title;
	Row.bIsSectionHeader = true;
	Rows.push_back(std::move(Row));
}

void FOverlayStatSystem::BuildFPSLines(const UEditorEngine& Editor, TArray<FStatRow>& OutRows) const
{
	const FTimer* Timer = Editor.GetTimer();
	if (Timer)
	{
		constexpr double FPSAverageWindowSeconds = 0.3;
		const double CurrentTime = Timer->GetTotalTime();

		if (!bFPSAverageInitialized)
		{
			FPSAverageWindowStartTime = CurrentTime;
			FPSAccumulatedFrameTimeMs = 0.0;
			FPSAccumulatedFrameCount = 0;
			bFPSAverageInitialized = true;
		}

		FPSAccumulatedFrameTimeMs += Timer->GetFrameTimeMs();
		++FPSAccumulatedFrameCount;

		const double WindowElapsed = CurrentTime - FPSAverageWindowStartTime;
		if (WindowElapsed >= FPSAverageWindowSeconds && FPSAccumulatedFrameCount > 0)
		{
			const float AverageMS = static_cast<float>(FPSAccumulatedFrameTimeMs / FPSAccumulatedFrameCount);
			const float AverageFPS = AverageMS > 0.0f ? 1000.0f / AverageMS : 0.0f;

			char Buffer[64] = {};
			snprintf(Buffer, sizeof(Buffer), "%.1f  (%.2f ms)", AverageFPS, AverageMS);
			CachedFPSLine = Buffer;

			FPSAverageWindowStartTime = CurrentTime;
			FPSAccumulatedFrameTimeMs = 0.0;
			FPSAccumulatedFrameCount = 0;
		}
	}
	else
	{
		CachedFPSLine = "0.0  (0.00 ms)";
		bFPSAverageInitialized = false;
		FPSAccumulatedFrameTimeMs = 0.0;
		FPSAccumulatedFrameCount = 0;
	}

	if (CachedFPSLine.empty())
		CachedFPSLine = "0.0  (0.00 ms)";

	FStatRow Row;
	Row.Label = "FPS";
	Row.Value = CachedFPSLine;
	OutRows.push_back(std::move(Row));

	if (bShowPickingTime)
	{
		AddRow(OutRows, "Picking Time", "%.5f ms", LastPickingTimeMs);
		AddRow(OutRows, "Num Attempts", "%d", static_cast<int32>(PickingAttemptCount));
		AddRow(OutRows, "Accumulated", "%.5f ms", AccumulatedPickingTimeMs);
	}
}

void FOverlayStatSystem::BuildMemoryLines(TArray<FStatRow>& OutRows) const
{
	char Buf[64] = {};

	AddRow(OutRows, "Allocation Count", "%u", MemoryStats::GetTotalAllocationCount());

	struct { const char* Label; uint64 Bytes; } MemEntries[] = {
		{ "Total Allocated",       MemoryStats::GetTotalAllocationBytes() },
		{ "PixelShader Memory",    MemoryStats::GetPixelShaderMemory() },
		{ "VertexShader Memory",   MemoryStats::GetVertexShaderMemory() },
		{ "VertexBuffer Memory",   MemoryStats::GetVertexBufferMemory() },
		{ "IndexBuffer Memory",    MemoryStats::GetIndexBufferMemory() },
		{ "StaticMesh CPU Memory", MemoryStats::GetStaticMeshCPUMemory() },
		{ "Texture Memory",        MemoryStats::GetTextureMemory() },
	};

	for (const auto& Entry : MemEntries)
	{
		FormatBytesValue(Buf, sizeof(Buf), Entry.Bytes);
		FStatRow Row;
		Row.Label = Entry.Label;
		Row.Value = Buf;
		OutRows.push_back(std::move(Row));
	}
}

void FOverlayStatSystem::BuildShadowLines(TArray<FStatRow>& OutRows) const
{
#if STATS
	char Buf[64] = {};

	FormatBytesValue(Buf, sizeof(Buf), FShadowStats::ShadowMapMemoryBytes);
	AddRow(OutRows, "Shadow Map Memory", "%s", Buf);

	const TArray<FStatEntry>& GPUSnapshot = FGPUProfiler::Get().GetGPUSnapshot();
	double ShadowGpuMs = 0.0;
	for (const FStatEntry& Entry : GPUSnapshot)
	{
		if (Entry.Name && strcmp(Entry.Name, "ShadowMapPass") == 0)
		{
			ShadowGpuMs = Entry.LastTime * 1000.0;
			break;
		}
	}
	AddRow(OutRows, "Shadow GPU Time",   "%.3f ms", ShadowGpuMs);
	AddRow(OutRows, "Shadow Draw Calls", "%u", FShadowStats::ShadowDrawCallCount);
	AddRow(OutRows, "Casters Spot",      "%u", FShadowStats::SpotLightCasterCount);
	AddRow(OutRows, "Casters Point",     "%u", FShadowStats::PointLightCasterCount);
	AddRow(OutRows, "Casters Dir",       "%u", FShadowStats::DirectionalLightCasterCount);
	AddRow(OutRows, "Lights Spot",       "%u", FShadowStats::SpotLightShadowCount);
	AddRow(OutRows, "Lights Point",      "%u", FShadowStats::PointLightShadowCount);
	AddRow(OutRows, "Lights Dir",        "%u", FShadowStats::DirectionalLightShadowCount);
	AddRow(OutRows, "CSM Resolution",    "%ux%u", FShadowStats::ShadowMapResolution, FShadowStats::ShadowMapResolution);
#else
	AddRow(OutRows, "Shadow", "unavailable (STATS=0)");
#endif
}

void FOverlayStatSystem::BuildSkinningLines(TArray<FStatRow>& OutRows) const
{
#if STATS
	const ESkinningMode SkinningMode = FRenderFeatureSettings::Get().GetSkinningMode();
	AddRow(OutRows, "Skinning Mode", "%s", SkinningMode == ESkinningMode::GPU ? "GPU" : "CPU");

	const char* StatNames[] =
	{
		"UpdateSkinMatrices",
		"Prepare Buffer(CPU)",
		"Prepare Buffer(GPU)",
	};

	const TArray<FStatEntry>& Snapshot = FStatManager::Get().GetSnapshot();
	for (const char* StatName : StatNames)
	{
		const FStatEntry* FoundEntry = nullptr;
		for (const FStatEntry& Entry : Snapshot)
		{
			const bool bSkinningCategory = Entry.Category && strcmp(Entry.Category, "Skinning") == 0;
			const bool bNameMatch = Entry.Name && strcmp(Entry.Name, StatName) == 0;
			if (bNameMatch && bSkinningCategory)
			{
				FoundEntry = &Entry;
				break;
			}
		}
		AddRow(OutRows, StatName, "%.3f ms  (2s avg %.3f)",
			FoundEntry ? FoundEntry->LastTime * 1000.0 : 0.0,
			FoundEntry ? FoundEntry->AvgTime * 1000.0 : 0.0);
	}

	char UsedBytesText[64] = {};
	FormatBytesValue(UsedBytesText, sizeof(UsedBytesText), SkinningStats::GetSkinMatrixUsedBytes());
	AddRow(OutRows, "SkinMatrix Buffer", "%u matrices  %s", SkinningStats::GetSkinMatrixUsedCount(), UsedBytesText);

	char CPUSkinnedBytesText[64] = {};
	FormatBytesValue(CPUSkinnedBytesText, sizeof(CPUSkinnedBytesText), SkinningStats::GetCPUSkinnedMeshVertexBytes());
	AddRow(OutRows, "CPU Skinning Verts", "%u verts  %s", SkinningStats::GetCPUSkinnedMeshVertexCount(), CPUSkinnedBytesText);
#else
	AddRow(OutRows, "Skinning", "unavailable (STATS=0)");
#endif
}

void FOverlayStatSystem::BuildParticleLines(const UEditorEngine& Editor, TArray<FStatRow>& OutRows) const
{
#if STATS
	int32 ComponentCount = 0;
	int32 ActiveComponentCount = 0;
	int32 TotalEmitterCount = 0;
	int32 ActiveEmitterCount = 0;
	int32 ActiveParticleCount = 0;
	int32 MaxParticleCount = 0;
	int32 DrawBatchCount = 0;
	int32 SpawnedThisFrame = 0;
	int32 KilledThisFrame = 0;

	if (UWorld* World = Editor.GetWorld())
	{
		for (AActor* Actor : World->GetActors())
		{
			if (!Actor)
				continue;

			for (UActorComponent* Component : Actor->GetComponents())
			{
				UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(Component);
				if (!PSC)
					continue;

				++ComponentCount;
				if (PSC->IsActive())
					++ActiveComponentCount;

				ActiveParticleCount += PSC->GetTotalActiveParticles();
				SpawnedThisFrame    += PSC->GetTotalSpawnedThisFrame();
				KilledThisFrame     += PSC->GetTotalKilledThisFrame();
				DrawBatchCount      += static_cast<int32>(PSC->GetEmitterRenderData().size());

				for (const FParticleEmitterInstance* Inst : PSC->GetEmitterInstances())
				{
					if (!Inst)
						continue;
					++TotalEmitterCount;
					MaxParticleCount += Inst->MaxActiveParticles;
					if (Inst->GetActiveParticleCount() > 0)
						++ActiveEmitterCount;
				}
			}
		}
	}

	// --- State ---
	AddSectionHeader(OutRows, "State");
	AddRow(OutRows, "Components", "%d total  %d active", ComponentCount, ActiveComponentCount);
	AddRow(OutRows, "Emitters",   "%d active / %d total", ActiveEmitterCount, TotalEmitterCount);
	AddRow(OutRows, "Particles",  "%d alive / %d max", ActiveParticleCount, MaxParticleCount);
	AddRow(OutRows, "DrawBatches","%d", DrawBatchCount);

	// --- Frame ---
	AddSectionHeader(OutRows, "Frame");
	AddRow(OutRows, "Spawned", "%d / frame", SpawnedThisFrame);
	AddRow(OutRows, "Killed",  "%d / frame", KilledThisFrame);
	AddRow(OutRows, "Events",  "%d / frame", 0); // FrameEventQueue는 component별이나 레벨 합산용 접근자 없음

	// --- CPU Time ---
	AddSectionHeader(OutRows, "CPU Time");
	const FStatEntry* TickEntry          = FindParticleStatEntry("ParticleSystemComponent::Tick");
	const FStatEntry* SimulateEntry      = FindParticleStatEntry("ParticleEmitters::Simulate");
	const FStatEntry* ProcessEventsEntry = FindParticleStatEntry("ParticleEmitters::ProcessEvents");
	const FStatEntry* BuildEntry         = FindParticleStatEntry("ParticleSystemComponent::BuildRenderData");

	AddRow(OutRows, "Tick",            "%.3f ms  (2s avg %.3f)",
		TickEntry ? TickEntry->LastTime * 1000.0 : 0.0,
		TickEntry ? TickEntry->AvgTime * 1000.0 : 0.0);
	AddRow(OutRows, "Simulate",        "%.3f ms  (2s avg %.3f)",
		SimulateEntry ? SimulateEntry->LastTime * 1000.0 : 0.0,
		SimulateEntry ? SimulateEntry->AvgTime * 1000.0 : 0.0);
	AddRow(OutRows, "ProcessEvents",   "%.3f ms  (2s avg %.3f)",
		ProcessEventsEntry ? ProcessEventsEntry->LastTime * 1000.0 : 0.0,
		ProcessEventsEntry ? ProcessEventsEntry->AvgTime * 1000.0 : 0.0);
	AddRow(OutRows, "BuildRenderData", "%.3f ms  (2s avg %.3f)",
		BuildEntry ? BuildEntry->LastTime * 1000.0 : 0.0,
		BuildEntry ? BuildEntry->AvgTime * 1000.0 : 0.0);
#else
	AddRow(OutRows, "Particles", "unavailable (STATS=0)");
#endif
}

void FOverlayStatSystem::BuildVehicleLines(const UEditorEngine& Editor, TArray<FStatRow>& OutRows) const
{
	UWorld* World = Editor.GetWorld();
	if (!World)
	{
		AddRow(OutRows, "Vehicle", "scene unavailable");
		return;
	}

	int32 TotalVehicles = 0;
	int32 ActiveVehicles = 0;
	int32 TotalWheels = 0;
	int32 TireContacts = 0;
	int32 GroundedWheels = 0;

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor)
		{
			continue;
		}

		UVehicleMovementComponent* VehicleMovement = Actor->GetComponentByClass<UVehicleMovementComponent>();
		if (!VehicleMovement)
		{
			continue;
		}

		++TotalVehicles;
		if (!VehicleMovement->IsVehicleValid())
		{
			continue;
		}

		++ActiveVehicles;

		const FVehicleRuntimeStats* Stats = VehicleMovement->GetRuntimeStats();
		if (!Stats)
		{
			continue;
		}

		TotalWheels += Stats->WheelCount;
		for (const FVehicleWheelDebugState& Wheel : Stats->Wheels)
		{
			if (!Wheel.bInAir)
			{
				++GroundedWheels;
				++TireContacts;
			}
		}
	}

	if (TotalVehicles == 0)
	{
		AddRow(OutRows, "Vehicle", "no vehicles");
		return;
	}

	AddSectionHeader(OutRows, "State");
	AddRow(OutRows, "Vehicles", "%d total, %d active", TotalVehicles, ActiveVehicles);
	AddRow(OutRows, "Wheels", "%d", TotalWheels);
	AddRow(OutRows, "Suspensions", "%d", TotalWheels);
	AddRow(OutRows, "Drivetrain", "%d", ActiveVehicles);
	AddRow(OutRows, "Tire Contacts", "%d", TireContacts);
	AddRow(OutRows, "Grounded Wheels", "%d / %d", GroundedWheels, TotalWheels);

	AddSectionHeader(OutRows, "Frame");
	AddRow(OutRows, "Wheel Raycasts", "%d / frame", TotalWheels);
	AddRow(OutRows, "Tire Force Solves", "%d / frame", TotalWheels);
	AddRow(OutRows, "Suspension Solves", "%d / frame", TotalWheels);
	AddRow(OutRows, "Updated Wheels", "%d / frame", TotalWheels);

	AddSectionHeader(OutRows, "CPU Time");
	AddTimingRow(OutRows, "Tick", -1.0f, FindStatEntry("Vehicle", "Tick"));
	AddTimingRow(OutRows, "InputUpdate", -1.0f, FindStatEntry("Vehicle", "InputUpdate"));
	AddTimingRow(OutRows, "WheelQuery", -1.0f, FindStatEntry("Vehicle", "WheelQuery"));
	AddTimingRow(OutRows, "VehicleSimulate", -1.0f, FindStatEntry("Vehicle", "VehicleSimulate"));
	AddTimingRow(OutRows, "PostSync", -1.0f, FindStatEntry("Vehicle", "PostSync"));
}

void FOverlayStatSystem::BuildPhysicsLines(const UEditorEngine& Editor, TArray<FStatRow>& OutRows) const
{
	UWorld* World = Editor.GetWorld();
	IPhysicsSceneInterface* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
	if (!PhysicsScene)
	{
		AddRow(OutRows, "Physics", "scene unavailable");
		return;
	}

	const FPhysicsRuntimeStats& Stats = PhysicsScene->GetStats();
	const int32 DynamicBodies = Stats.DynamicBodyCount > 0 ? Stats.DynamicBodyCount : Stats.BodyCount;
	const int32 PerBodyActors = Stats.PerBodyActorCount > 0
		? Stats.PerBodyActorCount
		: (std::max)(0, Stats.BodyCount - Stats.AggregateActorCount);
	AddSectionHeader(OutRows, "State");
	AddRow(OutRows, "Bodies", "%d total, %d dynamic", Stats.BodyCount, DynamicBodies);
	AddRow(OutRows, "Active / Sleeping", "%d / %d", Stats.ActiveBodyCount, Stats.SleepingBodyCount);
	AddRow(OutRows, "Constraints", "%d", Stats.ConstraintCount);
	AddRow(OutRows, "Contacts", "%d", Stats.ContactCount);
	AddRow(OutRows, "Contact Pairs/Points", "%d / %d", Stats.ContactPairCount, Stats.ContactPointCount);
	AddRow(OutRows, "Aggregates", "%d active / %d actors", Stats.AggregateCount, Stats.AggregateActorCount);
	AddRow(OutRows, "Per-body Actors", "%d", PerBodyActors);

	AddSectionHeader(OutRows, "Solver");
	AddRow(OutRows, "Position Iterations", "%d", Stats.SolverPositionIterationCount);
	AddRow(OutRows, "Velocity Iterations", "%d", Stats.SolverVelocityIterationCount);
	AddRow(OutRows, "Constraint Count", "%d", Stats.SolverConstraintCount);
	AddRow(OutRows, "Contact Count", "%d", Stats.SolverContactCount);

	AddSectionHeader(OutRows, "CPU Time");
	AddTimingRow(OutRows, "PreSim", Stats.PreSimTimeMs, FindStatEntry("Physics", "PreSim"));
	AddTimingRow(OutRows, "PhysX Simulate", Stats.SimulateTimeMs, FindStatEntry("Physics", "PhysX Simulate"));
	AddTimingRow(OutRows, "Fetch Results", Stats.FetchResultsTimeMs, FindStatEntry("Physics", "Fetch Results"));
	AddTimingRow(OutRows, "PostSync", Stats.PostSyncTimeMs, FindStatEntry("Physics", "PostSync"));
	AddTimingRow(OutRows, "Total Physics", Stats.TotalPhysicsTimeMs, FindStatEntry("Physics", "Total Physics"));
}

void FOverlayStatSystem::BuildRagdollLines(const UEditorEngine& Editor, TArray<FStatRow>& OutRows) const
{
	UWorld* World = Editor.GetWorld();
	IPhysicsSceneInterface* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
	if (!PhysicsScene)
	{
		AddRow(OutRows, "Ragdoll", "scene unavailable");
		return;
	}

	const FPhysicsRuntimeStats& Stats = PhysicsScene->GetStats();
	const int32 PerBodyActors = (std::max)(0, Stats.RagdollBodyCount - Stats.AggregateActorCount);

	AddSectionHeader(OutRows, "State");
	AddRow(OutRows, "Ragdolls", "%d total", Stats.ActiveRagdollCount);
	AddRow(OutRows, "Bodies", "%d", Stats.RagdollBodyCount);
	AddRow(OutRows, "Constraints", "%d", Stats.RagdollConstraintCount);
	AddRow(OutRows, "Aggregates", "%d active / %d actors", Stats.ActiveAggregateRagdollCount, Stats.AggregateActorCount);
	AddRow(OutRows, "Per-body Actors", "%d", PerBodyActors);

	AddSectionHeader(OutRows, "Frame");
	AddRow(OutRows, "Body -> Bone Sync", "%d / frame", Stats.RagdollBodyCount);
	AddRow(OutRows, "Bone -> Body Sync", "%d / frame", 0);
	AddRow(OutRows, "Updated Bodies", "%d / frame", Stats.RagdollBodyCount);

	AddSectionHeader(OutRows, "CPU Time");
	AddTimingRow(OutRows, "BuildBodies", -1.0f, FindStatEntry("Ragdoll", "BuildBodies"));
	AddTimingRow(OutRows, "UpdateKinematic", -1.0f, FindStatEntry("Ragdoll", "UpdateKinematic"));
	AddTimingRow(OutRows, "BodyToBoneSync", -1.0f, FindStatEntry("Ragdoll", "BodyToBoneSync"));
	AddTimingRow(OutRows, "BoneToBodySync", -1.0f, FindStatEntry("Ragdoll", "BoneToBodySync"));
	AddTimingRow(OutRows, "DebugDraw", -1.0f, FindStatEntry("Ragdoll", "DebugDraw"));
}

void FOverlayStatSystem::BuildClothLines(const UEditorEngine& Editor, TArray<FStatRow>& OutRows) const
{
	int32 ComponentCount = 0;
	int32 ActiveComponentCount = 0;
	int32 ClothCount = 0;
	int32 SimulatedParticleCount = 0;
	int32 TotalParticleCount = 0;
	int32 ConstraintCount = 0;
	int32 ColliderCount = 0;
	int32 CollisionTestCount = 0;
	int32 UpdatedVertexCount = 0;
	bool bAnySelfCollision = false;

	if (UWorld* World = Editor.GetWorld())
	{
		for (AActor* Actor : World->GetActors())
		{
			if (!Actor)
			{
				continue;
			}

			for (UActorComponent* Component : Actor->GetComponents())
			{
				UClothComponent* ClothComponent = Cast<UClothComponent>(Component);
				if (!ClothComponent)
				{
					continue;
				}

				++ComponentCount;
				FClothInstance& ClothInstance = ClothComponent->GetClothInstance();
				if (!ClothInstance.IsInitialized())
				{
					continue;
				}

				++ActiveComponentCount;
				++ClothCount;
				SimulatedParticleCount += ClothInstance.GetParticleCount();
				TotalParticleCount += ClothInstance.GetParticleCount();
				ConstraintCount += ClothInstance.GetConstraintCount();
				ColliderCount += ClothInstance.GetColliderCount();
				CollisionTestCount += ClothInstance.GetCollisionTestCount();
				UpdatedVertexCount += static_cast<int32>(ClothInstance.GetRenderVertices().size());
				bAnySelfCollision = bAnySelfCollision || ClothInstance.IsSelfCollisionEnabled();
			}
		}
	}

	AddSectionHeader(OutRows, "State");
	AddRow(OutRows, "Components", "%d total, %d active", ComponentCount, ActiveComponentCount);
	AddRow(OutRows, "Cloths", "%d total", ClothCount);
	AddRow(OutRows, "Particles", "%d simulated / %d total", SimulatedParticleCount, TotalParticleCount);
	AddRow(OutRows, "Constraints", "%d", ConstraintCount);
	AddRow(OutRows, "Colliders", "%d", ColliderCount);
	AddRow(OutRows, "Self Collision", "%s", bAnySelfCollision ? "On" : "Off");

	AddSectionHeader(OutRows, "Frame");
	AddRow(OutRows, "Simulated Particles", "%d / frame", SimulatedParticleCount);
	AddRow(OutRows, "Collision Tests", "%d / frame", CollisionTestCount);
	AddRow(OutRows, "Constraint Solves", "%d / frame", ConstraintCount);
	AddRow(OutRows, "Updated Vertices", "%d / frame", UpdatedVertexCount);

	AddSectionHeader(OutRows, "CPU Time");
	AddTimingRow(OutRows, "Tick", -1.0f, FindStatEntry("Cloth", "Tick"));
	AddTimingRow(OutRows, "Simulate", -1.0f, FindStatEntry("Cloth", "Simulate"));
	AddTimingRow(OutRows, "Collision", -1.0f, FindStatEntry("Cloth", "Collision"));
	AddTimingRow(OutRows, "SkinningSync", -1.0f, FindStatEntry("Cloth", "SkinningSync"));
	AddTimingRow(OutRows, "BuildRenderData", -1.0f, FindStatEntry("Cloth", "BuildRenderData"));
}

void FOverlayStatSystem::BuildLines(const UEditorEngine& Editor, TArray<FOverlayStatLine>& OutLines) const
{
	OutLines.clear();

	TArray<FStatRow> Rows;
	float CurrentY = Layout.StartY;
	auto AppendGroup = [&](const TArray<FStatRow>& GroupRows)
		{
			for (const FStatRow& Row : GroupRows)
			{
				FString Combined = Row.Label + "  " + Row.Value;
				AppendLine(OutLines, CurrentY, Combined);
				CurrentY += Layout.LineHeight;
			}
			if (!GroupRows.empty())
				CurrentY += Layout.GroupSpacing;
		};

	if (bShowFPS)
	{
		Rows.clear();
		BuildFPSLines(Editor, Rows);
		AppendGroup(Rows);
	}
	if (bShowMemory)
	{
		Rows.clear();
		BuildMemoryLines(Rows);
		AppendGroup(Rows);
	}
	if (bShowShadow)
	{
		Rows.clear();
		BuildShadowLines(Rows);
		AppendGroup(Rows);
	}
	if (bShowSkinning)
	{
		Rows.clear();
		BuildSkinningLines(Rows);
		AppendGroup(Rows);
	}
	if (bShowParticle)
	{
		Rows.clear();
		BuildParticleLines(Editor, Rows);
		AppendGroup(Rows);
	}
	if (bShowPhysics)
	{
		Rows.clear();
		BuildPhysicsLines(Editor, Rows);
		AppendGroup(Rows);
	}
	if (bShowRagdoll)
	{
		Rows.clear();
		BuildRagdollLines(Editor, Rows);
		AppendGroup(Rows);
	}
	if (bShowCloth)
	{
		Rows.clear();
		BuildClothLines(Editor, Rows);
		AppendGroup(Rows);
	}
	if (bShowVehicle)
	{
		Rows.clear();
		BuildVehicleLines(Editor, Rows);
		AppendGroup(Rows);
	}
}

TArray<FOverlayStatLine> FOverlayStatSystem::BuildLines(const UEditorEngine& Editor) const
{
	TArray<FOverlayStatLine> Result;
	BuildLines(Editor, Result);
	return Result;
}

void FOverlayStatSystem::RenderImGui(const UEditorEngine& Editor, const FRect& ViewportRect) const
{
	if (ViewportRect.Width <= 1.0f || ViewportRect.Height <= 1.0f)
		return;

	constexpr float PaddingX    = 10.0f;
	constexpr float PaddingY    = 30.0f;
	constexpr float WindowGap   = 6.0f;
	constexpr float ScreenColGap = 8.0f;
	constexpr float ColumnGap   = 14.0f;  // 레이블↔값 사이 간격
	const float ViewportLeft    = ViewportRect.X;
	const float ViewportTop     = ViewportRect.Y;
	const float ViewportRight   = ViewportRect.X + ViewportRect.Width;
	const float ViewportBottom  = ViewportRect.Y + ViewportRect.Height;

	float CurrentX = ViewportLeft + PaddingX;
	float CurrentY = ViewportTop + PaddingY;
	float CurrentColumnWidth = 0.0f;

	ImGuiWindowFlags Flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoInputs;

	auto RenderWindow = [&](const char* WindowID, const char* Title, const ImVec4& BgColor, const TArray<FStatRow>& Rows)
		{
			if (Rows.empty())
				return;

			// 레이블 최대 너비 → 값 컬럼 시작 X 결정 (섹션 헤더 제외)
			float MaxLabelW = 0.0f;
			float MaxValueW = 0.0f;
			float MaxHeaderW = 0.0f;
			for (const FStatRow& Row : Rows)
			{
				if (Row.bIsSectionHeader)
					MaxHeaderW = (std::max)(MaxHeaderW, ImGui::CalcTextSize(Row.Label.c_str()).x);
				else
				{
					MaxLabelW = (std::max)(MaxLabelW, ImGui::CalcTextSize(Row.Label.c_str()).x);
					MaxValueW = (std::max)(MaxValueW, ImGui::CalcTextSize(Row.Value.c_str()).x);
				}
			}
			const float ContentW = (std::max)(MaxHeaderW, MaxLabelW + ColumnGap + MaxValueW);

			const float LineH = ImGui::GetTextLineHeightWithSpacing();
			const float EstimatedHeight =
				LineH * (static_cast<float>(Rows.size()) + 1.0f) +
				ImGui::GetStyle().WindowPadding.y * 2.0f;

			if (CurrentY > ViewportTop + PaddingY && CurrentY + EstimatedHeight > ViewportBottom - PaddingY)
			{
				CurrentX += CurrentColumnWidth + ScreenColGap;
				CurrentY = ViewportTop + PaddingY;
				CurrentColumnWidth = 0.0f;
			}
			CurrentX = (std::max)(ViewportLeft + PaddingX, (std::min)(CurrentX, ViewportRight - PaddingX - 40.0f));

			ImGui::SetNextWindowPos(ImVec2(CurrentX, CurrentY), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(ContentW + ImGui::GetStyle().WindowPadding.x * 2.0f, 0.0f), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(BgColor.w);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, BgColor);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);

			ImGui::Begin(WindowID, nullptr, Flags);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.95f), "%s", Title);
			ImGui::Separator();

			const float WinX = ImGui::GetCursorScreenPos().x;
			const float ValueX = WinX + MaxLabelW + ColumnGap;
			ImDrawList* DL = ImGui::GetWindowDrawList();
			const ImU32 LabelColor = ImGui::GetColorU32(ImVec4(0.72f, 0.72f, 0.72f, 1.0f));
			const ImU32 ValueColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

			const ImU32 HeaderColor = ImGui::GetColorU32(ImVec4(0.55f, 0.85f, 1.0f, 0.90f));
			for (const FStatRow& Row : Rows)
			{
				const float RowY = ImGui::GetCursorScreenPos().y;
				if (Row.bIsSectionHeader)
				{
					// 상단 여백 + 구분선
					ImGui::Dummy(ImVec2(ContentW, 3.0f));
					const float LineY = ImGui::GetCursorScreenPos().y;
					DL->AddLine(ImVec2(WinX, LineY), ImVec2(WinX + ContentW, LineY), ImGui::GetColorU32(ImVec4(0.4f, 0.6f, 0.8f, 0.5f)));
					const float TextY2 = ImGui::GetCursorScreenPos().y + 2.0f;
					DL->AddText(ImVec2(WinX, TextY2), HeaderColor, Row.Label.c_str());
					ImGui::Dummy(ImVec2(ContentW, ImGui::GetTextLineHeight() + 3.0f));
				}
				else
				{
					DL->AddText(ImVec2(WinX, RowY), LabelColor, Row.Label.c_str());
					DL->AddText(ImVec2(ValueX, RowY), ValueColor, Row.Value.c_str());
					ImGui::Dummy(ImVec2(ContentW, ImGui::GetTextLineHeight()));
				}
			}

			const ImVec2 WindowSize = ImGui::GetWindowSize();
			ImGui::End();

			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor();

			CurrentY += WindowSize.y + WindowGap;
			CurrentColumnWidth = (std::max)(CurrentColumnWidth, WindowSize.x);
		};

	TArray<FStatRow> Rows;
	if (bShowFPS)
	{
		BuildFPSLines(Editor, Rows);
		RenderWindow("##StatFPSOverlay", "Stat FPS", ImVec4(0.05f, 0.09f, 0.12f, 0.62f), Rows);
	}
	if (bShowMemory)
	{
		Rows.clear();
		BuildMemoryLines(Rows);
		RenderWindow("##StatMemoryOverlay", "Stat Memory", ImVec4(0.10f, 0.07f, 0.04f, 0.62f), Rows);
	}
	if (bShowShadow)
	{
		Rows.clear();
		BuildShadowLines(Rows);
		RenderWindow("##StatShadowOverlay", "Stat Shadow", ImVec4(0.08f, 0.05f, 0.12f, 0.62f), Rows);
	}
	if (bShowSkinning)
	{
		Rows.clear();
		BuildSkinningLines(Rows);
		RenderWindow("##StatSkinningOverlay", "Stat Skinning", ImVec4(0.05f, 0.10f, 0.08f, 0.62f), Rows);
	}
	if (bShowParticle)
	{
		Rows.clear();
		BuildParticleLines(Editor, Rows);
		RenderWindow("##StatParticleOverlay", "Stat Particle", ImVec4(0.05f, 0.08f, 0.12f, 0.62f), Rows);
	}
	if (bShowPhysics)
	{
		Rows.clear();
		BuildPhysicsLines(Editor, Rows);
		RenderWindow("##StatPhysicsOverlay", "Stat Physics", ImVec4(0.08f, 0.08f, 0.12f, 0.62f), Rows);
	}
	if (bShowRagdoll)
	{
		Rows.clear();
		BuildRagdollLines(Editor, Rows);
		RenderWindow("##StatRagdollOverlay", "Stat Ragdoll", ImVec4(0.08f, 0.06f, 0.10f, 0.62f), Rows);
	}
	if (bShowCloth)
	{
		Rows.clear();
		BuildClothLines(Editor, Rows);
		RenderWindow("##StatClothOverlay", "Stat Cloth", ImVec4(0.05f, 0.09f, 0.10f, 0.62f), Rows);
	}
	if (bShowVehicle)
	{
		Rows.clear();
		BuildVehicleLines(Editor, Rows);
		RenderWindow("##StatVehicleOverlay", "Stat Vehicle", ImVec4(0.06f, 0.08f, 0.06f, 0.62f), Rows);
	}
}
