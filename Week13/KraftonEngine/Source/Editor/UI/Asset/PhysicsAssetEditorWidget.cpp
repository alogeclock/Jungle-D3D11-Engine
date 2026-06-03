#include "PhysicsAssetEditorWidget.h"

#include "Editor/EditorEngine.h"
#include "Collision/RayUtils.h"
#include "Component/Light/DirectionalLightComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/GizmoComponent.h"
#include "Debug/DrawDebugHelpers.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "GameFramework/StaticMeshActor.h"
#include "GameFramework/World.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Mesh/MeshManager.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletonAsset.h"
#include "Mesh/StaticMesh.h"
#include "Object/FName.h"
#include "Object/FUObjectArray.h"
#include "Physics/Common/PhysicalMaterialManager.h"
#include "Physics/Common/PhysicsMaterialTypes.h"
#include "Physics/Assets/PhysicsConstraintSetup.h"
#include "Physics/Assets/PhysicsAsset.h"
#include "Physics/Assets/PhysicsAssetManager.h"
#include "Physics/Assets/PhysicsBodySetup.h"
#include "Physics/Assets/PhysicsShapeSetup.h"
#include "Asset/AssetPackage.h"
#include "Platform/Paths.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Render/Shader/ShaderManager.h"
#include "Runtime/Engine.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "UI/EditorTextureManager.h"
#include "UI/Toolbar/ViewportToolbar.h"
#include "Viewport/Viewport.h"
#include "Gizmo/GizmoTransformTarget.h"
#include "Component/BoxComponent.h"
#include "Component/StaticMeshComponent.h"

#include <algorithm>
#include <cfloat>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <unordered_set>

namespace
{
	static uint32 GNextPhysicsAssetEditorInstanceId = 0;
	static uint64 GNextPhysicsAssetPreviewResourceId = 0;
	constexpr float Pi = 3.14159265358979323846f;
	const FVector PhysicsAssetCapsuleAxis = FVector::ForwardVector;
	const FVector PositiveXMeshAxis = FVector::ForwardVector;
	constexpr ImVec4 PhysicsPanelAccentColor = ImVec4(0.0f, 0.71f, 0.86f, 1.0f);
	constexpr ImVec4 PhysicsPanelHeaderColor = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
	constexpr ImVec4 PhysicsPanelBodyColor = ImVec4(0.23f, 0.23f, 0.23f, 1.0f);
	constexpr ImVec2 PhysicsPanelContentPadding = ImVec2(12.0f, 10.0f);
	constexpr ImVec2 PhysicsPanelItemSpacing = ImVec2(8.0f, 8.0f);
	constexpr ImVec2 PhysicsGraphControlItemSpacing = ImVec2(10.0f, 8.0f);
	constexpr ImVec2 PhysicsGraphControlFramePadding = ImVec2(8.0f, 5.0f);
	constexpr ImVec2 PhysicsToolsPanelContentPadding = ImVec2(14.0f, 12.0f);
	constexpr ImVec2 PhysicsToolsPanelItemSpacing = ImVec2(10.0f, 10.0f);
	constexpr float PhysicsPanelHeaderHeight = 28.0f;
	constexpr float PhysicsPanelAccentWidth = 4.0f;
	constexpr float PhysicsPanelTitleOffsetX = 12.0f;
	constexpr float PhysicsPanelTitleOffsetY = 7.0f;
	constexpr float PhysicsPanelHeaderSpacing = 34.0f;
	constexpr float PhysicsViewportToolbarHeight = 34.0f;
	constexpr float ConstraintSwingVisualScale    = 0.6f;
	constexpr float ConstraintTwistVisualScale    = 0.45f;
	constexpr float ConstraintAxisArrowScale      = 0.35f;
	constexpr float ConstraintAxisZArrowScale     = 0.22f;

	FMatrix BuildBoneWorldMatrix(const USkeletalMeshComponent* MeshComponent, const TArray<FMatrix>* CachedBoneGlobals, int32 BoneIndex);

	float GetEffectiveConstraintLimitAngle(EPhysicsConstraintMotionMode MotionMode, float LimitedAngleDeg, float FreeAngleDeg)
	{
		if (MotionMode == EPhysicsConstraintMotionMode::Locked)
		{
			return 0.0f;
		}

		return MotionMode == EPhysicsConstraintMotionMode::Free ? FreeAngleDeg : LimitedAngleDeg;
	}

	float GetSwingVisualHalfAngle(EPhysicsConstraintMotionMode MotionMode, float LimitedAngleDeg)
	{
		return (std::clamp)(GetEffectiveConstraintLimitAngle(MotionMode, LimitedAngleDeg, 89.0f), 0.0f, 89.0f);
	}

	float GetSymmetricTwistLimitAngle(const FPhysicsConstraintSetup& Constraint)
	{
		if (Constraint.TwistMotion == EPhysicsConstraintMotionMode::Locked)
		{
			return 0.0f;
		}

		if (Constraint.TwistMotion == EPhysicsConstraintMotionMode::Free)
		{
			return 179.0f;
		}

		return (std::max)(std::fabs(Constraint.TwistLimitMin), std::fabs(Constraint.TwistLimitMax));
	}

	float GetTwistVisualHalfAngle(const FPhysicsConstraintSetup& Constraint)
	{
		return (std::clamp)(GetSymmetricTwistLimitAngle(Constraint), 0.0f, 179.0f);
	}

	bool ShouldRenderSwingCone(float Swing1Limit, float Swing2Limit)
	{
		return Swing1Limit > 0.0f && Swing2Limit > 0.0f;
	}

	bool AreConstraintVisualLimitsEqual(float A, float B)
	{
		return std::fabs(A - B) <= 0.01f;
	}

	FString MakePhysicsAssetPreviewResourcePath(const char* ResourceType)
	{
		return "__PhysicsAssetPreview__/" + FString(ResourceType) + "_" + std::to_string(GNextPhysicsAssetPreviewResourceId++);
	}

	bool IsEditorPhysicsStatVisible()
	{
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
		{
			return EditorEngine->GetOverlayStatSystem().IsShowingPhysics();
		}
		return false;
	}

	void DrawPhysicsPanelHeader(const char* Title)
	{
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImVec2 HeaderMin = ImGui::GetCursorScreenPos();
		const float Width = ImGui::GetContentRegionAvail().x;
		DrawList->AddRectFilled(
			HeaderMin,
			ImVec2(HeaderMin.x + Width, HeaderMin.y + PhysicsPanelHeaderHeight),
			ImGui::GetColorU32(PhysicsPanelHeaderColor));
		DrawList->AddRectFilled(
			HeaderMin,
			ImVec2(HeaderMin.x + PhysicsPanelAccentWidth, HeaderMin.y + PhysicsPanelHeaderHeight),
			ImGui::GetColorU32(PhysicsPanelAccentColor));
		DrawList->AddText(
			ImVec2(HeaderMin.x + PhysicsPanelTitleOffsetX, HeaderMin.y + PhysicsPanelTitleOffsetY),
			ImGui::GetColorU32(ImGuiCol_Text),
			Title);
		ImGui::Dummy(ImVec2(Width, PhysicsPanelHeaderSpacing));
	}

	bool InputFString(const char* Label, FString& Value)
	{
		char Buffer[256];
		std::snprintf(Buffer, sizeof(Buffer), "%s", Value.c_str());
		if (ImGui::InputText(Label, Buffer, sizeof(Buffer)))
		{
			Value = Buffer;
			return true;
		}
		return false;
	}

	bool InputFName(const char* Label, FName& Value)
	{
		FString StringValue = Value.ToString();
		if (InputFString(Label, StringValue))
		{
			Value = FName(StringValue);
			return true;
		}
		return false;
	}

	bool DragVector3(const char* Label, FVector& Value, float Speed = 0.1f)
	{
		float Values[3] = { Value.X, Value.Y, Value.Z };
		if (ImGui::DragFloat3(Label, Values, Speed))
		{
			Value = FVector(Values[0], Values[1], Values[2]);
			return true;
		}
		return false;
	}

	bool RenderTransformEditor(const char* Label, FTransform& Transform)
	{
		bool bChanged = false;
		if (ImGui::TreeNode(Label))
		{
			bChanged |= DragVector3("Location", Transform.Location, 0.1f);

			FRotator Rotation = Transform.GetRotator();
			float RotationValues[3] = { Rotation.Pitch, Rotation.Yaw, Rotation.Roll };
			if (ImGui::DragFloat3("Rotation", RotationValues, 0.5f))
			{
				Transform.SetRotation(FRotator(RotationValues[0], RotationValues[1], RotationValues[2]));
				bChanged = true;
			}

			bChanged |= DragVector3("Scale", Transform.Scale, 0.01f);
			ImGui::TreePop();
		}
		return bChanged;
	}

		bool RenderBodyTypeCombo(EPhysicsBodyType& BodyType, const char* Label = "Body Type")
		{
			const char* Labels[] = { "Static", "Dynamic", "Kinematic" };
			int32 CurrentIndex = static_cast<int32>(BodyType);
			if (ImGui::Combo(Label, &CurrentIndex, Labels, IM_ARRAYSIZE(Labels)))
			{
				BodyType = static_cast<EPhysicsBodyType>(CurrentIndex);
				return true;
			}
			return false;
	}

	bool RenderCollisionEnabledCombo(const char* Label, EPhysicsCollisionEnabled& CollisionEnabled)
	{
		const char* Labels[] = { "No Collision", "Query Only", "Physics Only", "Query And Physics" };
		int32 CurrentIndex = static_cast<int32>(CollisionEnabled);
		if (ImGui::Combo(Label, &CurrentIndex, Labels, IM_ARRAYSIZE(Labels)))
		{
			CollisionEnabled = static_cast<EPhysicsCollisionEnabled>(CurrentIndex);
			return true;
		}
		return false;
	}

	bool RenderCollisionChannelCombo(const char* Label, EPhysicsCollisionChannel& Channel)
	{
		const char* Labels[] = { "WorldStatic", "WorldDynamic", "Pawn", "PhysicsBody", "Visibility", "Camera", "Custom" };
		int32 CurrentIndex = static_cast<int32>(Channel);
		if (ImGui::Combo(Label, &CurrentIndex, Labels, IM_ARRAYSIZE(Labels)))
		{
			Channel = static_cast<EPhysicsCollisionChannel>(CurrentIndex);
			return true;
		}
		return false;
	}

	bool RenderCollisionResponseCombo(const char* Label, EPhysicsCollisionResponse& Response)
	{
		const char* Labels[] = { "Ignore", "Overlap", "Block" };
		int32 CurrentIndex = static_cast<int32>(Response);
		if (ImGui::Combo(Label, &CurrentIndex, Labels, IM_ARRAYSIZE(Labels)))
		{
			Response = static_cast<EPhysicsCollisionResponse>(CurrentIndex);
			return true;
		}
		return false;
	}

		bool RenderJointTypeCombo(EPhysicsJointType& JointType, const char* Label = "Joint Type")
		{
			const char* Labels[] = { "Fixed", "Distance", "Revolute", "Prismatic", "Spherical", "D6" };
			int32 CurrentIndex = static_cast<int32>(JointType);
			if (ImGui::Combo(Label, &CurrentIndex, Labels, IM_ARRAYSIZE(Labels)))
			{
				JointType = static_cast<EPhysicsJointType>(CurrentIndex);
				return true;
			}
		return false;
	}

		bool RenderCollisionDesc(FPhysicsCollisionDesc& CollisionDesc)
		{
		bool bChanged = false;

		bChanged |= RenderCollisionEnabledCombo("Collision Enabled", CollisionDesc.CollisionEnabled);
		bChanged |= RenderCollisionChannelCombo("Object Channel", CollisionDesc.ObjectChannel);

		if (ImGui::TreeNode("Collision Responses"))
		{
			if (ImGui::Button("Add Response"))
			{
				CollisionDesc.Responses.push_back(FPhysicsCollisionResponsePair());
				bChanged = true;
			}

			for (int32 ResponseIndex = 0; ResponseIndex < static_cast<int32>(CollisionDesc.Responses.size()); ++ResponseIndex)
			{
				ImGui::PushID(ResponseIndex);
				FPhysicsCollisionResponsePair& ResponsePair = CollisionDesc.Responses[ResponseIndex];
				char Label[64];
				std::snprintf(Label, sizeof(Label), "Response %d", ResponseIndex);
				if (ImGui::TreeNode(Label))
				{
					bChanged |= RenderCollisionChannelCombo("Channel", ResponsePair.Channel);
					bChanged |= RenderCollisionResponseCombo("Response", ResponsePair.Response);
					if (ImGui::Button("Remove Response"))
					{
						CollisionDesc.Responses.erase(CollisionDesc.Responses.begin() + ResponseIndex);
						bChanged = true;
						ImGui::TreePop();
						ImGui::PopID();
						break;
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			ImGui::TreePop();
		}

			return bChanged;
		}

		TArray<FString> GatherPhysicalMaterialAssetPaths()
		{
			TArray<FString> Paths;
			const std::filesystem::path AssetRoot(FPaths::AssetDir());
			if (!std::filesystem::exists(AssetRoot))
			{
				return Paths;
			}

			for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
			{
				if (!Entry.is_regular_file() || Entry.path().extension() != L".uasset")
				{
					continue;
				}

				const FString PackagePath = FPaths::MakeProjectRelative(FPaths::ToUtf8(Entry.path().generic_wstring()));
				EAssetPackageType PackageType = EAssetPackageType::Unknown;
				if (FAssetPackage::GetPackageType(PackagePath, PackageType) && PackageType == EAssetPackageType::PhysicalMaterial)
				{
					Paths.push_back(PackagePath);
				}
			}

			std::sort(Paths.begin(), Paths.end());
			return Paths;
		}

		bool RenderPhysicalMaterialPicker(const char* Label, UPhysicalMaterial*& PhysicalMaterial)
		{
			const TArray<FString> MaterialPaths = GatherPhysicalMaterialAssetPaths();
			const FString CurrentPath = PhysicalMaterial ? PhysicalMaterial->GetAssetPathFileName() : FString();
			FString PreviewLabel = CurrentPath.empty() ? FString("None") : std::filesystem::path(FPaths::ToWide(CurrentPath)).stem().string();

			bool bChanged = false;
			if (ImGui::BeginCombo(Label, PreviewLabel.c_str()))
			{
				const bool bSelectedNone = CurrentPath.empty();
				if (ImGui::Selectable("None", bSelectedNone))
				{
					PhysicalMaterial = nullptr;
					bChanged = true;
				}

				for (const FString& MaterialPath : MaterialPaths)
				{
					const FString DisplayName = FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(MaterialPath)).stem().wstring());
					const bool bSelected = MaterialPath == CurrentPath;
					if (ImGui::Selectable(DisplayName.c_str(), bSelected))
					{
						PhysicalMaterial = FPhysicalMaterialManager::Get().Load(MaterialPath);
						bChanged = true;
					}
				}

				ImGui::EndCombo();
			}

			return bChanged;
		}

		bool RenderConstraintMotionRow(const char* Label, EPhysicsConstraintMotionMode& MotionMode)
		{
			bool bChanged = false;
			ImGui::PushID(Label);
			if (ImGui::RadioButton("Free", MotionMode == EPhysicsConstraintMotionMode::Free))
			{
				MotionMode = EPhysicsConstraintMotionMode::Free;
				bChanged = true;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("Limited", MotionMode == EPhysicsConstraintMotionMode::Limited))
			{
				MotionMode = EPhysicsConstraintMotionMode::Limited;
				bChanged = true;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("Locked", MotionMode == EPhysicsConstraintMotionMode::Locked))
			{
				MotionMode = EPhysicsConstraintMotionMode::Locked;
				bChanged = true;
			}
			ImGui::PopID();
			return bChanged;
		}

	template<typename TShapeArray, typename TRenderBody>
	bool RenderShapeArraySection(const char* Label, TShapeArray& Shapes, const char* AddButtonLabel, TRenderBody&& RenderBody)
	{
		bool bChanged = false;
		if (!ImGui::CollapsingHeader(Label, ImGuiTreeNodeFlags_DefaultOpen))
		{
			return false;
		}

		if (ImGui::Button(AddButtonLabel))
		{
			Shapes.push_back(typename TShapeArray::value_type());
			bChanged = true;
		}

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(Shapes.size()); ++ShapeIndex)
		{
			ImGui::PushID(ShapeIndex);
			char TreeLabel[64];
			std::snprintf(TreeLabel, sizeof(TreeLabel), "%s %d", Label, ShapeIndex);
			if (ImGui::TreeNode(TreeLabel))
			{
				bChanged |= RenderBody(Shapes[ShapeIndex]);
				if (ImGui::Button("Remove Shape"))
				{
					Shapes.erase(Shapes.begin() + ShapeIndex);
					bChanged = true;
					ImGui::TreePop();
					ImGui::PopID();
					break;
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		return bChanged;
	}

	bool RenderSphereShape(FPhysicsSphereShapeSetup& Shape)
	{
		bool bChanged = false;
		bChanged |= InputFName("Name", Shape.Name);
		bChanged |= RenderTransformEditor("Local Transform", Shape.LocalTransform);
		if (ImGui::DragFloat("Radius", &Shape.Radius, 0.1f, 0.0f, 10000.0f))
		{
			bChanged = true;
		}
		bChanged |= RenderCollisionDesc(Shape.CollisionDesc);
		return bChanged;
	}

	bool RenderBoxShape(FPhysicsBoxShapeSetup& Shape)
	{
		bool bChanged = false;
		bChanged |= InputFName("Name", Shape.Name);
		bChanged |= RenderTransformEditor("Local Transform", Shape.LocalTransform);
		bChanged |= DragVector3("Half Extent", Shape.HalfExtent, 0.1f);
		bChanged |= RenderCollisionDesc(Shape.CollisionDesc);
		return bChanged;
	}

	bool RenderCapsuleShape(FPhysicsCapsuleShapeSetup& Shape)
	{
		bool bChanged = false;
		bChanged |= InputFName("Name", Shape.Name);
		bChanged |= RenderTransformEditor("Local Transform", Shape.LocalTransform);
		if (ImGui::DragFloat("Radius", &Shape.Radius, 0.1f, 0.0f, 10000.0f))
		{
			bChanged = true;
		}
		if (ImGui::DragFloat("Length", &Shape.Length, 0.1f, 0.0f, 10000.0f))
		{
			bChanged = true;
		}
		bChanged |= RenderCollisionDesc(Shape.CollisionDesc);
		return bChanged;
	}

		bool RenderBodySetupInspector(UPhysicsBodySetup* BodySetup)
		{
			if (!BodySetup)
			{
				return false;
			}

			bool bChanged = false;

			if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::BeginTable("BodyPhysicsTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
				{
					auto Row = [](const char* Label)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(Label);
						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(-1.0f);
					};

					Row("Mass (kg)");
					bChanged |= ImGui::Checkbox("##OverrideMass", &BodySetup->bOverrideMass);
					ImGui::SameLine();
					{
						float Mass = BodySetup->GetMass();
						ImGui::BeginDisabled(!BodySetup->bOverrideMass);
						if (ImGui::DragFloat("##Mass", &Mass, 0.05f, 0.0f, 100000.0f))
						{
							BodySetup->SetMass(Mass);
							bChanged = true;
						}
						ImGui::EndDisabled();
					}

					Row("Linear Damping");
					{
						float LinearDamping = BodySetup->GetLinearDamping();
						if (ImGui::DragFloat("##LinearDamping", &LinearDamping, 0.01f, 0.0f, 1000.0f))
						{
							BodySetup->SetLinearDamping(LinearDamping);
							bChanged = true;
						}
					}

					Row("Angular Damping");
					{
						float AngularDamping = BodySetup->GetAngularDamping();
						if (ImGui::DragFloat("##AngularDamping", &AngularDamping, 0.01f, 0.0f, 1000.0f))
						{
							BodySetup->SetAngularDamping(AngularDamping);
							bChanged = true;
						}
					}

					Row("Enable Gravity");
					bChanged |= ImGui::Checkbox("##EnableGravity", &BodySetup->bEnableGravity);

					Row("Gravity Group Index");
					bChanged |= ImGui::InputInt("##GravityGroupIndex", &BodySetup->GravityGroupIndex);

					Row("Update Kinematic from Simulation");
					bChanged |= ImGui::Checkbox("##UpdateKinematicFromSimulation", &BodySetup->bUpdateKinematicFromSimulation);

					Row("Gyroscopic Torque Enabled");
					bChanged |= ImGui::Checkbox("##GyroscopicTorqueEnabled", &BodySetup->bGyroscopicTorqueEnabled);

					Row("Double Sided Geometry");
					bChanged |= ImGui::Checkbox("##DoubleSidedGeometry", &BodySetup->bDoubleSidedGeometry);

					Row("Simple Collision Physical Material");
					bChanged |= RenderPhysicalMaterialPicker("##SimpleCollisionPhysicalMaterial", BodySetup->SimpleCollisionPhysicalMaterial);

					Row("Physics Type");
					{
						EPhysicsBodyType BodyType = BodySetup->GetBodyType();
						if (RenderBodyTypeCombo(BodyType, "##BodyType"))
						{
							BodySetup->SetBodyType(BodyType);
							bChanged = true;
						}
					}

					ImGui::EndTable();
				}
			}

			if (ImGui::CollapsingHeader("Body Setup", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::BeginTable("BodySetupMetaTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
				{
					auto Row = [](const char* Label)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(Label);
						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(-1.0f);
					};

					Row("Skip Scale from Animation");
					bChanged |= ImGui::Checkbox("##SkipScaleFromAnimation", &BodySetup->bSkipScaleFromAnimation);
					ImGui::EndTable();
				}
			}

			if (ImGui::CollapsingHeader("Primitives", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::BeginTable("BodyPrimitiveMetaTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
				{
					auto Row = [](const char* Label)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(Label);
						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(-1.0f);
					};

					Row("Consider for Bounds");
					bChanged |= ImGui::Checkbox("##ConsiderForBounds", &BodySetup->bConsiderForBounds);

					Row("Bone Name");
					{
						char BoneName[256];
						std::snprintf(BoneName, sizeof(BoneName), "%s", BodySetup->GetTargetBoneName().ToString().c_str());
						ImGui::BeginDisabled();
						ImGui::InputText("##BodyBoneName", BoneName, sizeof(BoneName), ImGuiInputTextFlags_ReadOnly);
						ImGui::EndDisabled();
					}

					ImGui::EndTable();
				}

				FPhysicsAggregateShapeSetup& ShapeSetup = BodySetup->GetMutableShapeSetup();
				// Convex collision is intentionally hidden in the editor UI.
				bChanged |= RenderShapeArraySection("Sphere Shapes", ShapeSetup.SphereShapeSetups, "Add Sphere",
					[](FPhysicsSphereShapeSetup& Shape) { return RenderSphereShape(Shape); });
				bChanged |= RenderShapeArraySection("Box Shapes", ShapeSetup.BoxShapeSetups, "Add Box",
					[](FPhysicsBoxShapeSetup& Shape) { return RenderBoxShape(Shape); });
				bChanged |= RenderShapeArraySection("Capsule Shapes", ShapeSetup.CapsuleShapeSetups, "Add Capsule",
					[](FPhysicsCapsuleShapeSetup& Shape) { return RenderCapsuleShape(Shape); });
			}

			if (ImGui::CollapsingHeader("Collision", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::BeginTable("BodyCollisionTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
				{
					auto Row = [](const char* Label)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(Label);
						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(-1.0f);
					};

					Row("Simulation Generates Hit Events");
					bChanged |= ImGui::Checkbox("##SimulationGeneratesHitEvents", &BodySetup->bSimulationGeneratesHitEvents);

					Row("Phys Material Override");
					bChanged |= RenderPhysicalMaterialPicker("##PhysMaterialOverride", BodySetup->PhysMaterialOverride);

					Row("Never Needs Cooked Collision Data");
					bChanged |= ImGui::Checkbox("##NeverNeedsCookedCollisionData", &BodySetup->bNeverNeedsCookedCollisionData);

					Row("Collision Complexity");
					{
						const char* Labels[] = { "Project Default", "Use Simple Collision As Complex", "Use Complex Collision As Simple" };
						int CurrentIndex = static_cast<int>(BodySetup->CollisionComplexity);
						if (ImGui::Combo("##CollisionComplexity", &CurrentIndex, Labels, IM_ARRAYSIZE(Labels)))
						{
							BodySetup->CollisionComplexity = static_cast<EPhysicsBodyCollisionComplexity>(CurrentIndex);
							bChanged = true;
						}
					}

					Row("Collision Response");
					{
						FPhysicsCollisionDesc CollisionDesc = BodySetup->GetCollisionDesc();
						if (RenderCollisionEnabledCombo("##BodyCollisionEnabled", CollisionDesc.CollisionEnabled))
						{
							BodySetup->SetCollisionDesc(CollisionDesc);
							bChanged = true;
						}
					}

					ImGui::EndTable();
				}

				FPhysicsCollisionDesc CollisionDesc = BodySetup->GetCollisionDesc();
				if (ImGui::TreeNode("Collision Response Details"))
				{
					if (RenderCollisionDesc(CollisionDesc))
					{
						BodySetup->SetCollisionDesc(CollisionDesc);
						bChanged = true;
					}
					ImGui::TreePop();
				}
			}

			return bChanged;
		}

		bool RenderConstraintInspector(
			FPhysicsConstraintSetup& Constraint,
			bool* bOutPreviewDefinitionChanged = nullptr,
			bool* bOutPreviewTransformChanged = nullptr)
		{
			bool bChanged = false;
			bool bPreviewDefinitionChanged = false;
			bool bPreviewTransformChanged = false;

			if (ImGui::BeginTable("ConstraintTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
			{
				auto Row = [](const char* Label)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(Label);
					ImGui::TableSetColumnIndex(1);
					ImGui::SetNextItemWidth(-1.0f);
				};

				Row("Joint Name");
				bChanged |= InputFName("##ConstraintName", Constraint.ConstraintName);

				Row("Child Bone Name");
				if (InputFName("##ChildBoneName", Constraint.ChildBoneName))
				{
					bChanged = true;
					bPreviewTransformChanged = true;
				}

				Row("Parent Bone Name");
				if (InputFName("##ParentBoneName", Constraint.ParentBoneName))
				{
					bChanged = true;
					bPreviewTransformChanged = true;
				}

				ImGui::EndTable();
			}

			if (ImGui::CollapsingHeader("Constraint Transforms", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (RenderTransformEditor("Child", Constraint.ChildLocalFrame))
				{
					bChanged = true;
					bPreviewTransformChanged = true;
				}
				if (RenderTransformEditor("Parent", Constraint.ParentLocalFrame))
				{
					bChanged = true;
					bPreviewTransformChanged = true;
				}
			}

			if (ImGui::CollapsingHeader("Constraint Behavior", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::BeginTable("ConstraintBehaviorTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
				{
					auto Row = [](const char* Label)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(Label);
						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(-1.0f);
					};

					Row("Disable Collision");
					bChanged |= ImGui::Checkbox("##ConstraintDisableCollision", &Constraint.bDisableCollision);

					Row("Parent Dominates");
					bChanged |= ImGui::Checkbox("##ConstraintParentDominates", &Constraint.bParentDominates);

					Row("Enable Mass Conditioning");
					bChanged |= ImGui::Checkbox("##ConstraintMassConditioning", &Constraint.bEnableMassConditioning);

					Row("Use Linear Joint Solver");
					bChanged |= ImGui::Checkbox("##ConstraintLinearJointSolver", &Constraint.bUseLinearJointSolver);

					Row("Joint Type");
					bChanged |= RenderJointTypeCombo(Constraint.JointType, "##ConstraintJointType");

					ImGui::EndTable();
				}
			}

			if (ImGui::CollapsingHeader("Linear Limits", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::BeginTable("ConstraintLinearTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
				{
					auto Row = [](const char* Label)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(Label);
						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(-1.0f);
					};

					Row("X Motion");
					bChanged |= RenderConstraintMotionRow("XMotion", Constraint.XMotion);

					Row("Y Motion");
					bChanged |= RenderConstraintMotionRow("YMotion", Constraint.YMotion);

					Row("Z Motion");
					bChanged |= RenderConstraintMotionRow("ZMotion", Constraint.ZMotion);

					Row("Limit");
					bChanged |= ImGui::DragFloat("##ConstraintLinearLimit", &Constraint.LinearLimit, 0.1f, 0.0f, 100000.0f);

					Row("Scale Linear Limits");
					bChanged |= ImGui::Checkbox("##ConstraintScaleLinearLimits", &Constraint.bScaleLinearLimits);

					ImGui::EndTable();
				}
			}

			if (ImGui::CollapsingHeader("Angular Limits", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::BeginTable("ConstraintAngularTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
				{
					auto Row = [](const char* Label)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(Label);
						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(-1.0f);
					};

					Row("Swing 1 Motion");
					if (RenderConstraintMotionRow("Swing1Motion", Constraint.Swing1Motion))
					{
						bChanged = true;
						bPreviewDefinitionChanged = true;
					}

					Row("Swing 2 Motion");
					if (RenderConstraintMotionRow("Swing2Motion", Constraint.Swing2Motion))
					{
						bChanged = true;
						bPreviewDefinitionChanged = true;
					}

					Row("Twist Motion");
					if (RenderConstraintMotionRow("TwistMotion", Constraint.TwistMotion))
					{
						bChanged = true;
						bPreviewDefinitionChanged = true;
					}

					Row("Swing 1 Limit");
					if (ImGui::DragFloat("##Swing1Limit", &Constraint.SwingLimitY, 0.1f, 0.0f, 360.0f))
					{
						bChanged = true;
						// While dragging, the selected constraint is drawn by the live filled overlay.
						// Do not rebuild persistent UStaticMesh resources every frame.
					}
					if (ImGui::IsItemDeactivatedAfterEdit())
					{
						// Once the drag/text edit is committed, refresh the persistent preview mesh once
						// so this constraint still shows the latest limit when it is no longer selected.
						bPreviewDefinitionChanged = true;
					}

					Row("Swing 2 Limit");
					if (ImGui::DragFloat("##Swing2Limit", &Constraint.SwingLimitZ, 0.1f, 0.0f, 360.0f))
					{
						bChanged = true;
					}
					if (ImGui::IsItemDeactivatedAfterEdit())
					{
						bPreviewDefinitionChanged = true;
					}

					Row("Twist Limit");
					{
						float TwistLimit = GetSymmetricTwistLimitAngle(Constraint);
						if (ImGui::DragFloat("##TwistLimit", &TwistLimit, 0.1f, 0.0f, 360.0f))
						{
							TwistLimit = (std::max)(TwistLimit, 0.0f);
							Constraint.TwistLimitMin = -TwistLimit;
							Constraint.TwistLimitMax = TwistLimit;
							bChanged = true;
						}
						if (ImGui::IsItemDeactivatedAfterEdit())
						{
							bPreviewDefinitionChanged = true;
						}
					}

					Row("Stiffness");
					bChanged |= ImGui::DragFloat("##ConstraintStiffness", &Constraint.Stiffness, 0.1f, 0.0f, 100000.0f);

					Row("Damping");
					bChanged |= ImGui::DragFloat("##ConstraintDamping", &Constraint.Damping, 0.1f, 0.0f, 100000.0f);

					ImGui::EndTable();
				}
			}

			if (bOutPreviewDefinitionChanged)
			{
				*bOutPreviewDefinitionChanged |= bPreviewDefinitionChanged;
			}
			if (bOutPreviewTransformChanged)
			{
				*bOutPreviewTransformChanged |= bPreviewTransformChanged;
			}

			return bChanged;
		}

	bool RenderCreateParamsEditor(FPhysicsAssetCreateParams& Params)
	{
		bool bChanged = false;

		if (!ImGui::BeginTable("CreateParamsTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
		{
			return false;
		}

		auto Row = [](const char* Label)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(Label);
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1.0f);
		};

		Row("Min Bone Size");
		bChanged |= ImGui::DragFloat("##MinBoneSize", &Params.MinBoneSize, 0.1f, 0.1f, 1000.0f);

		Row("Body Grouping");
		{
			int32 RagdollMode = static_cast<int32>(Params.RagdollMode);
			const char* RagdollModeLabels[] = { "Per-Body", "PxAggregate" };
			if (ImGui::Combo("##RagdollMode", &RagdollMode, RagdollModeLabels, IM_ARRAYSIZE(RagdollModeLabels)))
			{
				Params.RagdollMode = static_cast<EPhysicsAssetRagdollMode>(RagdollMode);
				bChanged = true;
			}
		}

		Row("Primitive Type");
		{
			int32 PrimitiveType = static_cast<int32>(Params.PrimitiveType);
			const char* PrimitiveLabels[] = { "Capsule", "Box", "Sphere" };
			if (ImGui::Combo("##PrimitiveType", &PrimitiveType, PrimitiveLabels, IM_ARRAYSIZE(PrimitiveLabels)))
			{
				Params.PrimitiveType = static_cast<EPhysicsAssetPrimitiveType>(PrimitiveType);
				bChanged = true;
			}
		}

		Row("Vertex Weighting Type");
		{
			int32 VertexWeightingType = static_cast<int32>(Params.VertexWeightingType);
			const char* VertexWeightLabels[] = { "Dominant Weight", "Any Weight" };
			if (ImGui::Combo("##VertexWeightingType", &VertexWeightingType, VertexWeightLabels, IM_ARRAYSIZE(VertexWeightLabels)))
			{
				Params.VertexWeightingType = static_cast<EPhysicsAssetVertexWeightingType>(VertexWeightingType);
				bChanged = true;
			}
		}

		Row("Auto Orient to Bone");
		bChanged |= ImGui::Checkbox("##AutoOrientToBone", &Params.bAutoOrientToBone);

		Row("Walk Past Small Bones");
		bChanged |= ImGui::Checkbox("##WalkPastSmallBones", &Params.bWalkPastSmallBones);

		Row("Create Body for All Bones");
		bChanged |= ImGui::Checkbox("##CreateBodyForAllBones", &Params.bCreateBodyForAllBones);

		Row("Disable Collisions by Default");
		bChanged |= ImGui::Checkbox("##DisableCollisionsByDefault", &Params.bDisableCollisionsByDefault);

		Row("LOD Index");
		if (ImGui::InputInt("##LodIndex", &Params.LodIndex))
		{
			Params.LodIndex = (std::max)(Params.LodIndex, 0);
			bChanged = true;
		}

		Row("Create Constraints");
		bChanged |= ImGui::Checkbox("##CreateConstraints", &Params.bCreateConstraints);

		Row("Angular Constraint Mode");
		{
			int32 AngularMode = static_cast<int32>(Params.AngularConstraintMode);
			const char* AngularModeLabels[] = { "Limited", "Locked", "Free" };
			if (ImGui::Combo("##AngularConstraintMode", &AngularMode, AngularModeLabels, IM_ARRAYSIZE(AngularModeLabels)))
			{
				Params.AngularConstraintMode = static_cast<EPhysicsAssetAngularConstraintMode>(AngularMode);
				bChanged = true;
			}
		}

		ImGui::EndTable();
		return bChanged;
	}

	FString ToLowerCopy(const FString& Value)
	{
		FString Lower = Value;
		std::transform(Lower.begin(), Lower.end(), Lower.begin(),
			[](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
		return Lower;
	}

	bool ContainsInsensitive(const FString& Haystack, const FString& Needle)
	{
		if (Needle.empty())
		{
			return true;
		}

		return ToLowerCopy(Haystack).find(ToLowerCopy(Needle)) != FString::npos;
	}

	int32 FindBodySetupIndex(const UPhysicsAsset* PhysicsAsset, const FName& BoneName);

	bool BoneHasBody(const UPhysicsAsset* PhysicsAsset, const FBone& Bone)
	{
		if (!PhysicsAsset)
		{
			return false;
		}

		return FindBodySetupIndex(PhysicsAsset, FName(Bone.Name)) >= 0;
	}

	void GatherDisplayedBonesFromSubtree(
		const FSkeletonAsset* SkeletonAsset,
		const UPhysicsAsset* PhysicsAsset,
		int32 BoneIndex,
		const FString& SearchText,
		bool bShowBodiesOnly,
		TArray<int32>& OutBoneIndices);

	void GatherDisplayedChildBones(
		const FSkeletonAsset* SkeletonAsset,
		const UPhysicsAsset* PhysicsAsset,
		int32 ParentBoneIndex,
		const FString& SearchText,
		bool bShowBodiesOnly,
		TArray<int32>& OutBoneIndices)
	{
		if (!SkeletonAsset)
		{
			return;
		}

		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(SkeletonAsset->Bones.size()); ++BoneIndex)
		{
			if (SkeletonAsset->Bones[BoneIndex].ParentIndex == ParentBoneIndex)
			{
				GatherDisplayedBonesFromSubtree(
					SkeletonAsset,
					PhysicsAsset,
					BoneIndex,
					SearchText,
					bShowBodiesOnly,
					OutBoneIndices);
			}
		}
	}

	bool BoneMatchesTreeFilterRecursive(
		const FSkeletonAsset* SkeletonAsset,
		const UPhysicsAsset* PhysicsAsset,
		int32 BoneIndex,
		const FString& SearchText,
		bool bShowBodiesOnly)
	{
		if (!SkeletonAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(SkeletonAsset->Bones.size()))
		{
			return false;
		}

		const FBone& Bone = SkeletonAsset->Bones[BoneIndex];
		const bool bMatchesSearch = ContainsInsensitive(Bone.Name, SearchText);
		const bool bMatchesSelf = bMatchesSearch && (!bShowBodiesOnly || BoneHasBody(PhysicsAsset, Bone));
		if (bMatchesSelf)
		{
			return true;
		}

		for (int32 Index = 0; Index < static_cast<int32>(SkeletonAsset->Bones.size()); ++Index)
		{
			if (SkeletonAsset->Bones[Index].ParentIndex == BoneIndex
				&& BoneMatchesTreeFilterRecursive(SkeletonAsset, PhysicsAsset, Index, SearchText, bShowBodiesOnly))
			{
				return true;
			}
		}

		return false;
	}

	void GatherDisplayedBonesFromSubtree(
		const FSkeletonAsset* SkeletonAsset,
		const UPhysicsAsset* PhysicsAsset,
		int32 BoneIndex,
		const FString& SearchText,
		bool bShowBodiesOnly,
		TArray<int32>& OutBoneIndices)
	{
		if (!BoneMatchesTreeFilterRecursive(SkeletonAsset, PhysicsAsset, BoneIndex, SearchText, bShowBodiesOnly))
		{
			return;
		}

		const FBone& Bone = SkeletonAsset->Bones[BoneIndex];
		if (!bShowBodiesOnly || BoneHasBody(PhysicsAsset, Bone))
		{
			OutBoneIndices.push_back(BoneIndex);
			return;
		}

		GatherDisplayedChildBones(
			SkeletonAsset,
			PhysicsAsset,
			BoneIndex,
			SearchText,
			bShowBodiesOnly,
			OutBoneIndices);
	}

	int32 FindSkeletonBoneIndexByName(const FSkeletonAsset* SkeletonAsset, const FName& BoneName)
	{
		if (!SkeletonAsset)
		{
			return -1;
		}

		const FString TargetName = BoneName.ToString();
		for (int32 Index = 0; Index < static_cast<int32>(SkeletonAsset->Bones.size()); ++Index)
		{
			if (SkeletonAsset->Bones[Index].Name == TargetName)
			{
				return Index;
			}
		}

		return -1;
	}

	int32 FindBodySetupIndex(const UPhysicsAsset* PhysicsAsset, const FName& BoneName)
	{
		if (!PhysicsAsset)
		{
			return -1;
		}

		const TArray<UPhysicsBodySetup*>& BodySetups = PhysicsAsset->GetBodySetups();
		for (int32 Index = 0; Index < static_cast<int32>(BodySetups.size()); ++Index)
		{
			if (BodySetups[Index] && BodySetups[Index]->GetTargetBoneName() == BoneName)
			{
				return Index;
			}
		}

		return -1;
	}

	FTransform* GetShapeLocalTransform(UPhysicsBodySetup* BodySetup, EPhysicsAssetPreviewShapeType ShapeType, int32 ShapeIndex)
	{
		if (!BodySetup || ShapeIndex < 0)
		{
			return nullptr;
		}

		FPhysicsAggregateShapeSetup& ShapeSetup = BodySetup->GetMutableShapeSetup();
		switch (ShapeType)
		{
		case EPhysicsAssetPreviewShapeType::Sphere:
			return ShapeIndex < static_cast<int32>(ShapeSetup.SphereShapeSetups.size()) ? &ShapeSetup.SphereShapeSetups[ShapeIndex].LocalTransform : nullptr;
		case EPhysicsAssetPreviewShapeType::Box:
			return ShapeIndex < static_cast<int32>(ShapeSetup.BoxShapeSetups.size()) ? &ShapeSetup.BoxShapeSetups[ShapeIndex].LocalTransform : nullptr;
		case EPhysicsAssetPreviewShapeType::Capsule:
			return ShapeIndex < static_cast<int32>(ShapeSetup.CapsuleShapeSetups.size()) ? &ShapeSetup.CapsuleShapeSetups[ShapeIndex].LocalTransform : nullptr;
		case EPhysicsAssetPreviewShapeType::Convex:
			return ShapeIndex < static_cast<int32>(ShapeSetup.ConvexShapeSetups.size()) ? &ShapeSetup.ConvexShapeSetups[ShapeIndex].LocalTransform : nullptr;
		case EPhysicsAssetPreviewShapeType::None:
		default:
			break;
		}

		return nullptr;
	}

	bool FindPreferredShapeSelection(UPhysicsBodySetup* BodySetup, EPhysicsAssetPreviewShapeType& OutShapeType, int32& OutShapeIndex)
	{
		if (!BodySetup)
		{
			return false;
		}

		const FPhysicsAggregateShapeSetup& ShapeSetup = BodySetup->GetShapeSetup();
		if (!ShapeSetup.CapsuleShapeSetups.empty())
		{
			OutShapeType = EPhysicsAssetPreviewShapeType::Capsule;
			OutShapeIndex = 0;
			return true;
		}
		if (!ShapeSetup.BoxShapeSetups.empty())
		{
			OutShapeType = EPhysicsAssetPreviewShapeType::Box;
			OutShapeIndex = 0;
			return true;
		}
		if (!ShapeSetup.SphereShapeSetups.empty())
		{
			OutShapeType = EPhysicsAssetPreviewShapeType::Sphere;
			OutShapeIndex = 0;
			return true;
		}
		OutShapeType = EPhysicsAssetPreviewShapeType::None;
		OutShapeIndex = -1;
		return false;
	}

	constexpr float PhysicsGraphGridStep = 32.0f;
	constexpr float PhysicsGraphNodeRounding = 6.0f;
	constexpr float PhysicsGraphNodeHeaderHeight = 24.0f;
	constexpr float PhysicsGraphBodyNodeWidth = 156.0f;
	constexpr float PhysicsGraphConstraintNodeWidth = 188.0f;
	constexpr float PhysicsGraphBodyNodeHeight = 78.0f;
	constexpr float PhysicsGraphConstraintNodeHeight = 58.0f;
	constexpr float PhysicsGraphMinZoom = 0.55f;
	constexpr float PhysicsGraphMaxZoom = 1.8f;
	constexpr float PhysicsGraphBodyColumnWidth = 270.0f;
	constexpr float PhysicsGraphRowHeight = 118.0f;

	enum class EPhysicsGraphNodeKind : uint8
	{
		Body,
		Constraint
	};

	struct FPhysicsGraphNodeVisual
	{
		FString NodeKey;
		FString Title;
		FString Subtitle;
		FString Detail;
		FVector2 Position;
		ImVec2 Size = ImVec2(0.0f, 0.0f);
		int32 LayoutIndex = -1;
		int32 DataIndex = -1;
		EPhysicsGraphNodeKind NodeKind = EPhysicsGraphNodeKind::Body;
		bool bSelected = false;
	};

	FString FitTextToWidth(const FString& Text, float MaxWidth, float TextScale = 1.0f)
	{
		if (Text.empty() || MaxWidth <= 0.0f)
		{
			return FString();
		}

		const float SafeTextScale = (std::max)(0.01f, TextScale);
		const float UnscaledMaxWidth = MaxWidth / SafeTextScale;
		if (ImGui::CalcTextSize(Text.c_str()).x <= UnscaledMaxWidth)
		{
			return Text;
		}

		const FString Ellipsis = "...";
		const float EllipsisWidth = ImGui::CalcTextSize(Ellipsis.c_str()).x;
		if (UnscaledMaxWidth <= EllipsisWidth)
		{
			return FString();
		}

		FString Result = Text;
		while (!Result.empty())
		{
			const FString Candidate = Result + Ellipsis;
			if (ImGui::CalcTextSize(Candidate.c_str()).x <= UnscaledMaxWidth)
			{
				return Candidate;
			}
			Result.pop_back();
		}

		return Ellipsis;
	}

	void DrawClippedText(ImDrawList* DrawList, const ImVec2& Pos, float MaxWidth, ImU32 Color, const FString& Text, float TextScale = 1.0f)
	{
		if (!DrawList)
		{
			return;
		}

		const float SafeTextScale = (std::max)(0.01f, TextScale);
		const FString FittedText = FitTextToWidth(Text, MaxWidth, SafeTextScale);
		if (!FittedText.empty())
		{
			DrawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * SafeTextScale, Pos, Color, FittedText.c_str());
		}
	}

	FString MakeBodyGraphNodeKey(const UPhysicsBodySetup* BodySetup)
	{
		return FString("Body:") + (BodySetup ? BodySetup->GetTargetBoneName().ToString() : FString("None"));
	}

	FString MakeConstraintGraphNodeKey(const FPhysicsConstraintSetup& ConstraintSetup)
	{
		return FString("Constraint:")
			+ ConstraintSetup.ConstraintName.ToString()
			+ ":"
			+ ConstraintSetup.ParentBoneName.ToString()
			+ ":"
			+ ConstraintSetup.ChildBoneName.ToString();
	}

	int32 CountBodyShapes(const UPhysicsBodySetup* BodySetup)
	{
		if (!BodySetup)
		{
			return 0;
		}

		const FPhysicsAggregateShapeSetup& ShapeSetup = BodySetup->GetShapeSetup();
		return static_cast<int32>(
			ShapeSetup.SphereShapeSetups.size()
			+ ShapeSetup.BoxShapeSetups.size()
			+ ShapeSetup.CapsuleShapeSetups.size());
	}

	int32 ComputeBoneDepth(const FSkeletonAsset* SkeletonAsset, int32 BoneIndex)
	{
		if (!SkeletonAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(SkeletonAsset->Bones.size()))
		{
			return 0;
		}

		int32 Depth = 0;
		int32 CurrentIndex = SkeletonAsset->Bones[BoneIndex].ParentIndex;
		while (CurrentIndex >= 0 && CurrentIndex < static_cast<int32>(SkeletonAsset->Bones.size()))
		{
			++Depth;
			CurrentIndex = SkeletonAsset->Bones[CurrentIndex].ParentIndex;
		}

		return Depth;
	}

	int32 GetOrCreateGraphNodeLayoutIndex(UPhysicsAsset* PhysicsAsset, const FString& NodeKey, const FVector2& DefaultPosition)
	{
		if (!PhysicsAsset)
		{
			return -1;
		}

		TArray<FPhysicsAssetGraphNodeLayout>& NodeLayouts = PhysicsAsset->GetMutableGraphViewState().NodeLayouts;
		for (int32 LayoutIndex = 0; LayoutIndex < static_cast<int32>(NodeLayouts.size()); ++LayoutIndex)
		{
			if (NodeLayouts[LayoutIndex].NodeKey == NodeKey)
			{
				return LayoutIndex;
			}
		}

		NodeLayouts.emplace_back();
		FPhysicsAssetGraphNodeLayout& NewLayout = NodeLayouts.back();
		NewLayout.NodeKey = NodeKey;
		NewLayout.Position = DefaultPosition;
		return static_cast<int32>(NodeLayouts.size()) - 1;
	}

	void FramePhysicsGraphNodes(const TArray<FPhysicsGraphNodeVisual>& GraphNodes, const ImVec2& CanvasSize, FPhysicsAssetGraphViewState& ViewState)
	{
		constexpr float GraphFrameMargin = 32.0f;
		if (GraphNodes.empty())
		{
			ViewState.Pan = FVector2(GraphFrameMargin, GraphFrameMargin);
			ViewState.Zoom = 1.0f;
			return;
		}

		float MinX = FLT_MAX;
		float MinY = FLT_MAX;
		float MaxX = -FLT_MAX;
		float MaxY = -FLT_MAX;
		for (const FPhysicsGraphNodeVisual& Node : GraphNodes)
		{
			MinX = (std::min)(MinX, Node.Position.X);
			MinY = (std::min)(MinY, Node.Position.Y);
			MaxX = (std::max)(MaxX, Node.Position.X + Node.Size.x);
			MaxY = (std::max)(MaxY, Node.Position.Y + Node.Size.y);
		}

		const float BoundsWidth = (std::max)(1.0f, MaxX - MinX);
		const float BoundsHeight = (std::max)(1.0f, MaxY - MinY);
		const float AvailableWidth = (std::max)(1.0f, CanvasSize.x - GraphFrameMargin * 2.0f);
		const float AvailableHeight = (std::max)(1.0f, CanvasSize.y - GraphFrameMargin * 2.0f);
		const float FitZoomX = AvailableWidth / BoundsWidth;
		const float FitZoomY = AvailableHeight / BoundsHeight;
		ViewState.Zoom = (std::max)(PhysicsGraphMinZoom, (std::min)((std::min)(FitZoomX, FitZoomY), PhysicsGraphMaxZoom));

		const float CenteredOffsetX = GraphFrameMargin + (std::max)(0.0f, (CanvasSize.x - GraphFrameMargin * 2.0f - BoundsWidth * ViewState.Zoom) * 0.5f);
		const float CenteredOffsetY = GraphFrameMargin + (std::max)(0.0f, (CanvasSize.y - GraphFrameMargin * 2.0f - BoundsHeight * ViewState.Zoom) * 0.5f);
		ViewState.Pan = FVector2(
			CenteredOffsetX - MinX * ViewState.Zoom,
			CenteredOffsetY - MinY * ViewState.Zoom);
	}

	ImVec2 PhysicsGraphToScreen(const FVector2& GraphPosition, const ImVec2& CanvasMin, const FPhysicsAssetGraphViewState& ViewState)
	{
		return ImVec2(
			CanvasMin.x + ViewState.Pan.X + GraphPosition.X * ViewState.Zoom,
			CanvasMin.y + ViewState.Pan.Y + GraphPosition.Y * ViewState.Zoom);
	}

	void ScreenToPhysicsGraph(const ImVec2& ScreenPos, const ImVec2& CanvasMin, const FPhysicsAssetGraphViewState& ViewState, float& OutX, float& OutY)
	{
		const float SafeZoom = (std::max)(0.01f, ViewState.Zoom);
		OutX = (ScreenPos.x - CanvasMin.x - ViewState.Pan.X) / SafeZoom;
		OutY = (ScreenPos.y - CanvasMin.y - ViewState.Pan.Y) / SafeZoom;
	}

	void DrawPhysicsGraphGrid(const ImVec2& CanvasMin, const ImVec2& CanvasMax, const FPhysicsAssetGraphViewState& ViewState, ImDrawList* DrawList)
	{
		if (!DrawList)
		{
			return;
		}

		DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(26, 27, 31, 255));
		const float Step = (std::max)(8.0f, PhysicsGraphGridStep * ViewState.Zoom);
		const float OffsetX = std::fmod(ViewState.Pan.X, Step);
		const float OffsetY = std::fmod(ViewState.Pan.Y, Step);
		for (float X = CanvasMin.x + OffsetX; X < CanvasMax.x; X += Step)
		{
			DrawList->AddLine(ImVec2(X, CanvasMin.y), ImVec2(X, CanvasMax.y), IM_COL32(43, 45, 51, 255));
		}
		for (float Y = CanvasMin.y + OffsetY; Y < CanvasMax.y; Y += Step)
		{
			DrawList->AddLine(ImVec2(CanvasMin.x, Y), ImVec2(CanvasMax.x, Y), IM_COL32(43, 45, 51, 255));
		}
		DrawList->AddRect(CanvasMin, CanvasMax, IM_COL32(70, 73, 82, 255));
	}

	class FPhysicsBodyGizmoTarget : public IGizmoTransformTarget
	{
	public:
		void Clear()
		{
			MeshComponent = nullptr;
			PhysicsAsset = nullptr;
			BodyIndex = -1;
			ShapeType = EPhysicsAssetPreviewShapeType::None;
			ShapeIndex = -1;
		}

		void SetSelection(
			USkeletalMeshComponent* InMeshComponent,
			UPhysicsAsset* InPhysicsAsset,
			int32 InBodyIndex,
			EPhysicsAssetPreviewShapeType InShapeType,
			int32 InShapeIndex)
		{
			MeshComponent = InMeshComponent;
			PhysicsAsset = InPhysicsAsset;
			BodyIndex = InBodyIndex;
			ShapeType = InShapeType;
			ShapeIndex = InShapeIndex;
		}

		bool IsValid() const override
		{
			return MeshComponent != nullptr && GetEditableTransform() != nullptr;
		}

		UWorld* GetWorld() const override
		{
			return MeshComponent ? MeshComponent->GetWorld() : nullptr;
		}

		FVector GetWorldLocation() const override
		{
			return GetWorldTransform().Location;
		}

		FRotator GetWorldRotation() const override
		{
			return GetWorldTransform().GetRotator();
		}

		FQuat GetWorldQuat() const override
		{
			return GetWorldTransform().Rotation;
		}

		FVector GetWorldScale() const override
		{
			return GetWorldTransform().Scale;
		}

		void SetWorldLocation(const FVector& NewLocation) override
		{
			FTransform WorldTransform = GetWorldTransform();
			WorldTransform.Location = NewLocation;
			SetWorldTransform(WorldTransform);
		}

		void SetWorldRotation(const FRotator& NewRotation) override
		{
			FTransform WorldTransform = GetWorldTransform();
			WorldTransform.SetRotation(NewRotation);
			SetWorldTransform(WorldTransform);
		}

		void SetWorldRotation(const FQuat& NewQuat) override
		{
			FTransform WorldTransform = GetWorldTransform();
			WorldTransform.SetRotation(NewQuat);
			SetWorldTransform(WorldTransform);
		}

		void SetWorldScale(const FVector& NewScale) override
		{
			FTransform WorldTransform = GetWorldTransform();
			WorldTransform.Scale = NewScale;
			SetWorldTransform(WorldTransform);
		}

		void AddWorldOffset(const FVector& Delta) override
		{
			SetWorldLocation(GetWorldLocation() + Delta);
		}

		void AddWorldRotation(const FQuat& Delta, bool bWorldSpace) override
		{
			const FQuat Current = GetWorldQuat();
			SetWorldRotation(bWorldSpace ? (Delta * Current) : (Current * Delta));
		}

		void AddScaleDelta(const FVector& Delta) override
		{
			SetWorldScale(GetWorldScale() + Delta);
		}

	private:
		UPhysicsBodySetup* GetBodySetup() const
		{
			if (!PhysicsAsset || BodyIndex < 0 || BodyIndex >= static_cast<int32>(PhysicsAsset->GetBodySetups().size()))
			{
				return nullptr;
			}

			return PhysicsAsset->GetBodySetups()[BodyIndex];
		}

		FTransform* GetEditableTransform() const
		{
			UPhysicsBodySetup* BodySetup = GetBodySetup();
			if (!BodySetup)
			{
				return nullptr;
			}

			if (FTransform* ShapeTransform = GetShapeLocalTransform(BodySetup, ShapeType, ShapeIndex))
			{
				return ShapeTransform;
			}

			EPhysicsAssetPreviewShapeType FallbackType = EPhysicsAssetPreviewShapeType::None;
			int32 FallbackIndex = -1;
			if (!FindPreferredShapeSelection(BodySetup, FallbackType, FallbackIndex))
			{
				return nullptr;
			}

			return GetShapeLocalTransform(BodySetup, FallbackType, FallbackIndex);
		}

		int32 GetBoneIndex() const
		{
			if (!MeshComponent)
			{
				return -1;
			}

			USkeletalMesh* SkeletalMesh = MeshComponent->GetSkeletalMesh();
			const FSkeletonAsset* SkeletonAsset = SkeletalMesh ? SkeletalMesh->GetSkeletonAsset() : nullptr;
			const UPhysicsBodySetup* BodySetup = GetBodySetup();
			return BodySetup ? FindSkeletonBoneIndexByName(SkeletonAsset, BodySetup->GetTargetBoneName()) : -1;
		}

		FMatrix GetBoneWorldMatrix() const
		{
			const int32 BoneIndex = GetBoneIndex();
			return BuildBoneWorldMatrix(MeshComponent, nullptr, BoneIndex);
		}

		FTransform GetWorldTransform() const
		{
			if (const FTransform* LocalTransform = GetEditableTransform())
			{
				return FTransform(LocalTransform->ToMatrix() * GetBoneWorldMatrix());
			}

			return FTransform();
		}

		void SetWorldTransform(const FTransform& NewWorldTransform)
		{
			if (FTransform* LocalTransform = GetEditableTransform())
			{
				const FMatrix LocalMatrix = NewWorldTransform.ToMatrix() * GetBoneWorldMatrix().GetInverse();
				*LocalTransform = FTransform(LocalMatrix);
			}
		}

	private:
		USkeletalMeshComponent* MeshComponent = nullptr;
		UPhysicsAsset* PhysicsAsset = nullptr;
		int32 BodyIndex = -1;
		EPhysicsAssetPreviewShapeType ShapeType = EPhysicsAssetPreviewShapeType::None;
		int32 ShapeIndex = -1;
	};

	class FPhysicsConstraintGizmoTarget : public IGizmoTransformTarget
	{
	public:
		void Clear()
		{
			MeshComponent = nullptr;
			PhysicsAsset = nullptr;
			ConstraintIndex = -1;
		}

		void SetSelection(USkeletalMeshComponent* InMeshComponent, UPhysicsAsset* InPhysicsAsset, int32 InConstraintIndex)
		{
			MeshComponent = InMeshComponent;
			PhysicsAsset = InPhysicsAsset;
			ConstraintIndex = InConstraintIndex;
		}

		bool IsValid() const override
		{
			return MeshComponent != nullptr && GetConstraintSetup() != nullptr;
		}

		UWorld* GetWorld() const override
		{
			return MeshComponent ? MeshComponent->GetWorld() : nullptr;
		}

		FVector GetWorldLocation() const override
		{
			return GetWorldTransform().Location;
		}

		FRotator GetWorldRotation() const override
		{
			return GetWorldTransform().GetRotator();
		}

		FQuat GetWorldQuat() const override
		{
			return GetWorldTransform().Rotation;
		}

		FVector GetWorldScale() const override
		{
			return FVector::OneVector;
		}

		void SetWorldLocation(const FVector& NewLocation) override
		{
			FTransform WorldTransform = GetWorldTransform();
			WorldTransform.Location = NewLocation;
			SetWorldTransform(WorldTransform);
		}

		void SetWorldRotation(const FRotator& NewRotation) override
		{
			FTransform WorldTransform = GetWorldTransform();
			WorldTransform.SetRotation(NewRotation);
			SetWorldTransform(WorldTransform);
		}

		void SetWorldRotation(const FQuat& NewQuat) override
		{
			FTransform WorldTransform = GetWorldTransform();
			WorldTransform.SetRotation(NewQuat);
			SetWorldTransform(WorldTransform);
		}

		void SetWorldScale(const FVector& NewScale) override
		{
			(void)NewScale;
		}

		void AddWorldOffset(const FVector& Delta) override
		{
			SetWorldLocation(GetWorldLocation() + Delta);
		}

		void AddWorldRotation(const FQuat& Delta, bool bWorldSpace) override
		{
			const FQuat Current = GetWorldQuat();
			SetWorldRotation(bWorldSpace ? (Delta * Current) : (Current * Delta));
		}

		void AddScaleDelta(const FVector& Delta) override
		{
			(void)Delta;
		}

	private:
		FPhysicsConstraintSetup* GetConstraintSetup() const
		{
			if (!PhysicsAsset || ConstraintIndex < 0 || ConstraintIndex >= static_cast<int32>(PhysicsAsset->GetConstraintSetups().size()))
			{
				return nullptr;
			}

			return &PhysicsAsset->GetMutableConstraintSetups()[ConstraintIndex];
		}

		int32 GetBoneIndex(const FName& BoneName) const
		{
			if (!MeshComponent)
			{
				return -1;
			}

			USkeletalMesh* SkeletalMesh = MeshComponent->GetSkeletalMesh();
			const FSkeletonAsset* SkeletonAsset = SkeletalMesh ? SkeletalMesh->GetSkeletonAsset() : nullptr;
			return FindSkeletonBoneIndexByName(SkeletonAsset, BoneName);
		}

		FMatrix GetParentBoneWorldMatrix(const FPhysicsConstraintSetup& Constraint) const
		{
			return BuildBoneWorldMatrix(MeshComponent, nullptr, GetBoneIndex(Constraint.ParentBoneName));
		}

		FMatrix GetChildBoneWorldMatrix(const FPhysicsConstraintSetup& Constraint) const
		{
			return BuildBoneWorldMatrix(MeshComponent, nullptr, GetBoneIndex(Constraint.ChildBoneName));
		}

		FTransform GetWorldTransform() const
		{
			FPhysicsConstraintSetup* Constraint = GetConstraintSetup();
			if (!Constraint)
			{
				return FTransform();
			}

			const FTransform ParentFrame(Constraint->ParentLocalFrame.ToMatrix() * GetParentBoneWorldMatrix(*Constraint));
			const FTransform ChildFrame(Constraint->ChildLocalFrame.ToMatrix() * GetChildBoneWorldMatrix(*Constraint));

			FTransform Result = ParentFrame;
			Result.Location = (ParentFrame.Location + ChildFrame.Location) * 0.5f;
			Result.Scale = FVector::OneVector;
			return Result;
		}

		void SetWorldTransform(const FTransform& NewWorldTransform)
		{
			FPhysicsConstraintSetup* Constraint = GetConstraintSetup();
			if (!Constraint)
			{
				return;
			}

			FTransform JointWorld = NewWorldTransform;
			JointWorld.Scale = FVector::OneVector;

			Constraint->ParentLocalFrame = FTransform(JointWorld.ToMatrix() * GetParentBoneWorldMatrix(*Constraint).GetInverse());
			Constraint->ChildLocalFrame = FTransform(JointWorld.ToMatrix() * GetChildBoneWorldMatrix(*Constraint).GetInverse());
		}

	private:
		USkeletalMeshComponent* MeshComponent = nullptr;
		UPhysicsAsset* PhysicsAsset = nullptr;
		int32 ConstraintIndex = -1;
	};

	void AddFallbackBodyForBone(UPhysicsAsset* PhysicsAsset, const FString& BoneName)
	{
		if (!PhysicsAsset)
		{
			return;
		}

		UPhysicsBodySetup* BodySetup = GUObjectArray.CreateObject<UPhysicsBodySetup>(PhysicsAsset);
		BodySetup->SetTargetBoneName(FName(BoneName));
		BodySetup->SetBodyType(EPhysicsBodyType::PBT_Dynamic);
		BodySetup->SetCollisionDesc(FPhysicsCollisionDesc());

		FPhysicsCapsuleShapeSetup CapsuleShape;
		CapsuleShape.Name = FName(BoneName + "_Capsule");
		CapsuleShape.Radius = 8.0f;
		CapsuleShape.Length = 16.0f;
		BodySetup->GetMutableShapeSetup().CapsuleShapeSetups.push_back(CapsuleShape);

		PhysicsAsset->AddBodySetup(BodySetup);
	}

	FNormalVertex LerpPreviewVertex(const FNormalVertex& A, const FNormalVertex& B, float Alpha)
	{
		const float T = std::clamp(Alpha, 0.0f, 1.0f);

		FNormalVertex Result;
		Result.pos = A.pos + (B.pos - A.pos) * T;
		Result.normal = A.normal + (B.normal - A.normal) * T;
		if (!Result.normal.IsNearlyZero())
		{
			Result.normal.Normalize();
		}

		Result.color = A.color + (B.color - A.color) * T;
		Result.tex = A.tex + (B.tex - A.tex) * T;
		Result.tangent = A.tangent + (B.tangent - A.tangent) * T;

		FVector TangentDirection(Result.tangent.X, Result.tangent.Y, Result.tangent.Z);
		if (!TangentDirection.IsNearlyZero())
		{
			TangentDirection.Normalize();
			Result.tangent.X = TangentDirection.X;
			Result.tangent.Y = TangentDirection.Y;
			Result.tangent.Z = TangentDirection.Z;
		}

		return Result;
	}

	FNormalVertex IntersectPreviewVertexAtXPlane(const FNormalVertex& A, const FNormalVertex& B)
	{
		const float Denominator = A.pos.X - B.pos.X;
		if (std::abs(Denominator) <= 1.0e-6f)
		{
			return A;
		}

		return LerpPreviewVertex(A, B, A.pos.X / Denominator);
	}

	void ClipTriangleToPositiveXHalfSpace(const FNormalVertex& A, const FNormalVertex& B, const FNormalVertex& C, TArray<FNormalVertex>& OutTriangles)
	{
		constexpr float PlaneTolerance = 1.0e-4f;

		TArray<FNormalVertex> Polygon = { A, B, C };
		TArray<FNormalVertex> Clipped;
		Clipped.reserve(4);

		for (int32 Index = 0; Index < static_cast<int32>(Polygon.size()); ++Index)
		{
			const FNormalVertex& Current = Polygon[Index];
			const FNormalVertex& Next = Polygon[(Index + 1) % Polygon.size()];

			const bool bCurrentInside = Current.pos.X >= -PlaneTolerance;
			const bool bNextInside = Next.pos.X >= -PlaneTolerance;

			if (bCurrentInside && bNextInside)
			{
				Clipped.push_back(Next);
			}
			else if (bCurrentInside && !bNextInside)
			{
				Clipped.push_back(IntersectPreviewVertexAtXPlane(Current, Next));
			}
			else if (!bCurrentInside && bNextInside)
			{
				Clipped.push_back(IntersectPreviewVertexAtXPlane(Current, Next));
				Clipped.push_back(Next);
			}
		}

		if (Clipped.size() < 3)
		{
			return;
		}

		for (int32 VertexIndex = 1; VertexIndex + 1 < static_cast<int32>(Clipped.size()); ++VertexIndex)
		{
			OutTriangles.push_back(Clipped[0]);
			OutTriangles.push_back(Clipped[VertexIndex]);
			OutTriangles.push_back(Clipped[VertexIndex + 1]);
		}
	}

	UStaticMesh* CreatePositiveXHemispherePreviewMesh(UStaticMesh* SourceSphereMesh, ID3D11Device* Device)
	{
		if (!SourceSphereMesh || !Device)
		{
			return nullptr;
		}

		const FStaticMesh* SourceMeshAsset = SourceSphereMesh->GetStaticMeshAsset();
		if (!SourceMeshAsset || SourceMeshAsset->Vertices.empty() || SourceMeshAsset->Indices.empty())
		{
			return nullptr;
		}

		UStaticMesh* HemisphereMesh = GUObjectArray.CreateObject<UStaticMesh>();
		if (!HemisphereMesh)
		{
			return nullptr;
		}

		FStaticMesh* HemisphereMeshAsset = new FStaticMesh();
		HemisphereMeshAsset->PathFileName = "__PreviewHemisphere__";

		TArray<FStaticMaterial> Materials = SourceSphereMesh->GetStaticMaterials();
		HemisphereMesh->SetStaticMaterials(std::move(Materials));

		TArray<FStaticMeshSection> SourceSections = SourceMeshAsset->Sections;
		if (SourceSections.empty())
		{
			FStaticMeshSection Section;
			Section.MaterialSlotName = HemisphereMesh->GetStaticMaterials().empty()
				? FString("None")
				: HemisphereMesh->GetStaticMaterials()[0].MaterialSlotName;
			Section.FirstIndex = 0;
			Section.NumTriangles = static_cast<uint32>(SourceMeshAsset->Indices.size() / 3);
			SourceSections.push_back(Section);
		}

		for (const FStaticMeshSection& SourceSection : SourceSections)
		{
			FStaticMeshSection HemisphereSection = SourceSection;
			HemisphereSection.FirstIndex = static_cast<uint32>(HemisphereMeshAsset->Indices.size());
			HemisphereSection.NumTriangles = 0;

			const uint32 SectionStart = SourceSection.FirstIndex;
			const uint32 SectionEnd = (std::min)(
				SectionStart + SourceSection.NumTriangles * 3u,
				static_cast<uint32>(SourceMeshAsset->Indices.size()));

			for (uint32 Index = SectionStart; Index + 2 < SectionEnd; Index += 3)
			{
				const uint32 Index0 = SourceMeshAsset->Indices[Index];
				const uint32 Index1 = SourceMeshAsset->Indices[Index + 1];
				const uint32 Index2 = SourceMeshAsset->Indices[Index + 2];
				if (Index0 >= SourceMeshAsset->Vertices.size()
					|| Index1 >= SourceMeshAsset->Vertices.size()
					|| Index2 >= SourceMeshAsset->Vertices.size())
				{
					continue;
				}

				TArray<FNormalVertex> ClippedTriangles;
				ClipTriangleToPositiveXHalfSpace(
					SourceMeshAsset->Vertices[Index0],
					SourceMeshAsset->Vertices[Index1],
					SourceMeshAsset->Vertices[Index2],
					ClippedTriangles);

				for (int32 TriangleVertex = 0; TriangleVertex + 2 < static_cast<int32>(ClippedTriangles.size()); TriangleVertex += 3)
				{
					const uint32 BaseVertexIndex = static_cast<uint32>(HemisphereMeshAsset->Vertices.size());
					HemisphereMeshAsset->Vertices.push_back(ClippedTriangles[TriangleVertex]);
					HemisphereMeshAsset->Vertices.push_back(ClippedTriangles[TriangleVertex + 1]);
					HemisphereMeshAsset->Vertices.push_back(ClippedTriangles[TriangleVertex + 2]);
					HemisphereMeshAsset->Indices.push_back(BaseVertexIndex);
					HemisphereMeshAsset->Indices.push_back(BaseVertexIndex + 1);
					HemisphereMeshAsset->Indices.push_back(BaseVertexIndex + 2);
					++HemisphereSection.NumTriangles;
				}
			}

			if (HemisphereSection.NumTriangles > 0)
			{
				HemisphereMeshAsset->Sections.push_back(HemisphereSection);
			}
		}

		if (HemisphereMeshAsset->Vertices.empty())
		{
			delete HemisphereMeshAsset;
			GUObjectArray.DestroyObject(HemisphereMesh);
			return nullptr;
		}

		HemisphereMeshAsset->CacheBounds();
		HemisphereMesh->SetAssetPathFileName("__PreviewHemisphere__");
		HemisphereMesh->SetStaticMeshAsset(HemisphereMeshAsset);
		HemisphereMesh->InitResources(Device);
		return HemisphereMesh;
	}

	FVector GetStaticMeshBoundsExtent(UStaticMesh* StaticMesh)
	{
		if (!StaticMesh)
		{
			return FVector::OneVector;
		}

		FStaticMesh* MeshAsset = StaticMesh->GetStaticMeshAsset();
		if (!MeshAsset)
		{
			return FVector::OneVector;
		}

		if (!MeshAsset->bBoundsValid)
		{
			MeshAsset->CacheBounds();
		}

		return MeshAsset->bBoundsValid ? MeshAsset->BoundsExtent : FVector::OneVector;
	}

	int32 GetLeastNormalAxisIndex(UStaticMesh* StaticMesh)
	{
		auto GetLargestExtentAxisIndex =
			[](const FVector& Extent)
			{
				const float AbsX = std::abs(Extent.X);
				const float AbsY = std::abs(Extent.Y);
				const float AbsZ = std::abs(Extent.Z);
				if (AbsX >= AbsY && AbsX >= AbsZ)
				{
					return 0;
				}
				if (AbsY >= AbsZ)
				{
					return 1;
				}
				return 2;
			};

		if (!StaticMesh)
		{
			return 0;
		}

		const FStaticMesh* MeshAsset = StaticMesh->GetStaticMeshAsset();
		if (!MeshAsset || MeshAsset->Vertices.empty())
		{
			return GetLargestExtentAxisIndex(GetStaticMeshBoundsExtent(StaticMesh));
		}

		const FVector BoundsExtent = GetStaticMeshBoundsExtent(StaticMesh);
		const float MaxExtent = (std::max)(BoundsExtent.X, (std::max)(BoundsExtent.Y, BoundsExtent.Z));
		const float QuantizeStep = (std::max)(0.001f, MaxExtent * 0.02f);
		TArray<int32> QuantizedLevels[3];
		FVector SumAbsNormal(0.0f, 0.0f, 0.0f);
		int32 NormalCount = 0;
		for (const FNormalVertex& Vertex : MeshAsset->Vertices)
		{
			QuantizedLevels[0].push_back(static_cast<int32>(std::round(Vertex.pos.X / QuantizeStep)));
			QuantizedLevels[1].push_back(static_cast<int32>(std::round(Vertex.pos.Y / QuantizeStep)));
			QuantizedLevels[2].push_back(static_cast<int32>(std::round(Vertex.pos.Z / QuantizeStep)));

			if (Vertex.normal.IsNearlyZero())
			{
				continue;
			}

			SumAbsNormal.X += std::fabs(Vertex.normal.X);
			SumAbsNormal.Y += std::fabs(Vertex.normal.Y);
			SumAbsNormal.Z += std::fabs(Vertex.normal.Z);
			++NormalCount;
		}

		if (NormalCount <= 0)
		{
			return GetLargestExtentAxisIndex(GetStaticMeshBoundsExtent(StaticMesh));
		}

		auto GetUniqueCount =
			[](TArray<int32>& Values)
			{
				std::sort(Values.begin(), Values.end());
				Values.erase(std::unique(Values.begin(), Values.end()), Values.end());
				return static_cast<int32>(Values.size());
			};

		const int32 UniqueCountX = GetUniqueCount(QuantizedLevels[0]);
		const int32 UniqueCountY = GetUniqueCount(QuantizedLevels[1]);
		const int32 UniqueCountZ = GetUniqueCount(QuantizedLevels[2]);
		if (UniqueCountX < UniqueCountY && UniqueCountX < UniqueCountZ)
		{
			return 0;
		}
		if (UniqueCountY < UniqueCountZ && UniqueCountY < UniqueCountX)
		{
			return 1;
		}
		if (UniqueCountZ < UniqueCountX && UniqueCountZ < UniqueCountY)
		{
			return 2;
		}

		if (SumAbsNormal.X <= SumAbsNormal.Y && SumAbsNormal.X <= SumAbsNormal.Z)
		{
			return 0;
		}
		if (SumAbsNormal.Y <= SumAbsNormal.Z)
		{
			return 1;
		}
		return 2;
	}

	FTransform ComposeMatrixToTransform(const FMatrix& LocalMatrix, const FMatrix& ParentMatrix)
	{
		return FTransform(LocalMatrix * ParentMatrix);
	}

		FMatrix BuildBoneWorldMatrix(const USkeletalMeshComponent* MeshComponent, const TArray<FMatrix>* CachedBoneGlobals, int32 BoneIndex)
		{
		if (!MeshComponent)
		{
			return FMatrix::Identity;
		}

		const FMatrix& MeshWorldMatrix = MeshComponent->GetWorldMatrix();
		if (CachedBoneGlobals
			&& BoneIndex >= 0
			&& BoneIndex < static_cast<int32>(CachedBoneGlobals->size()))
		{
			return (*CachedBoneGlobals)[BoneIndex] * MeshWorldMatrix;
		}

		if (BoneIndex >= 0)
		{
			TArray<FMatrix> BoneGlobals;
			MeshComponent->GetCurrentBoneGlobalMatrices(BoneGlobals);
			if (BoneIndex < static_cast<int32>(BoneGlobals.size()))
			{
				return BoneGlobals[BoneIndex] * MeshWorldMatrix;
			}
		}

			return MeshWorldMatrix;
		}

	int32 GetDominantAxisIndex(const FVector& Value)
	{
		const float AbsX = std::abs(Value.X);
		const float AbsY = std::abs(Value.Y);
		const float AbsZ = std::abs(Value.Z);
		if (AbsX >= AbsY && AbsX >= AbsZ)
		{
			return 0;
		}
		if (AbsY >= AbsZ)
		{
			return 1;
		}
		return 2;
	}

	FVector MakeAxisAlignedExtent(int32 DominantAxisIndex, float Radius, float HalfLength)
	{
		FVector Extent(Radius, Radius, Radius);
		if (DominantAxisIndex == 0)
		{
			Extent.X = HalfLength;
		}
		else if (DominantAxisIndex == 1)
		{
			Extent.Y = HalfLength;
		}
		else
		{
			Extent.Z = HalfLength;
		}
		return Extent;
	}

	FVector MakeAxisUnitVector(int32 DominantAxisIndex)
	{
		if (DominantAxisIndex == 0)
		{
			return FVector::ForwardVector;
		}
		if (DominantAxisIndex == 1)
		{
			return FVector::RightVector;
		}
		return FVector::UpVector;
	}

	FQuat MakeAxisAlignmentQuat(const FVector& FromAxis, const FVector& ToAxis)
	{
		FVector SafeFromAxis = FromAxis;
		FVector SafeToAxis = ToAxis;
		if (SafeFromAxis.IsNearlyZero() || SafeToAxis.IsNearlyZero())
		{
			return FQuat::Identity;
		}

		SafeFromAxis.Normalize();
		SafeToAxis.Normalize();

		const float AxisDot = std::clamp(SafeFromAxis.Dot(SafeToAxis), -1.0f, 1.0f);
		if (AxisDot >= 1.0f - 1.0e-4f)
		{
			return FQuat::Identity;
		}

		if (AxisDot <= -1.0f + 1.0e-4f)
		{
			FVector RotationAxis = SafeFromAxis.Cross(FVector::ForwardVector);
			if (RotationAxis.IsNearlyZero())
			{
				RotationAxis = SafeFromAxis.Cross(FVector::RightVector);
			}
			if (RotationAxis.IsNearlyZero())
			{
				RotationAxis = SafeFromAxis.Cross(FVector::UpVector);
			}

			RotationAxis.Normalize();
			return FQuat::FromAxisAngle(RotationAxis, Pi);
		}

		const FVector RotationAxis = SafeFromAxis.Cross(SafeToAxis);
		FQuat Result(RotationAxis.X, RotationAxis.Y, RotationAxis.Z, 1.0f + AxisDot);
		Result.Normalize();
		return Result;
	}

	FMatrix MakeAxisAlignmentMatrix(const FVector& FromAxis, const FVector& ToAxis)
	{
		return FTransform(
			FVector::ZeroVector,
			MakeAxisAlignmentQuat(FromAxis, ToAxis),
			FVector::OneVector).ToMatrix();
	}

	FMatrix MakeMeshAxisToTargetAxisMatrix(int32 DominantAxisIndex, const FVector& TargetAxis)
	{
		return MakeAxisAlignmentMatrix(MakeAxisUnitVector(DominantAxisIndex), TargetAxis);
	}

	void AppendPreviewTriangle(FStaticMesh* MeshAsset, FStaticMeshSection& Section, const FNormalVertex& A, const FNormalVertex& B, const FNormalVertex& C)
	{
		if (!MeshAsset)
		{
			return;
		}

		const uint32 BaseVertexIndex = static_cast<uint32>(MeshAsset->Vertices.size());
		MeshAsset->Vertices.push_back(A);
		MeshAsset->Vertices.push_back(B);
		MeshAsset->Vertices.push_back(C);
		MeshAsset->Indices.push_back(BaseVertexIndex);
		MeshAsset->Indices.push_back(BaseVertexIndex + 1);
		MeshAsset->Indices.push_back(BaseVertexIndex + 2);
		++Section.NumTriangles;
	}

	FVector ComputePreviewTriangleNormal(const FNormalVertex& A, const FNormalVertex& B, const FNormalVertex& C)
	{
		FVector TriangleNormal = (B.pos - A.pos).Cross(C.pos - A.pos);
		if (!TriangleNormal.IsNearlyZero())
		{
			TriangleNormal.Normalize();
		}
		return TriangleNormal;
	}

	UStaticMesh* CreateOpenCylinderPreviewMesh(UStaticMesh* SourceCylinderMesh, ID3D11Device* Device)
	{
		if (!SourceCylinderMesh || !Device)
		{
			return nullptr;
		}

		const FStaticMesh* SourceMeshAsset = SourceCylinderMesh->GetStaticMeshAsset();
		if (!SourceMeshAsset || SourceMeshAsset->Vertices.empty() || SourceMeshAsset->Indices.empty())
		{
			return nullptr;
		}

		UStaticMesh* OpenCylinderMesh = GUObjectArray.CreateObject<UStaticMesh>();
		if (!OpenCylinderMesh)
		{
			return nullptr;
		}

		FStaticMesh* OpenCylinderMeshAsset = new FStaticMesh();
		OpenCylinderMeshAsset->PathFileName = "__PreviewOpenCylinder__";

		TArray<FStaticMaterial> Materials = SourceCylinderMesh->GetStaticMaterials();
		OpenCylinderMesh->SetStaticMaterials(std::move(Materials));

		TArray<FStaticMeshSection> SourceSections = SourceMeshAsset->Sections;
		if (SourceSections.empty())
		{
			FStaticMeshSection Section;
			Section.MaterialSlotName = OpenCylinderMesh->GetStaticMaterials().empty()
				? FString("None")
				: OpenCylinderMesh->GetStaticMaterials()[0].MaterialSlotName;
			Section.FirstIndex = 0;
			Section.NumTriangles = static_cast<uint32>(SourceMeshAsset->Indices.size() / 3);
			SourceSections.push_back(Section);
		}

		const int32 DominantAxisIndex = GetLeastNormalAxisIndex(SourceCylinderMesh);
		const FVector AxisVector = MakeAxisUnitVector(DominantAxisIndex);
		constexpr float CapNormalThreshold = 0.55f;

		for (const FStaticMeshSection& SourceSection : SourceSections)
		{
			FStaticMeshSection OpenCylinderSection = SourceSection;
			OpenCylinderSection.FirstIndex = static_cast<uint32>(OpenCylinderMeshAsset->Indices.size());
			OpenCylinderSection.NumTriangles = 0;

			const uint32 SectionStart = SourceSection.FirstIndex;
			const uint32 SectionEnd = (std::min)(
				SectionStart + SourceSection.NumTriangles * 3u,
				static_cast<uint32>(SourceMeshAsset->Indices.size()));

			for (uint32 Index = SectionStart; Index + 2 < SectionEnd; Index += 3)
			{
				const uint32 Index0 = SourceMeshAsset->Indices[Index];
				const uint32 Index1 = SourceMeshAsset->Indices[Index + 1];
				const uint32 Index2 = SourceMeshAsset->Indices[Index + 2];
				if (Index0 >= SourceMeshAsset->Vertices.size()
					|| Index1 >= SourceMeshAsset->Vertices.size()
					|| Index2 >= SourceMeshAsset->Vertices.size())
				{
					continue;
				}

				const FNormalVertex& Vertex0 = SourceMeshAsset->Vertices[Index0];
				const FNormalVertex& Vertex1 = SourceMeshAsset->Vertices[Index1];
				const FNormalVertex& Vertex2 = SourceMeshAsset->Vertices[Index2];
				const FVector TriangleNormal = ComputePreviewTriangleNormal(Vertex0, Vertex1, Vertex2);
				if (std::fabs(TriangleNormal.Dot(AxisVector)) >= CapNormalThreshold)
				{
					continue;
				}

				AppendPreviewTriangle(OpenCylinderMeshAsset, OpenCylinderSection, Vertex0, Vertex1, Vertex2);
			}

			if (OpenCylinderSection.NumTriangles > 0)
			{
				OpenCylinderMeshAsset->Sections.push_back(OpenCylinderSection);
			}
		}

		if (OpenCylinderMeshAsset->Vertices.empty())
		{
			delete OpenCylinderMeshAsset;
			GUObjectArray.DestroyObject(OpenCylinderMesh);
			return nullptr;
		}

		OpenCylinderMeshAsset->CacheBounds();
		OpenCylinderMesh->SetAssetPathFileName("__PreviewOpenCylinder__");
		OpenCylinderMesh->SetStaticMeshAsset(OpenCylinderMeshAsset);
		OpenCylinderMesh->InitResources(Device);
		return OpenCylinderMesh;
	}

	UStaticMesh* CreateCanonicalCapsulePreviewMesh(UStaticMesh* MaterialSourceMesh, ID3D11Device* Device)
	{
		if (!Device)
		{
			return nullptr;
		}

		constexpr float CapsuleRadius = 1.0f;
		constexpr float CylinderHalfLength = 1.0f;
		constexpr float Pi = 3.14159265358979323846f;
		constexpr int32 CircleSegments = 24;
		constexpr int32 HemisphereSegments = 8;

		UStaticMesh* CapsuleMesh = GUObjectArray.CreateObject<UStaticMesh>();
		if (!CapsuleMesh)
		{
			return nullptr;
		}

		FStaticMesh* CapsuleMeshAsset = new FStaticMesh();
		CapsuleMeshAsset->PathFileName = "__PreviewCapsule__";

		TArray<FStaticMaterial> Materials = MaterialSourceMesh ? MaterialSourceMesh->GetStaticMaterials() : TArray<FStaticMaterial>();
		CapsuleMesh->SetStaticMaterials(std::move(Materials));

		FStaticMeshSection CapsuleSection;
		CapsuleSection.MaterialSlotName = CapsuleMesh->GetStaticMaterials().empty()
			? FString("None")
			: CapsuleMesh->GetStaticMaterials()[0].MaterialSlotName;
		CapsuleSection.FirstIndex = 0;
		CapsuleSection.NumTriangles = 0;

		auto MakeVertex =
			[Pi](float X, float RingRadius, float Theta, float NormalX, float NormalRadiusScale)
			{
				FNormalVertex Vertex{};
				const float CosTheta = cosf(Theta);
				const float SinTheta = sinf(Theta);
				Vertex.pos = FVector(X, CosTheta * RingRadius, SinTheta * RingRadius);

				FVector Normal(NormalX, CosTheta * NormalRadiusScale, SinTheta * NormalRadiusScale);
				if (!Normal.IsNearlyZero())
				{
					Normal.Normalize();
				}
				Vertex.normal = Normal;
				Vertex.color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
				Vertex.tex = FVector2(Theta / (2.0f * Pi), X);

				FVector Tangent(0.0f, -SinTheta, CosTheta);
				if (Tangent.IsNearlyZero())
				{
					Tangent = FVector::RightVector;
				}
				else
				{
					Tangent.Normalize();
				}
				Vertex.tangent = FVector4(Tangent, 1.0f);
				return Vertex;
			};

		auto MakeTipVertex =
			[](float X, float NormalX)
			{
				FNormalVertex Vertex{};
				Vertex.pos = FVector(X, 0.0f, 0.0f);
				Vertex.normal = FVector(NormalX, 0.0f, 0.0f);
				Vertex.color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
				Vertex.tex = FVector2(0.0f, X);
				Vertex.tangent = FVector4(0.0f, 1.0f, 0.0f, 1.0f);
				return Vertex;
			};

		auto AddOrientedTriangle =
			[&](const FNormalVertex& A, const FNormalVertex& B, const FNormalVertex& C)
			{
				FNormalVertex Vertex0 = A;
				FNormalVertex Vertex1 = B;
				FNormalVertex Vertex2 = C;

				const FVector DesiredNormal = Vertex0.normal + Vertex1.normal + Vertex2.normal;
				const FVector TriangleNormal = (Vertex1.pos - Vertex0.pos).Cross(Vertex2.pos - Vertex0.pos);
				if (!DesiredNormal.IsNearlyZero() && TriangleNormal.Dot(DesiredNormal) < 0.0f)
				{
					std::swap(Vertex1, Vertex2);
				}

				AppendPreviewTriangle(CapsuleMeshAsset, CapsuleSection, Vertex0, Vertex1, Vertex2);
			};

		TArray<TArray<FNormalVertex>> Rings;
		Rings.reserve(HemisphereSegments * 2 + 4);

		Rings.push_back({ MakeTipVertex(-(CylinderHalfLength + CapsuleRadius), -1.0f) });
		for (int32 SegmentIndex = HemisphereSegments - 1; SegmentIndex >= 1; --SegmentIndex)
		{
			const float Alpha = (static_cast<float>(SegmentIndex) / static_cast<float>(HemisphereSegments)) * (Pi * 0.5f);
			const float X = -CylinderHalfLength - sinf(Alpha) * CapsuleRadius;
			const float RingRadius = cosf(Alpha) * CapsuleRadius;
			Rings.emplace_back();
			TArray<FNormalVertex>& Ring = Rings.back();
			Ring.reserve(CircleSegments);
			for (int32 CircleIndex = 0; CircleIndex < CircleSegments; ++CircleIndex)
			{
				const float Theta = (2.0f * Pi * static_cast<float>(CircleIndex)) / static_cast<float>(CircleSegments);
				Ring.push_back(MakeVertex(X, RingRadius, Theta, -sinf(Alpha), cosf(Alpha)));
			}
		}

		for (float X : { -CylinderHalfLength, CylinderHalfLength })
		{
			Rings.emplace_back();
			TArray<FNormalVertex>& Ring = Rings.back();
			Ring.reserve(CircleSegments);
			for (int32 CircleIndex = 0; CircleIndex < CircleSegments; ++CircleIndex)
			{
				const float Theta = (2.0f * Pi * static_cast<float>(CircleIndex)) / static_cast<float>(CircleSegments);
				Ring.push_back(MakeVertex(X, CapsuleRadius, Theta, 0.0f, 1.0f));
			}
		}

		for (int32 SegmentIndex = 1; SegmentIndex <= HemisphereSegments - 1; ++SegmentIndex)
		{
			const float Alpha = (static_cast<float>(SegmentIndex) / static_cast<float>(HemisphereSegments)) * (Pi * 0.5f);
			const float X = CylinderHalfLength + sinf(Alpha) * CapsuleRadius;
			const float RingRadius = cosf(Alpha) * CapsuleRadius;
			Rings.emplace_back();
			TArray<FNormalVertex>& Ring = Rings.back();
			Ring.reserve(CircleSegments);
			for (int32 CircleIndex = 0; CircleIndex < CircleSegments; ++CircleIndex)
			{
				const float Theta = (2.0f * Pi * static_cast<float>(CircleIndex)) / static_cast<float>(CircleSegments);
				Ring.push_back(MakeVertex(X, RingRadius, Theta, sinf(Alpha), cosf(Alpha)));
			}
		}
		Rings.push_back({ MakeTipVertex(CylinderHalfLength + CapsuleRadius, 1.0f) });

		for (int32 RingIndex = 0; RingIndex + 1 < static_cast<int32>(Rings.size()); ++RingIndex)
		{
			const TArray<FNormalVertex>& RingA = Rings[RingIndex];
			const TArray<FNormalVertex>& RingB = Rings[RingIndex + 1];
			if (RingA.empty() || RingB.empty())
			{
				continue;
			}

			if (RingA.size() == 1 && RingB.size() > 1)
			{
				for (int32 CircleIndex = 0; CircleIndex < static_cast<int32>(RingB.size()); ++CircleIndex)
				{
					const int32 NextIndex = (CircleIndex + 1) % static_cast<int32>(RingB.size());
					AddOrientedTriangle(RingA[0], RingB[CircleIndex], RingB[NextIndex]);
				}
			}
			else if (RingA.size() > 1 && RingB.size() == 1)
			{
				for (int32 CircleIndex = 0; CircleIndex < static_cast<int32>(RingA.size()); ++CircleIndex)
				{
					const int32 NextIndex = (CircleIndex + 1) % static_cast<int32>(RingA.size());
					AddOrientedTriangle(RingA[CircleIndex], RingA[NextIndex], RingB[0]);
				}
			}
			else if (RingA.size() == RingB.size())
			{
				for (int32 CircleIndex = 0; CircleIndex < static_cast<int32>(RingA.size()); ++CircleIndex)
				{
					const int32 NextIndex = (CircleIndex + 1) % static_cast<int32>(RingA.size());
					AddOrientedTriangle(RingA[CircleIndex], RingB[CircleIndex], RingB[NextIndex]);
					AddOrientedTriangle(RingA[CircleIndex], RingB[NextIndex], RingA[NextIndex]);
				}
			}
		}

		if (CapsuleSection.NumTriangles > 0)
		{
			CapsuleMeshAsset->Sections.push_back(CapsuleSection);
		}

		if (CapsuleMeshAsset->Vertices.empty())
		{
			delete CapsuleMeshAsset;
			GUObjectArray.DestroyObject(CapsuleMesh);
			return nullptr;
		}

		CapsuleMeshAsset->CacheBounds();
		CapsuleMesh->SetAssetPathFileName("__PreviewCapsule__");
		CapsuleMesh->SetStaticMeshAsset(CapsuleMeshAsset);
		CapsuleMesh->InitResources(Device);
		return CapsuleMesh;
	}

	float ComputeSelectionAlpha(
		int32 BodyIndex,
		int32 SelectedBodyIndex,
		int32 SelectedConstraintIndex,
		const UPhysicsAsset* PhysicsAsset,
		float BaseAlpha,
		float ConstraintAlpha)
	{
		if (BodyIndex == SelectedBodyIndex)
		{
			return BaseAlpha;
		}

		if (SelectedConstraintIndex >= 0 && PhysicsAsset && SelectedConstraintIndex < static_cast<int32>(PhysicsAsset->GetConstraintSetups().size()))
		{
			const FPhysicsConstraintSetup& Constraint = PhysicsAsset->GetConstraintSetups()[SelectedConstraintIndex];
			const UPhysicsBodySetup* BodySetup = PhysicsAsset->GetBodySetups()[BodyIndex];
			if (BodySetup)
			{
				const FName& BoneName = BodySetup->GetTargetBoneName();
				if (BoneName == Constraint.ParentBoneName || BoneName == Constraint.ChildBoneName)
				{
					return ConstraintAlpha;
				}
			}
		}

		return BaseAlpha;
	}

	void RenderStatsOverlay(
		ImDrawList* DrawList,
		const ImVec2& ViewportPos,
		const UPhysicsAsset* PhysicsAsset,
		const USkeletalMesh* PreviewSkeletalMesh,
		const UWorld* PreviewWorld,
		bool bPreviewSimulating)
	{
		if (!DrawList || !PhysicsAsset)
		{
			return;
		}

		struct FPhysicsAssetOverlaySummary
		{
			int32 BodyCount = 0;
			int32 ConsideredForBoundsCount = 0;
			int32 SphereCount = 0;
			int32 BoxCount = 0;
			int32 CapsuleCount = 0;
			int32 ConvexCount = 0;
			int32 ConstraintCount = 0;
			int32 CrossConstraintCount = 0;
			int32 CollisionInteractionCount = 0;
		};

		auto FormatCountLabel = [](int32 Count, const char* Singular, const char* Plural) -> FString
		{
			return std::to_string(Count) + " " + (Count == 1 ? Singular : Plural);
		};

		auto IsCrossConstraint = [](const FSkeletonAsset* SkeletonAsset, const FPhysicsConstraintSetup& Constraint) -> bool
		{
			if (!SkeletonAsset)
			{
				return false;
			}

			const int32 ParentBoneIndex = FindSkeletonBoneIndexByName(SkeletonAsset, Constraint.ParentBoneName);
			const int32 ChildBoneIndex = FindSkeletonBoneIndexByName(SkeletonAsset, Constraint.ChildBoneName);
			if (ParentBoneIndex < 0 || ChildBoneIndex < 0)
			{
				return false;
			}

			const int32 BoneCount = static_cast<int32>(SkeletonAsset->Bones.size());
			if (ParentBoneIndex >= BoneCount || ChildBoneIndex >= BoneCount)
			{
				return false;
			}

			return SkeletonAsset->Bones[ChildBoneIndex].ParentIndex != ParentBoneIndex
				&& SkeletonAsset->Bones[ParentBoneIndex].ParentIndex != ChildBoneIndex;
		};

		auto BuildOverlaySummary = [&](const UPhysicsAsset* Asset, const FSkeletonAsset* SkeletonAsset) -> FPhysicsAssetOverlaySummary
		{
			FPhysicsAssetOverlaySummary Summary;
			if (!Asset)
			{
				return Summary;
			}

			const TArray<UPhysicsBodySetup*>& BodySetups = Asset->GetBodySetups();
			Summary.BodyCount = static_cast<int32>(BodySetups.size());

			for (const UPhysicsBodySetup* BodySetup : BodySetups)
			{
				if (!BodySetup)
				{
					continue;
				}

				if (BodySetup->bConsiderForBounds)
				{
					++Summary.ConsideredForBoundsCount;
				}

				const FPhysicsAggregateShapeSetup& ShapeSetup = BodySetup->GetShapeSetup();
				Summary.SphereCount += static_cast<int32>(ShapeSetup.SphereShapeSetups.size());
				Summary.BoxCount += static_cast<int32>(ShapeSetup.BoxShapeSetups.size());
				Summary.CapsuleCount += static_cast<int32>(ShapeSetup.CapsuleShapeSetups.size());
				Summary.ConvexCount += static_cast<int32>(ShapeSetup.ConvexShapeSetups.size());
			}

			std::unordered_set<uint64> CollisionInteractionPairs;
			const TArray<FPhysicsConstraintSetup>& Constraints = Asset->GetConstraintSetups();
			Summary.ConstraintCount = static_cast<int32>(Constraints.size());
			for (const FPhysicsConstraintSetup& Constraint : Constraints)
			{
				if (IsCrossConstraint(SkeletonAsset, Constraint))
				{
					++Summary.CrossConstraintCount;
				}

				if (Constraint.bDisableCollision)
				{
					continue;
				}

				const int32 ParentBodyIndex = FindBodySetupIndex(Asset, Constraint.ParentBoneName);
				const int32 ChildBodyIndex = FindBodySetupIndex(Asset, Constraint.ChildBoneName);
				if (ParentBodyIndex < 0 || ChildBodyIndex < 0 || ParentBodyIndex == ChildBodyIndex)
				{
					continue;
				}

				const uint32 MinIndex = static_cast<uint32>((std::min)(ParentBodyIndex, ChildBodyIndex));
				const uint32 MaxIndex = static_cast<uint32>((std::max)(ParentBodyIndex, ChildBodyIndex));
				const uint64 PairKey = (static_cast<uint64>(MinIndex) << 32) | static_cast<uint64>(MaxIndex);
				CollisionInteractionPairs.insert(PairKey);
			}

			Summary.CollisionInteractionCount = static_cast<int32>(CollisionInteractionPairs.size());
			return Summary;
		};

		auto BuildPrimitiveSummaryText = [&](const FPhysicsAssetOverlaySummary& Summary) -> FString
		{
			const int32 PrimitiveCount = Summary.CapsuleCount + Summary.SphereCount + Summary.BoxCount + Summary.ConvexCount;
			TArray<FString> PrimitiveBreakdown;
			if (Summary.CapsuleCount > 0)
			{
				PrimitiveBreakdown.push_back(FormatCountLabel(Summary.CapsuleCount, "Capsule", "Capsules"));
			}
			if (Summary.SphereCount > 0)
			{
				PrimitiveBreakdown.push_back(FormatCountLabel(Summary.SphereCount, "Sphere", "Spheres"));
			}
			if (Summary.BoxCount > 0)
			{
				PrimitiveBreakdown.push_back(FormatCountLabel(Summary.BoxCount, "Box", "Boxes"));
			}
			if (Summary.ConvexCount > 0)
			{
				PrimitiveBreakdown.push_back(FormatCountLabel(Summary.ConvexCount, "Convex", "Convexes"));
			}
			if (PrimitiveBreakdown.empty())
			{
				PrimitiveBreakdown.push_back("None");
			}

			FString BreakdownText;
			for (int32 Index = 0; Index < static_cast<int32>(PrimitiveBreakdown.size()); ++Index)
			{
				if (Index > 0)
				{
					BreakdownText += ", ";
				}
				BreakdownText += PrimitiveBreakdown[Index];
			}

			return FormatCountLabel(PrimitiveCount, "Primitive", "Primitives") + ": (" + BreakdownText + ")";
		};

		const FSkeletonAsset* SkeletonAsset = PreviewSkeletalMesh ? PreviewSkeletalMesh->GetSkeletonAsset() : nullptr;
		const FPhysicsAssetOverlaySummary Summary = BuildOverlaySummary(PhysicsAsset, SkeletonAsset);
		const int32 ConsideredPercent = Summary.BodyCount > 0
			? static_cast<int32>(std::round(static_cast<double>(Summary.ConsideredForBoundsCount) * 100.0 / static_cast<double>(Summary.BodyCount)))
			: 0;

		const bool bShowDetailedPhysicsStats = IsEditorPhysicsStatVisible();
		FString Text =
			FormatCountLabel(Summary.BodyCount, "Body", "Bodies")
			+ " (" + std::to_string(Summary.ConsideredForBoundsCount)
			+ " Considered For Bounds, " + std::to_string(ConsideredPercent) + "%)\n"
			+ BuildPrimitiveSummaryText(Summary) + "\n"
			+ FormatCountLabel(Summary.ConstraintCount, "Constraint", "Constraints")
			+ " (" + FormatCountLabel(Summary.CrossConstraintCount, "Cross Constraint", "Cross Constraints") + ")\n"
			+ std::to_string(Summary.CollisionInteractionCount) + " Collision Interactions";

		Text += "\nBody Grouping: ";
		Text += PhysicsAssetRagdollModeToString(PhysicsAsset->GetRagdollMode());

		const IPhysicsSceneInterface* PhysicsScene = PreviewWorld ? PreviewWorld->GetPhysicsScene() : nullptr;
		if (PhysicsScene && bPreviewSimulating)
		{
			const FPhysicsRuntimeStats& Stats = PhysicsScene->GetStats();
			Text += "\n\nScene Bodies: " + std::to_string(Stats.BodyCount);
			Text += "\nActive Ragdolls: " + std::to_string(Stats.ActiveRagdollCount)
				+ " (" + std::to_string(Stats.ActiveAggregateRagdollCount)
				+ " agg / " + std::to_string(Stats.ActivePerBodyRagdollCount) + " per-body)";
			Text += "\nRagdoll Bodies: " + std::to_string(Stats.RagdollBodyCount)
				+ "  Constraints: " + std::to_string(Stats.RagdollConstraintCount);
			Text += "\nAggregates: " + std::to_string(Stats.AggregateCount)
				+ "  Actors: " + std::to_string(Stats.AggregateActorCount);
			if (bShowDetailedPhysicsStats)
			{
				char TimingBuffer[96] = {};
				std::snprintf(
					TimingBuffer,
					sizeof(TimingBuffer),
					"Step/Sync: %.3f / %.3f ms",
					Stats.StepTimeMs,
					Stats.SyncTimeMs);
				Text += "\n";
				Text += TimingBuffer;
			}
		}
		else if (bShowDetailedPhysicsStats)
		{
			Text += "\n\nRuntime Stats: press Simulate to populate";
		}

		const ImVec2 TextPos(ViewportPos.x + 8.0f, ViewportPos.y + 36.0f);
		DrawList->AddText(ImVec2(TextPos.x + 1.0f, TextPos.y + 1.0f), IM_COL32(0, 0, 0, 220), Text.c_str());
		DrawList->AddText(TextPos, IM_COL32(235, 238, 242, 255), Text.c_str());
	}
}

FPhysicsAssetEditorWidget::FPhysicsAssetEditorWidget()
	: InstanceId(GNextPhysicsAssetEditorInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("PhysicsAssetPreview_" + Id);
	WindowIdSuffix = "###PhysicsAssetEditor_" + Id;
}

bool FPhysicsAssetEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UPhysicsAsset>();
}

void FPhysicsAssetEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);

	SelectedBoneIndex = -1;
	SelectedBodyIndex = -1;
	SelectedConstraintIndex = -1;
	SelectedShapeType = EPhysicsAssetPreviewShapeType::None;
	SelectedShapeIndex = -1;
	BoneSearchBuffer[0] = '\0';
	bShowBonesWithBodiesOnly = false;
	bExpandBodyTreeOnNextRender = true;
	bFrameGraphOnNextRender = true;
	bPreviewSimulating = false;
	PreviewSimulationTime = 0.0f;
	ToolSettings = FPhysicsAssetCreateParams();
	PreviewSkeletalMesh = nullptr;
	if (!BodyGizmoTarget)
	{
		BodyGizmoTarget = new FPhysicsBodyGizmoTarget();
	}
	if (!ConstraintGizmoTarget)
	{
		ConstraintGizmoTarget = new FPhysicsConstraintGizmoTarget();
	}

	if (!CapsuleBodyIcon)
	{
		const FString IconPath = FPaths::ToUtf8(FPaths::Combine(FPaths::AssetDir(), L"Editor/Icons/Capsule_16px.png"));
		CapsuleBodyIcon = FEditorTextureManager::Get().GetOrLoadIcon(IconPath);
	}

	InitializePreviewScene(static_cast<UPhysicsAsset*>(EditedObject));

	UPhysicsAsset* PhysicsAsset = static_cast<UPhysicsAsset*>(EditedObject);
	if (PhysicsAsset)
	{
		ToolSettings.RagdollMode = PhysicsAsset->GetRagdollMode();
	}
	if (PhysicsAsset && !PhysicsAsset->GetBodySetups().empty())
	{
		if (const UPhysicsBodySetup* FirstBodySetup = PhysicsAsset->GetBodySetups()[0])
		{
			LoadBatchBodyPropertyValues(FirstBodySetup);
		}
		SelectBodyByIndex(0);
	}
}

void FPhysicsAssetEditorWidget::InitializePreviewScene(UPhysicsAsset* PhysicsAsset)
{
	if (!PhysicsAsset || !GEngine)
	{
		return;
	}

	const FString PreviewMeshPath = PhysicsAsset->GetPreviewSkeletalMeshPath();
	if (PreviewMeshPath.empty())
	{
		return;
	}

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	PreviewSkeletalMesh = FMeshManager::FindSkeletalMesh(PreviewMeshPath);
	if (!PreviewSkeletalMesh)
	{
		PreviewSkeletalMesh = FMeshManager::LoadSkeletalMesh(PreviewMeshPath, Device);
	}

	if (!PreviewSkeletalMesh)
	{
		return;
	}

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	AActor* Actor = WorldContext.World->SpawnActor<AActor>();
	Actor->bTickInEditor = true;
	USkeletalMeshComponent* MeshComponent = Actor->AddComponent<USkeletalMeshComponent>();
	MeshComponent->SetRagdollSelfCollisionEnabled(bPreviewEnableSelfCollision);
	MeshComponent->SetSkeletalMesh(PreviewSkeletalMesh);
	Actor->SetRootComponent(MeshComponent);
	Actor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
	PreviewShapeActor = WorldContext.World->SpawnActor<AActor>();

	PreviewCubeMesh = FMeshManager::FindStaticMesh("Asset/Mesh/BasicShape/Cube_StaticMesh.uasset");
	if (!PreviewCubeMesh)
	{
		PreviewCubeMesh = FMeshManager::LoadStaticMesh("Asset/Mesh/BasicShape/Cube_StaticMesh.uasset", Device);
	}
	PreviewSphereMesh = FMeshManager::FindStaticMesh("Asset/Mesh/BasicShape/Sphere_StaticMesh.uasset");
	if (!PreviewSphereMesh)
	{
		PreviewSphereMesh = FMeshManager::LoadStaticMesh("Asset/Mesh/BasicShape/Sphere_StaticMesh.uasset", Device);
	}
	if (PreviewHemisphereMesh)
	{
		GUObjectArray.DestroyObject(PreviewHemisphereMesh);
		PreviewHemisphereMesh = nullptr;
	}
	PreviewHemisphereMesh = CreatePositiveXHemispherePreviewMesh(PreviewSphereMesh, Device);
	if (PreviewOpenCylinderMesh)
	{
		GUObjectArray.DestroyObject(PreviewOpenCylinderMesh);
		PreviewOpenCylinderMesh = nullptr;
	}
	if (PreviewCapsuleMesh)
	{
		GUObjectArray.DestroyObject(PreviewCapsuleMesh);
		PreviewCapsuleMesh = nullptr;
	}
	PreviewCylinderMesh = FMeshManager::FindStaticMesh("Asset/Mesh/BasicShape/Cylinder_StaticMesh.uasset");
	if (!PreviewCylinderMesh)
	{
		PreviewCylinderMesh = FMeshManager::LoadStaticMesh("Asset/Mesh/BasicShape/Cylinder_StaticMesh.uasset", Device);
	}
	PreviewOpenCylinderMesh = CreateOpenCylinderPreviewMesh(PreviewCylinderMesh, Device);
	PreviewCapsuleMesh = CreateCanonicalCapsulePreviewMesh(PreviewCylinderMesh ? PreviewCylinderMesh : PreviewSphereMesh, Device);

	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));
	if (UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>())
	{
		LightComp->SetShadowBias(0.0f);
		LightComp->PushToScene();
	}

	AStaticMeshActor* FloorActor = WorldContext.World->SpawnActor<AStaticMeshActor>();
	FloorActor->InitDefaultComponents("Asset/Mesh/CubeGrid/SM_CubeGrid_StaticMesh.uasset");
	UBoxComponent* FloorBoxComponent = FloorActor->AddComponent<UBoxComponent>();
	FloorBoxComponent->AttachToComponent(FloorActor->GetRootComponent());
	FloorBoxComponent->SetBoxExtent(FVector(1.0f, 1.0f, 1.0f));
	FloorBoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	FloorBoxComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
	FloorBoxComponent->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
	FloorActor->SetActorLocation(FVector(0.0f, 0.0f, -1.0f));
	FloorActor->SetActorScale(FVector(10.0f, 10.0f, 1.0f));

	ViewportClient.Initialize(Device, 320, 240);
	ViewportClient.GetRenderOptions().ShowFlags.bGrid = false;
	ViewportClient.GetRenderOptions().ShowFlags.bDebugDraw = true;
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(Actor);
	ViewportClient.SetPreviewMeshComponent(MeshComponent);
	ViewportClient.CreatePreviewGizmo();
	WorldContext.World->SetEditorPOVProvider(&ViewportClient);
	ViewportClient.SetSelectedBone(PreviewSkeletalMesh, -1);

	FSlateApplication::Get().RegisterViewport(&PhysicsAssetViewportWindow, &ViewportClient);
	ViewportClient.ResetCameraToPreviewBounds();
	RebuildPreviewShapeComponents(PhysicsAsset);
	SetEditorPreviewOverlaysVisible(true);
}

// ---------------------------------------------------------------------------
// Constraint angular limit fan helpers
// ---------------------------------------------------------------------------

namespace
{
	FNormalVertex MakePreviewVertex(const FVector& Position, const FVector& Normal)
	{
		FNormalVertex Vertex{};
		Vertex.pos = Position;
		Vertex.normal = Normal.IsNearlyZero() ? FVector::ForwardVector : Normal;
		Vertex.color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

		FVector Tangent = FVector::UpVector.Cross(Vertex.normal);
		if (Tangent.IsNearlyZero())
		{
			Tangent = FVector::RightVector;
		}
		else
		{
			Tangent.Normalize();
		}
		Vertex.tangent = FVector4(Tangent, 1.0f);
		return Vertex;
	}

	void InitializePreviewConstraintMesh(UStaticMesh* Mesh, UStaticMesh* MaterialSourceMesh)
	{
		if (!Mesh || !MaterialSourceMesh)
		{
			return;
		}

		TArray<FStaticMaterial> Materials = MaterialSourceMesh->GetStaticMaterials();
		Mesh->SetStaticMaterials(std::move(Materials));
	}

	FStaticMeshSection MakePreviewConstraintSection(UStaticMesh* Mesh)
	{
		FStaticMeshSection Section;
		Section.MaterialSlotName = Mesh && !Mesh->GetStaticMaterials().empty()
			? Mesh->GetStaticMaterials()[0].MaterialSlotName
			: FString("None");
		Section.FirstIndex = 0;
		Section.NumTriangles = 0;
		return Section;
	}

	UStaticMesh* CreateConstraintSwingConeMesh(
		UStaticMesh* MaterialSourceMesh,
		float Swing1HalfAngleDeg,
		float Swing2HalfAngleDeg,
		ID3D11Device* Device)
	{
		if (!Device)
		{
			return nullptr;
		}

		const float ClampedSwing1Deg = (std::max)(0.0f, (std::min)(Swing1HalfAngleDeg, 89.0f));
		const float ClampedSwing2Deg = (std::max)(0.0f, (std::min)(Swing2HalfAngleDeg, 89.0f));
		if (ClampedSwing1Deg <= 0.0f && ClampedSwing2Deg <= 0.0f)
		{
			return nullptr;
		}

		const float BaseRadiusY = sinf(ClampedSwing2Deg * Pi / 180.0f);
		const float BaseRadiusZ = sinf(ClampedSwing1Deg * Pi / 180.0f);
		const float RadiusY = (std::max)(BaseRadiusY, 0.001f);
		const float RadiusZ = (std::max)(BaseRadiusZ, 0.001f);
		constexpr int32 Segments = 32;

		UStaticMesh* ConeMesh = GUObjectArray.CreateObject<UStaticMesh>();
		if (!ConeMesh)
		{
			return nullptr;
		}

		const FString ConePath = MakePhysicsAssetPreviewResourcePath("SwingCone");
		FStaticMesh* ConeAsset = new FStaticMesh();
		ConeAsset->PathFileName = ConePath;
		InitializePreviewConstraintMesh(ConeMesh, MaterialSourceMesh);

		FStaticMeshSection Section = MakePreviewConstraintSection(ConeMesh);
		const FNormalVertex Apex = MakePreviewVertex(FVector::ZeroVector, FVector(-1.0f, 0.0f, 0.0f));

		for (int32 SegmentIndex = 0; SegmentIndex < Segments; ++SegmentIndex)
		{
			const float Theta0 = (2.0f * Pi * static_cast<float>(SegmentIndex)) / static_cast<float>(Segments);
			const float Theta1 = (2.0f * Pi * static_cast<float>(SegmentIndex + 1)) / static_cast<float>(Segments);

			const FVector Position0(1.0f, cosf(Theta0) * RadiusY, sinf(Theta0) * RadiusZ);
			const FVector Position1(1.0f, cosf(Theta1) * RadiusY, sinf(Theta1) * RadiusZ);
			FVector TriangleNormal = (Position0 - Apex.pos).Cross(Position1 - Apex.pos);
			if (TriangleNormal.IsNearlyZero())
			{
				continue;
			}
			TriangleNormal.Normalize();

			const FNormalVertex Vertex0 = MakePreviewVertex(Position0, TriangleNormal);
			const FNormalVertex Vertex1 = MakePreviewVertex(Position1, TriangleNormal);
			AppendPreviewTriangle(ConeAsset, Section, Apex, Vertex0, Vertex1);
			AppendPreviewTriangle(ConeAsset, Section, Apex, Vertex1, Vertex0);
		}

		if (Section.NumTriangles == 0)
		{
			delete ConeAsset;
			GUObjectArray.DestroyObject(ConeMesh);
			return nullptr;
		}

		ConeAsset->Sections.push_back(Section);
		ConeAsset->CacheBounds();
		ConeMesh->SetAssetPathFileName(ConePath);
		ConeMesh->SetStaticMeshAsset(ConeAsset);
		ConeMesh->InitResources(Device);
		return ConeMesh;
	}

	UStaticMesh* CreateConstraintTwistFanMesh(UStaticMesh* MaterialSourceMesh, float HalfAngleDeg, ID3D11Device* Device)
	{
		if (!Device)
		{
			return nullptr;
		}

		const float ClampedHalfAngleDeg = (std::max)(0.0f, (std::min)(HalfAngleDeg, 179.0f));
		if (ClampedHalfAngleDeg <= 0.0f)
		{
			return nullptr;
		}

		const float HalfAngleRad = ClampedHalfAngleDeg * Pi / 180.0f;
		constexpr int32 Segments = 32;

		UStaticMesh* FanMesh = GUObjectArray.CreateObject<UStaticMesh>();
		if (!FanMesh)
		{
			return nullptr;
		}

		const FString FanPath = MakePhysicsAssetPreviewResourcePath("TwistFan");
		FStaticMesh* FanAsset = new FStaticMesh();
		FanAsset->PathFileName = FanPath;
		InitializePreviewConstraintMesh(FanMesh, MaterialSourceMesh);

		FStaticMeshSection Section = MakePreviewConstraintSection(FanMesh);
		const FNormalVertex Center = MakePreviewVertex(FVector::ZeroVector, FVector::ForwardVector);

		for (int32 SegmentIndex = 0; SegmentIndex < Segments; ++SegmentIndex)
		{
			const float T0 = -HalfAngleRad + (2.0f * HalfAngleRad * static_cast<float>(SegmentIndex) / static_cast<float>(Segments));
			const float T1 = -HalfAngleRad + (2.0f * HalfAngleRad * static_cast<float>(SegmentIndex + 1) / static_cast<float>(Segments));

			const FNormalVertex Vertex0 = MakePreviewVertex(FVector(0.0f, cosf(T0), sinf(T0)), FVector::ForwardVector);
			const FNormalVertex Vertex1 = MakePreviewVertex(FVector(0.0f, cosf(T1), sinf(T1)), FVector::ForwardVector);
			AppendPreviewTriangle(FanAsset, Section, Center, Vertex0, Vertex1);
			AppendPreviewTriangle(FanAsset, Section, Center, Vertex1, Vertex0);
		}

		if (Section.NumTriangles == 0)
		{
			delete FanAsset;
			GUObjectArray.DestroyObject(FanMesh);
			return nullptr;
		}

		FanAsset->Sections.push_back(Section);
		FanAsset->CacheBounds();
		FanMesh->SetAssetPathFileName(FanPath);
		FanMesh->SetStaticMeshAsset(FanAsset);
		FanMesh->InitResources(Device);
		return FanMesh;
	}

	// Arrow mesh pointing along local +X: thin cylinder shaft + cone head.
	// Used for constraint local frame axis visualization (scale controls length).
	UStaticMesh* CreateConstraintAxisArrowMesh(UStaticMesh* MaterialSourceMesh, ID3D11Device* Device)
	{
		if (!Device) return nullptr;

		constexpr int32 Segments   = 8;
		constexpr float ShaftEnd   = 0.65f;
		constexpr float ShaftR     = 0.05f;
		constexpr float HeadR      = 0.14f;

		UStaticMesh* ArrowMesh = GUObjectArray.CreateObject<UStaticMesh>();
		if (!ArrowMesh) return nullptr;

		const FString ArrowPath = MakePhysicsAssetPreviewResourcePath("ConstraintAxisArrow");
		FStaticMesh* ArrowAsset = new FStaticMesh();
		ArrowAsset->PathFileName = ArrowPath;
		InitializePreviewConstraintMesh(ArrowMesh, MaterialSourceMesh);

		FStaticMeshSection Section = MakePreviewConstraintSection(ArrowMesh);

		// Shaft cylinder (side walls, front + back faces for translucency)
		for (int32 Seg = 0; Seg < Segments; ++Seg)
		{
			const float T0 = 2.0f * Pi * Seg / Segments;
			const float T1 = 2.0f * Pi * (Seg + 1) / Segments;
			const FVector P00(0.0f,    cosf(T0) * ShaftR, sinf(T0) * ShaftR);
			const FVector P01(0.0f,    cosf(T1) * ShaftR, sinf(T1) * ShaftR);
			const FVector P10(ShaftEnd, cosf(T0) * ShaftR, sinf(T0) * ShaftR);
			const FVector P11(ShaftEnd, cosf(T1) * ShaftR, sinf(T1) * ShaftR);
			const FVector N0(0.0f, cosf(T0), sinf(T0));
			const FVector N1(0.0f, cosf(T1), sinf(T1));
			AppendPreviewTriangle(ArrowAsset, Section, MakePreviewVertex(P00, N0),          MakePreviewVertex(P10, N0),          MakePreviewVertex(P11, N1));
			AppendPreviewTriangle(ArrowAsset, Section, MakePreviewVertex(P00, N0),          MakePreviewVertex(P11, N1),          MakePreviewVertex(P01, N1));
			AppendPreviewTriangle(ArrowAsset, Section, MakePreviewVertex(P00, N0 * -1.0f),  MakePreviewVertex(P11, N1 * -1.0f), MakePreviewVertex(P10, N0 * -1.0f));
			AppendPreviewTriangle(ArrowAsset, Section, MakePreviewVertex(P00, N0 * -1.0f),  MakePreviewVertex(P01, N1 * -1.0f), MakePreviewVertex(P11, N1 * -1.0f));
		}

		// Cone head
		const FNormalVertex Tip = MakePreviewVertex(FVector(1.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f));
		for (int32 Seg = 0; Seg < Segments; ++Seg)
		{
			const float T0 = 2.0f * Pi * Seg / Segments;
			const float T1 = 2.0f * Pi * (Seg + 1) / Segments;
			const FVector B0(ShaftEnd, cosf(T0) * HeadR, sinf(T0) * HeadR);
			const FVector B1(ShaftEnd, cosf(T1) * HeadR, sinf(T1) * HeadR);
			FVector SN = (B0 - FVector(1.0f, 0.0f, 0.0f)).Cross(B1 - B0);
			if (!SN.IsNearlyZero()) SN.Normalize(); else SN = FVector(1.0f, 0.0f, 0.0f);
			AppendPreviewTriangle(ArrowAsset, Section, Tip, MakePreviewVertex(B0, SN), MakePreviewVertex(B1, SN));
			AppendPreviewTriangle(ArrowAsset, Section, Tip, MakePreviewVertex(B1, SN), MakePreviewVertex(B0, SN));
		}

		// Cone base cap
		const FNormalVertex BaseCtr = MakePreviewVertex(FVector(ShaftEnd, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f));
		for (int32 Seg = 0; Seg < Segments; ++Seg)
		{
			const float T0 = 2.0f * Pi * Seg / Segments;
			const float T1 = 2.0f * Pi * (Seg + 1) / Segments;
			const FVector B0(ShaftEnd, cosf(T0) * HeadR, sinf(T0) * HeadR);
			const FVector B1(ShaftEnd, cosf(T1) * HeadR, sinf(T1) * HeadR);
			const FVector BN(-1.0f, 0.0f, 0.0f);
			AppendPreviewTriangle(ArrowAsset, Section, BaseCtr, MakePreviewVertex(B1, BN), MakePreviewVertex(B0, BN));
			AppendPreviewTriangle(ArrowAsset, Section, BaseCtr, MakePreviewVertex(B0, BN), MakePreviewVertex(B1, BN));
		}

		if (Section.NumTriangles == 0)
		{
			delete ArrowAsset;
			GUObjectArray.DestroyObject(ArrowMesh);
			return nullptr;
		}

		ArrowAsset->Sections.push_back(Section);
		ArrowAsset->CacheBounds();
		ArrowMesh->SetAssetPathFileName(ArrowPath);
		ArrowMesh->SetStaticMeshAsset(ArrowAsset);
		ArrowMesh->InitResources(Device);
		return ArrowMesh;
	}
}

void FPhysicsAssetEditorWidget::ClearConstraintConeComponents()
{
	for (FPreviewConstraintConeEntry& Entry : PreviewConstraintCones)
	{
		if (PreviewShapeActor && Entry.Component)
		{
			PreviewShapeActor->RemoveComponent(Entry.Component);
			Entry.Component = nullptr;
		}
		if (Entry.Material)
		{
			GUObjectArray.DestroyObject(Entry.Material);
			Entry.Material = nullptr;
		}
		if (Entry.Mesh)
		{
			GUObjectArray.DestroyObject(Entry.Mesh);
			Entry.Mesh = nullptr;
		}
	}
	PreviewConstraintCones.clear();
}

bool FPhysicsAssetEditorWidget::NeedsConstraintConeRebuild(UPhysicsAsset* PhysicsAsset) const
{
	// Do not rebuild GPU meshes just because a numeric twist/swing limit changed.
	// DragFloat can produce a new value every frame; rebuilding every constraint mesh here
	// causes visible stutter and tanks the PhysicsAsset editor FPS.
	// The live debug overlay is redrawn every frame from the current values, while this
	// persistent mesh list is kept only for visibility/picking and topology changes.
	if (!PhysicsAsset)
	{
		return !PreviewConstraintCones.empty();
	}

	int32 ExpectedVisualCount = 0;
	const TArray<FPhysicsConstraintSetup>& Constraints = PhysicsAsset->GetConstraintSetups();
	for (const FPhysicsConstraintSetup& Constraint : Constraints)
	{
		const float Swing1Limit = GetSwingVisualHalfAngle(Constraint.Swing1Motion, Constraint.SwingLimitY);
		const float Swing2Limit = GetSwingVisualHalfAngle(Constraint.Swing2Motion, Constraint.SwingLimitZ);
		if (ShouldRenderSwingCone(Swing1Limit, Swing2Limit))
		{
			++ExpectedVisualCount;
		}

		const float TwistLimit = GetTwistVisualHalfAngle(Constraint);
		if (TwistLimit > 0.0f)
		{
			++ExpectedVisualCount;
		}

		ExpectedVisualCount += 3; // AxisX, AxisY, AxisZ arrows
	}

	if (ExpectedVisualCount != static_cast<int32>(PreviewConstraintCones.size()))
	{
		return true;
	}

	int32 EntryIndex = 0;
	for (int32 ConstraintIndex = 0; ConstraintIndex < static_cast<int32>(Constraints.size()); ++ConstraintIndex)
	{
		const FPhysicsConstraintSetup& Constraint = Constraints[ConstraintIndex];
		const float Swing1Limit = GetSwingVisualHalfAngle(Constraint.Swing1Motion, Constraint.SwingLimitY);
		const float Swing2Limit = GetSwingVisualHalfAngle(Constraint.Swing2Motion, Constraint.SwingLimitZ);
		if (ShouldRenderSwingCone(Swing1Limit, Swing2Limit))
		{
			if (EntryIndex >= static_cast<int32>(PreviewConstraintCones.size()))
			{
				return true;
			}

			const FPreviewConstraintConeEntry& Entry = PreviewConstraintCones[EntryIndex++];
			if (Entry.ConstraintIndex != ConstraintIndex
				|| Entry.VisualType != EPhysicsAssetConstraintVisualType::SwingCone)
			{
				return true;
			}
		}

		const float TwistLimit = GetTwistVisualHalfAngle(Constraint);
		if (TwistLimit > 0.0f)
		{
			if (EntryIndex >= static_cast<int32>(PreviewConstraintCones.size()))
			{
				return true;
			}

			const FPreviewConstraintConeEntry& Entry = PreviewConstraintCones[EntryIndex++];
			if (Entry.ConstraintIndex != ConstraintIndex
				|| Entry.VisualType != EPhysicsAssetConstraintVisualType::TwistFan)
			{
				return true;
			}
		}

		// Axis frame arrows (always 3 per constraint)
		for (EPhysicsAssetConstraintVisualType AxisType : {
				EPhysicsAssetConstraintVisualType::AxisX,
				EPhysicsAssetConstraintVisualType::AxisY,
				EPhysicsAssetConstraintVisualType::AxisZ })
		{
			if (EntryIndex >= static_cast<int32>(PreviewConstraintCones.size())) return true;
			const FPreviewConstraintConeEntry& AE = PreviewConstraintCones[EntryIndex++];
			if (AE.ConstraintIndex != ConstraintIndex || AE.VisualType != AxisType) return true;
		}
	}

	return false;
}

void FPhysicsAssetEditorWidget::RebuildConstraintConeComponents(UPhysicsAsset* PhysicsAsset)
{
	ClearConstraintConeComponents();
	if (!PhysicsAsset || !PreviewShapeActor || !GEngine) return;

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	const TArray<FPhysicsConstraintSetup>& Constraints = PhysicsAsset->GetConstraintSetups();

	auto AddVisual = [&](int32 ConstraintIndex, UStaticMesh* Mesh, EPhysicsAssetConstraintVisualType VisualType, const FVector4& Color) -> FPreviewConstraintConeEntry*
	{
		if (!Mesh) return nullptr;

		UStaticMeshComponent* Comp = PreviewShapeActor->AddComponent<UStaticMeshComponent>();
		if (!Comp) { GUObjectArray.DestroyObject(Mesh); return nullptr; }
		Comp->SetStaticMesh(Mesh);
		Comp->SetCastShadow(false);

		UMaterial* Mat = CreatePreviewShapeMaterial(PreviewConstraintShapeOpacity, true);
		if (Mat) Mat->SetVector4Parameter("SectionColor", Color);
		if (Mat) Comp->SetMaterial(0, Mat);

		FPreviewConstraintConeEntry Entry;
		Entry.Component = Comp;
		Entry.Material = Mat;
		Entry.Mesh = Mesh;
		Entry.LastColor = Color;
		Entry.ConstraintIndex = ConstraintIndex;
		Entry.VisualType = VisualType;
		Entry.EffectivePrimaryLimit = 0.0f;
		Entry.EffectiveSecondaryLimit = 0.0f;
		PreviewConstraintCones.push_back(Entry);
		return &PreviewConstraintCones.back();
	};

	for (int32 Ci = 0; Ci < static_cast<int32>(Constraints.size()); ++Ci)
	{
		const FPhysicsConstraintSetup& CS = Constraints[Ci];
		const float Swing1Limit = GetSwingVisualHalfAngle(CS.Swing1Motion, CS.SwingLimitY);
		const float Swing2Limit = GetSwingVisualHalfAngle(CS.Swing2Motion, CS.SwingLimitZ);

		// Swing limits are visualized as a single red cone around the twist axis.
		if (ShouldRenderSwingCone(Swing1Limit, Swing2Limit))
		{
			if (FPreviewConstraintConeEntry* Entry = AddVisual(
				Ci,
				CreateConstraintSwingConeMesh(PreviewSphereMesh, Swing1Limit, Swing2Limit, Device),
				EPhysicsAssetConstraintVisualType::SwingCone,
				FVector4(1.0f, 0.2f, 0.2f, PreviewConstraintShapeOpacity)))
			{
				Entry->EffectivePrimaryLimit = Swing1Limit;
				Entry->EffectiveSecondaryLimit = Swing2Limit;
			}
		}

		// Twist is visualized as a single green fan in the YZ plane.
		const float TwistLimit = GetTwistVisualHalfAngle(CS);
		if (TwistLimit > 0.0f)
		{
			if (FPreviewConstraintConeEntry* Entry = AddVisual(
				Ci,
				CreateConstraintTwistFanMesh(PreviewSphereMesh, TwistLimit, Device),
				EPhysicsAssetConstraintVisualType::TwistFan,
				FVector4(0.2f, 1.0f, 0.2f, PreviewConstraintShapeOpacity)))
			{
				Entry->EffectivePrimaryLimit = TwistLimit;
				Entry->EffectiveSecondaryLimit = 0.0f;
			}
		}

		// Local frame axis arrows: X=red (Twist), Y=green (Swing1), Z=blue (Swing2)
		AddVisual(Ci, CreateConstraintAxisArrowMesh(PreviewSphereMesh, Device),
			EPhysicsAssetConstraintVisualType::AxisX, FVector4(0.9f, 0.15f, 0.15f, PreviewConstraintShapeOpacity));
		AddVisual(Ci, CreateConstraintAxisArrowMesh(PreviewSphereMesh, Device),
			EPhysicsAssetConstraintVisualType::AxisY, FVector4(0.15f, 0.9f, 0.15f, PreviewConstraintShapeOpacity));
		AddVisual(Ci, CreateConstraintAxisArrowMesh(PreviewSphereMesh, Device),
			EPhysicsAssetConstraintVisualType::AxisZ, FVector4(0.15f, 0.35f, 0.9f, PreviewConstraintShapeOpacity));
	}
}

void FPhysicsAssetEditorWidget::SyncConstraintConeComponents(UPhysicsAsset* PhysicsAsset)
{
	if (PreviewConstraintCones.empty() || !PhysicsAsset) return;

	const FSkeletonAsset* Skel = PreviewSkeletalMesh ? PreviewSkeletalMesh->GetSkeletonAsset() : nullptr;
	USkeletalMeshComponent* MC = ViewportClient.GetPreviewMeshComponent();
	if (!Skel || !MC) return;

	TArray<FMatrix> BoneGlobals;
	MC->GetCurrentBoneGlobalMatrices(BoneGlobals);
	const TArray<FPhysicsConstraintSetup>& Constraints = PhysicsAsset->GetConstraintSetups();

	for (FPreviewConstraintConeEntry& Entry : PreviewConstraintCones)
	{
		if (!Entry.Component || Entry.ConstraintIndex >= static_cast<int32>(Constraints.size())) continue;

		const FPhysicsConstraintSetup& CS = Constraints[Entry.ConstraintIndex];
		const int32 PBone = FindBoneIndexByName(Skel, CS.ParentBoneName);
		const int32 CBone = FindBoneIndexByName(Skel, CS.ChildBoneName);
		if (PBone < 0 || CBone < 0) { Entry.Component->SetVisibility(false); continue; }

		const FTransform PFrame(CS.ParentLocalFrame.ToMatrix() * BuildBoneWorldMatrix(MC, &BoneGlobals, PBone));
		const FTransform CFrame(CS.ChildLocalFrame.ToMatrix()  * BuildBoneWorldMatrix(MC, &BoneGlobals, CBone));
		const FVector Origin = (PFrame.Location + CFrame.Location) * 0.5f;

		const bool bSelected = Entry.ConstraintIndex == SelectedConstraintIndex;
		const float VisualLength = bSelected ? ConstraintAxisLength * 2.0f : ConstraintAxisLength;
		const float Alpha = PreviewConstraintShapeOpacity;

		const bool bLimitVisual = Entry.VisualType == EPhysicsAssetConstraintVisualType::SwingCone
			|| Entry.VisualType == EPhysicsAssetConstraintVisualType::TwistFan;

		// Do not recreate the selected swing/twist GPU mesh while the user drags
		// numeric angular limits. Recreating UStaticMesh + InitResources per mouse
		// delta is the main FPS spike. The current selected limit is visualized by
		// DrawConstraintDebug() as a live 2x always-on-top overlay.

		const bool bShowPersistentComponent = bShowConstraintDebug && !(bSelected && bLimitVisual);
		if (Entry.Component->IsVisible() != bShowPersistentComponent)
			Entry.Component->SetVisibility(bShowPersistentComponent);

		constexpr float PosTolerance   = 1e-4f;
		constexpr float ScaleTolerance = 1e-5f;
		constexpr float RotTolerance   = 1e-5f;

		auto SetLocIfChanged = [&](const FVector& NewLoc)
		{
			const FVector Cur = Entry.Component->GetRelativeLocation();
			if (std::fabs(Cur.X - NewLoc.X) > PosTolerance ||
				std::fabs(Cur.Y - NewLoc.Y) > PosTolerance ||
				std::fabs(Cur.Z - NewLoc.Z) > PosTolerance)
				Entry.Component->SetRelativeLocation(NewLoc);
		};
		auto SetRotIfChanged = [&](const FQuat& NewRot)
		{
			if (!Entry.Component->GetRelativeQuat().Equals(NewRot, RotTolerance))
				Entry.Component->SetRelativeRotation(NewRot);
		};
		auto SetScaleIfChanged = [&](const FVector& NewScale)
		{
			const FVector Cur = Entry.Component->GetRelativeScale();
			if (std::fabs(Cur.X - NewScale.X) > ScaleTolerance ||
				std::fabs(Cur.Y - NewScale.Y) > ScaleTolerance ||
				std::fabs(Cur.Z - NewScale.Z) > ScaleTolerance)
				Entry.Component->SetRelativeScale(NewScale);
		};
		auto SetMaterialColorIfChanged = [&](const FVector4& NewColor)
		{
			constexpr float ColorTolerance = 1e-4f;
			if (!Entry.Material)
			{
				return;
			}
			if (std::fabs(Entry.LastColor.X - NewColor.X) > ColorTolerance ||
				std::fabs(Entry.LastColor.Y - NewColor.Y) > ColorTolerance ||
				std::fabs(Entry.LastColor.Z - NewColor.Z) > ColorTolerance ||
				std::fabs(Entry.LastColor.W - NewColor.W) > ColorTolerance)
			{
				Entry.Material->SetVector4Parameter("SectionColor", NewColor);
				Entry.LastColor = NewColor;
			}
		};

		SetLocIfChanged(Origin);

		if (Entry.VisualType == EPhysicsAssetConstraintVisualType::SwingCone)
		{
			SetRotIfChanged(PFrame.Rotation);
			const float SwingLength = VisualLength * ConstraintSwingVisualScale;
			SetScaleIfChanged(FVector(SwingLength, SwingLength, SwingLength));
			SetMaterialColorIfChanged(FVector4(bSelected ? 1.0f : 0.9f, 0.f, 0.f, Alpha));
		}
		else if (Entry.VisualType == EPhysicsAssetConstraintVisualType::TwistFan)
		{
			SetRotIfChanged(PFrame.Rotation);
			const float TwistLength = VisualLength * ConstraintTwistVisualScale;
			SetScaleIfChanged(FVector(TwistLength, TwistLength, TwistLength));
			SetMaterialColorIfChanged(FVector4(0.f, bSelected ? 1.0f : 0.9f, 0.f, Alpha));
		}
		else
		{
			// Axis frame arrow: orient so the arrow's local +X aligns with the desired parent frame axis.
			// SetAxes(Right=localX, Up=localY, Forward=localZ) — M[0] is the "forward" in GetEuler convention.
			const FRotator PR  = PFrame.GetRotator();
			const FVector AX   = PR.GetForwardVector(); // Twist / X
			const FVector AY   = PR.GetRightVector();   // Swing1 / Y
			const FVector AZ   = PR.GetUpVector();      // Swing2 / Z

			FMatrix ArrowMat;
			FVector4 ArrowColor;
			float ArrowScale;

			if (Entry.VisualType == EPhysicsAssetConstraintVisualType::AxisX)
			{
				ArrowMat.SetAxes(AX, AY, AZ);
				ArrowColor = FVector4(bSelected ? 1.0f : 0.9f, 0.15f, 0.15f, Alpha);
				ArrowScale = VisualLength * ConstraintAxisArrowScale;
			}
			else if (Entry.VisualType == EPhysicsAssetConstraintVisualType::AxisY)
			{
				ArrowMat.SetAxes(AY, AZ, AX);
				ArrowColor = FVector4(0.15f, bSelected ? 1.0f : 0.9f, 0.15f, Alpha);
				ArrowScale = VisualLength * ConstraintAxisArrowScale;
			}
			else // AxisZ
			{
				ArrowMat.SetAxes(AZ, AX, AY);
				ArrowColor = FVector4(0.15f, 0.35f, bSelected ? 1.0f : 0.9f, Alpha);
				ArrowScale = VisualLength * ConstraintAxisZArrowScale;
			}

			SetRotIfChanged(ArrowMat.ToRotator().ToQuaternion());
			SetScaleIfChanged(FVector(ArrowScale, ArrowScale, ArrowScale));
			SetMaterialColorIfChanged(ArrowColor);
		}
	}
}

void FPhysicsAssetEditorWidget::ClearPreviewShapeComponents()
{
	for (FPreviewShapeComponentEntry& Entry : PreviewShapeComponents)
	{
		if (PreviewShapeActor && Entry.Component)
		{
			PreviewShapeActor->RemoveComponent(Entry.Component);
			Entry.Component = nullptr;
		}

		if (Entry.Material)
		{
			GUObjectArray.DestroyObject(Entry.Material);
			Entry.Material = nullptr;
		}
	}

	PreviewShapeComponents.clear();
}

UMaterial* FPhysicsAssetEditorWidget::CreatePreviewShapeMaterial(float Alpha, bool bDisableDepthTest) const
{
	if (!GEngine)
	{
		return nullptr;
	}

	UMaterial* BaseMaterial = FMaterialManager::Get().GetOrCreateMaterial("Asset/Materials/Auto/BasicShapeMaterial.mat");
	if (!BaseMaterial)
	{
		return nullptr;
	}

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	UMaterial* PreviewMaterial = BaseMaterial->CreateEditableCopy(Device);
	if (!PreviewMaterial)
	{
		return nullptr;
	}

	PreviewMaterial->SetAssetPathFileName(MakePhysicsAssetPreviewResourcePath("PreviewMaterial"));
	PreviewMaterial->SetBlendMode(EMaterialBlendMode::Translucent);
	if (bDisableDepthTest)
	{
		PreviewMaterial->SetDepthStencilState(EDepthStencilState::NoDepth);
		FShader* UnlitShader = FShaderManager::Get().GetOrCreate(
			FShaderKey(EShaderPath::UberLit, "VS_Static", "PS", EUberLitDefines::Unlit));
		if (UnlitShader)
			PreviewMaterial->SetShaderOverride(UnlitShader);
	}
	PreviewMaterial->SetVector4Parameter("SectionColor", FVector4(PreviewShapeTint.X, PreviewShapeTint.Y, PreviewShapeTint.Z, Alpha));
	return PreviewMaterial;
}

UStaticMeshComponent* FPhysicsAssetEditorWidget::CreatePreviewShapeComponent(UStaticMesh* StaticMesh, int32 BodyIndex, EPhysicsAssetPreviewShapeType ShapeType, int32 ShapeIndex, int32 PartIndex, const FVector& BaseMeshExtent)
{
	if (!PreviewShapeActor || !StaticMesh)
	{
		return nullptr;
	}

	UStaticMeshComponent* Component = PreviewShapeActor->AddComponent<UStaticMeshComponent>();
	if (!Component)
	{
		return nullptr;
	}

	Component->SetStaticMesh(StaticMesh);
	Component->SetCastShadow(false);
	UMaterial* PreviewMaterial = CreatePreviewShapeMaterial(PreviewBaseShapeOpacity);
	if (PreviewMaterial)
	{
		Component->SetMaterial(0, PreviewMaterial);
	}

	FPreviewShapeComponentEntry Entry;
	Entry.Component = Component;
	Entry.Material = PreviewMaterial;
	Entry.BodyIndex = BodyIndex;
	Entry.ShapeType = ShapeType;
	Entry.ShapeIndex = ShapeIndex;
	Entry.PartIndex = PartIndex;
	Entry.BaseMeshExtent = BaseMeshExtent;
	PreviewShapeComponents.push_back(Entry);
	return Component;
}

void FPhysicsAssetEditorWidget::RebuildPreviewShapeComponents(UPhysicsAsset* PhysicsAsset)
{
	ClearPreviewShapeComponents();
	ClearConstraintConeComponents();

	if (!PhysicsAsset || !PreviewShapeActor)
	{
		return;
	}

	const FVector CubeExtent = GetStaticMeshBoundsExtent(PreviewCubeMesh);
	const FVector SphereExtent = GetStaticMeshBoundsExtent(PreviewSphereMesh);
	const bool bUseUnifiedCapsuleMesh = PreviewCapsuleMesh != nullptr;
	const FVector UnifiedCapsuleExtent = GetStaticMeshBoundsExtent(PreviewCapsuleMesh);
	UStaticMesh* CapsuleCylinderMesh = PreviewOpenCylinderMesh ? PreviewOpenCylinderMesh : PreviewCylinderMesh;
	const FVector CylinderExtent = GetStaticMeshBoundsExtent(CapsuleCylinderMesh);
	UStaticMesh* CapsuleCapMesh = PreviewHemisphereMesh ? PreviewHemisphereMesh : PreviewSphereMesh;
	const FVector CapsuleCapExtent = GetStaticMeshBoundsExtent(CapsuleCapMesh);
	const TArray<UPhysicsBodySetup*>& BodySetups = PhysicsAsset->GetBodySetups();

	for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(BodySetups.size()); ++BodyIndex)
	{
		const UPhysicsBodySetup* BodySetup = BodySetups[BodyIndex];
		if (!BodySetup)
		{
			continue;
		}

		const FPhysicsAggregateShapeSetup& ShapeSetup = BodySetup->GetShapeSetup();
		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(ShapeSetup.SphereShapeSetups.size()); ++ShapeIndex)
		{
			CreatePreviewShapeComponent(PreviewSphereMesh, BodyIndex, EPhysicsAssetPreviewShapeType::Sphere, ShapeIndex, 0, SphereExtent);
		}
		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(ShapeSetup.BoxShapeSetups.size()); ++ShapeIndex)
		{
			CreatePreviewShapeComponent(PreviewCubeMesh, BodyIndex, EPhysicsAssetPreviewShapeType::Box, ShapeIndex, 0, CubeExtent);
		}
		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(ShapeSetup.CapsuleShapeSetups.size()); ++ShapeIndex)
		{
			if (bUseUnifiedCapsuleMesh)
			{
				CreatePreviewShapeComponent(PreviewCapsuleMesh, BodyIndex, EPhysicsAssetPreviewShapeType::Capsule, ShapeIndex, 0, UnifiedCapsuleExtent);
			}
			else
			{
				CreatePreviewShapeComponent(CapsuleCylinderMesh, BodyIndex, EPhysicsAssetPreviewShapeType::Capsule, ShapeIndex, 0, CylinderExtent);
				CreatePreviewShapeComponent(CapsuleCapMesh, BodyIndex, EPhysicsAssetPreviewShapeType::Capsule, ShapeIndex, 1, CapsuleCapExtent);
				CreatePreviewShapeComponent(CapsuleCapMesh, BodyIndex, EPhysicsAssetPreviewShapeType::Capsule, ShapeIndex, 2, CapsuleCapExtent);
			}
		}
		// Convex collision is intentionally excluded from preview rendering.
	}

	SyncPreviewShapeComponents(PhysicsAsset);
	// Constraint fans use NoDepth and must be submitted after translucent body shapes
	// so body preview blending cannot wash out the constraint color.
	RebuildConstraintConeComponents(PhysicsAsset);
	SyncConstraintConeComponents(PhysicsAsset);
}

void FPhysicsAssetEditorWidget::SyncPreviewShapeComponents(UPhysicsAsset* PhysicsAsset)
{
	if (!PhysicsAsset || PreviewShapeComponents.empty())
	{
		return;
	}

	USkeletalMeshComponent* PreviewMeshComponent = ViewportClient.GetPreviewMeshComponent();
	if (!PreviewMeshComponent)
	{
		return;
	}

	const FSkeletonAsset* SkeletonAsset = PreviewSkeletalMesh ? PreviewSkeletalMesh->GetSkeletonAsset() : nullptr;
	TArray<FMatrix> BoneGlobalMatrices;
	PreviewMeshComponent->GetCurrentBoneGlobalMatrices(BoneGlobalMatrices);

	auto ApplyEntryTransform =
		[&](FPreviewShapeComponentEntry& Entry, const UPhysicsBodySetup* BodySetup, const FMatrix& LocalMatrix, const FVector& TargetMeshExtent)
		{
			if (!Entry.Component || !BodySetup)
			{
				return;
			}

			const int32 BoneIndex = FindBoneIndexByName(SkeletonAsset, BodySetup->GetTargetBoneName());
			const FMatrix BoneMatrix = BuildBoneWorldMatrix(PreviewMeshComponent, &BoneGlobalMatrices, BoneIndex);

			FTransform WorldTransform = ComposeMatrixToTransform(LocalMatrix, BoneMatrix);
			const FVector BaseExtent(
				(std::max)(Entry.BaseMeshExtent.X, 0.001f),
				(std::max)(Entry.BaseMeshExtent.Y, 0.001f),
				(std::max)(Entry.BaseMeshExtent.Z, 0.001f));
			const FVector MeshScale(
				TargetMeshExtent.X / BaseExtent.X,
				TargetMeshExtent.Y / BaseExtent.Y,
				TargetMeshExtent.Z / BaseExtent.Z);

			const FVector NewLocation = WorldTransform.Location;
			const FQuat NewRotation = WorldTransform.Rotation;
			const FVector NewScale(
				WorldTransform.Scale.X * MeshScale.X,
				WorldTransform.Scale.Y * MeshScale.Y,
				WorldTransform.Scale.Z * MeshScale.Z);

			constexpr float PosTolerance = 1e-4f;
			constexpr float ScaleTolerance = 1e-5f;
			const FVector CurLoc = Entry.Component->GetRelativeLocation();
			const FVector CurScale = Entry.Component->GetRelativeScale();
			const bool bLocationChanged =
				std::fabs(CurLoc.X - NewLocation.X) > PosTolerance ||
				std::fabs(CurLoc.Y - NewLocation.Y) > PosTolerance ||
				std::fabs(CurLoc.Z - NewLocation.Z) > PosTolerance;
			const bool bRotationChanged = !Entry.Component->GetRelativeQuat().Equals(NewRotation, 1e-5f);
			const bool bScaleChanged =
				std::fabs(CurScale.X - NewScale.X) > ScaleTolerance ||
				std::fabs(CurScale.Y - NewScale.Y) > ScaleTolerance ||
				std::fabs(CurScale.Z - NewScale.Z) > ScaleTolerance;

			if (bLocationChanged)
				Entry.Component->SetRelativeLocation(NewLocation);
			if (bRotationChanged)
				Entry.Component->SetRelativeRotation(NewRotation);
			if (bScaleChanged)
				Entry.Component->SetRelativeScale(NewScale);
			if (Entry.Component->IsVisible() != bShowPreviewBodies)
				Entry.Component->SetVisibility(bShowPreviewBodies);

			if (Entry.Material)
			{
				// Body preview opacity is independent from constraint selection.
				// Selecting a constraint should not make its parent/child body suddenly opaque.
				Entry.Material->SetVector4Parameter("SectionColor", FVector4(PreviewShapeTint.X, PreviewShapeTint.Y, PreviewShapeTint.Z, PreviewBaseShapeOpacity));
			}
		};

	int32 EntryIndex = 0;
	const TArray<UPhysicsBodySetup*>& BodySetups = PhysicsAsset->GetBodySetups();
	for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(BodySetups.size()); ++BodyIndex)
	{
		const UPhysicsBodySetup* BodySetup = BodySetups[BodyIndex];
		if (!BodySetup)
		{
			continue;
		}

		const FPhysicsAggregateShapeSetup& ShapeSetup = BodySetup->GetShapeSetup();

		for (const FPhysicsSphereShapeSetup& Shape : ShapeSetup.SphereShapeSetups)
		{
			if (EntryIndex >= static_cast<int32>(PreviewShapeComponents.size()))
			{
				return;
			}
			ApplyEntryTransform(PreviewShapeComponents[EntryIndex++], BodySetup, Shape.LocalTransform.ToMatrix(), FVector(Shape.Radius, Shape.Radius, Shape.Radius));
		}

		for (const FPhysicsBoxShapeSetup& Shape : ShapeSetup.BoxShapeSetups)
		{
			if (EntryIndex >= static_cast<int32>(PreviewShapeComponents.size()))
			{
				return;
			}
			ApplyEntryTransform(PreviewShapeComponents[EntryIndex++], BodySetup, Shape.LocalTransform.ToMatrix(), Shape.HalfExtent);
		}

		for (const FPhysicsCapsuleShapeSetup& Shape : ShapeSetup.CapsuleShapeSetups)
		{
			const bool bUseUnifiedCapsuleMesh = PreviewCapsuleMesh != nullptr;
			const FMatrix ShapeLocalMatrix = Shape.LocalTransform.ToMatrix();
			if (bUseUnifiedCapsuleMesh)
			{
				if (EntryIndex >= static_cast<int32>(PreviewShapeComponents.size()))
				{
					return;
				}

				const FMatrix CapsuleCorrectionMatrix = MakeAxisAlignmentMatrix(PositiveXMeshAxis, PhysicsAssetCapsuleAxis);
				const FVector CapsuleExtent(Shape.Length * 0.5f + Shape.Radius, Shape.Radius, Shape.Radius);
				ApplyEntryTransform(PreviewShapeComponents[EntryIndex++], BodySetup, CapsuleCorrectionMatrix * ShapeLocalMatrix, CapsuleExtent);
				continue;
			}

			if (EntryIndex + 2 >= static_cast<int32>(PreviewShapeComponents.size()))
			{
				return;
			}

			UStaticMesh* CapsuleCylinderMesh = PreviewOpenCylinderMesh ? PreviewOpenCylinderMesh : PreviewCylinderMesh;
			if (!CapsuleCylinderMesh && PreviewShapeComponents[EntryIndex].Component)
			{
				CapsuleCylinderMesh = PreviewShapeComponents[EntryIndex].Component->GetStaticMesh();
			}
			const int32 DominantAxisIndex = GetLeastNormalAxisIndex(CapsuleCylinderMesh);
			const float HalfLength = Shape.Length * 0.5f;
			const FMatrix CylinderCorrectionMatrix = MakeMeshAxisToTargetAxisMatrix(DominantAxisIndex, PhysicsAssetCapsuleAxis);
			const FMatrix CylinderLocalMatrix = CylinderCorrectionMatrix * ShapeLocalMatrix;
			const FMatrix PositiveCapCorrectionMatrix = MakeAxisAlignmentMatrix(PositiveXMeshAxis, PhysicsAssetCapsuleAxis);
			const FMatrix NegativeCapCorrectionMatrix = MakeAxisAlignmentMatrix(PositiveXMeshAxis, PhysicsAssetCapsuleAxis * -1.0f);
			const bool bUsingHemisphereMesh = PreviewHemisphereMesh != nullptr;
			const FVector CapsuleCapExtent = bUsingHemisphereMesh
				? FVector(Shape.Radius * 0.5f, Shape.Radius, Shape.Radius)
				: FVector(Shape.Radius, Shape.Radius, Shape.Radius);
			const FMatrix PositiveCapLocalMatrix = PositiveCapCorrectionMatrix
				* FMatrix::MakeTranslationMatrix(PhysicsAssetCapsuleAxis * HalfLength)
				* ShapeLocalMatrix;
			const FMatrix NegativeCapLocalMatrix = NegativeCapCorrectionMatrix
				* FMatrix::MakeTranslationMatrix(PhysicsAssetCapsuleAxis * -HalfLength)
				* ShapeLocalMatrix;

			ApplyEntryTransform(PreviewShapeComponents[EntryIndex++], BodySetup, CylinderLocalMatrix, MakeAxisAlignedExtent(DominantAxisIndex, Shape.Radius, HalfLength));
			ApplyEntryTransform(PreviewShapeComponents[EntryIndex++], BodySetup, PositiveCapLocalMatrix, CapsuleCapExtent);
			ApplyEntryTransform(PreviewShapeComponents[EntryIndex++], BodySetup, NegativeCapLocalMatrix, CapsuleCapExtent);
		}

	}
}

namespace
{
	// Swing cone outline: forward axis is the cone axis and Y/Z radii come from swing2/swing1.
	void DrawSwingCone(
		UWorld* World,
		const FVector& Apex,
		const FVector& ForwardAxis,
		const FVector& SideAxisY,
		const FVector& SideAxisZ,
		float Swing1LimitDeg,
		float Swing2LimitDeg,
		float Length,
		const FColor& Color,
		float Duration)
	{
		if (Swing1LimitDeg <= 0.0f && Swing2LimitDeg <= 0.0f) return;

		const float RadiusY = sinf((std::max)(0.0f, (std::min)(Swing2LimitDeg, 89.0f)) * Pi / 180.0f) * Length;
		const float RadiusZ = sinf((std::max)(0.0f, (std::min)(Swing1LimitDeg, 89.0f)) * Pi / 180.0f) * Length;
		const FVector BaseCenter = Apex + ForwardAxis * Length;
		constexpr int32 N = 24;
		FVector PrevPoint = FVector::ZeroVector;

		for (int32 i = 0; i <= N; ++i)
		{
			const float Theta = (2.0f * Pi * static_cast<float>(i)) / static_cast<float>(N);
			const FVector Point = BaseCenter
				+ SideAxisY * (cosf(Theta) * RadiusY)
				+ SideAxisZ * (sinf(Theta) * RadiusZ);

			if (i > 0)
				DrawDebugLineAlwaysOnTop(World, PrevPoint, Point, Color, Duration);
			// 양 끝과 중심 방사선
			if (i == 0 || i == N / 4 || i == N / 2 || i == (3 * N) / 4)
				DrawDebugLineAlwaysOnTop(World, Apex, Point, Color, Duration);

			PrevPoint = Point;
		}

		DrawDebugLineAlwaysOnTop(World, Apex, BaseCenter, Color, Duration);
	}

	// Twist fan outline in the YZ plane around the twist axis.
	void DrawTwistFan(
		UWorld* World,
		const FVector& Origin,
		const FVector& AxisY,
		const FVector& AxisZ,
		float HalfAngleDeg,
		float Radius,
		const FColor& Color,
		float Duration)
	{
		const float LimitDeg = (std::max)(0.0f, (std::min)(HalfAngleDeg, 179.0f));
		if (LimitDeg <= 0.0f) return;

		const float TMin = -LimitDeg;
		const float TMax = LimitDeg;
		constexpr int32 N = 20;
		FVector PrevPoint = FVector::ZeroVector;

		for (int32 i = 0; i <= N; ++i)
		{
			const float T    = TMin + (TMax - TMin) * i / N;
			const float TRad = T * Pi / 180.0f;
			const FVector Point = Origin + (AxisY * std::cos(TRad) + AxisZ * std::sin(TRad)) * Radius;

			if (i > 0)
				DrawDebugLineAlwaysOnTop(World, PrevPoint, Point, Color, Duration);
			if (i == 0 || i == N || i == N / 2)
				DrawDebugLineAlwaysOnTop(World, Origin, Point, Color, Duration);

			PrevPoint = Point;
		}
	}
}

void FPhysicsAssetEditorWidget::DrawConstraintDebug(UPhysicsAsset* PhysicsAsset)
{
	UWorld* PreviewWorld = ViewportClient.GetPreviewWorld();
	if (PreviewWorld)
	{
		PreviewWorld->GetScene().GetDebugDrawQueue().Clear();
	}

	if (!PhysicsAsset || !bShowConstraintDebug || !ViewportClient.GetRenderOptions().ShowFlags.bDebugDraw)
	{
		return;
	}

	USkeletalMeshComponent* PreviewMeshComponent = ViewportClient.GetPreviewMeshComponent();
	const FSkeletonAsset* SkeletonAsset = PreviewSkeletalMesh ? PreviewSkeletalMesh->GetSkeletonAsset() : nullptr;
	if (!PreviewMeshComponent || !PreviewWorld || !SkeletonAsset)
	{
		return;
	}

	constexpr float ConstraintDebugDuration = 0.0f;
	TArray<FMatrix> BoneGlobalMatrices;
	PreviewMeshComponent->GetCurrentBoneGlobalMatrices(BoneGlobalMatrices);

	for (int32 ConstraintIndex = 0; ConstraintIndex < static_cast<int32>(PhysicsAsset->GetConstraintSetups().size()); ++ConstraintIndex)
	{
		const FPhysicsConstraintSetup& Constraint = PhysicsAsset->GetConstraintSetups()[ConstraintIndex];
		const int32 ParentBoneIndex = FindBoneIndexByName(SkeletonAsset, Constraint.ParentBoneName);
		const int32 ChildBoneIndex = FindBoneIndexByName(SkeletonAsset, Constraint.ChildBoneName);
		if (ParentBoneIndex < 0 || ChildBoneIndex < 0)
		{
			continue;
		}

		const FMatrix ParentBoneMatrix = BuildBoneWorldMatrix(PreviewMeshComponent, &BoneGlobalMatrices, ParentBoneIndex);
		const FMatrix ChildBoneMatrix = BuildBoneWorldMatrix(PreviewMeshComponent, &BoneGlobalMatrices, ChildBoneIndex);
		const FTransform ParentFrame(Constraint.ParentLocalFrame.ToMatrix() * ParentBoneMatrix);
		const FTransform ChildFrame(Constraint.ChildLocalFrame.ToMatrix() * ChildBoneMatrix);
		const bool bSelected = ConstraintIndex == SelectedConstraintIndex;
		if (!bSelected)
		{
			continue;
		}

		const FRotator Rotation = ParentFrame.GetRotator();
		const FVector AxisX = Rotation.GetForwardVector();
		const FVector AxisY = Rotation.GetRightVector();
		const FVector AxisZ = Rotation.GetUpVector();
		const FVector Origin = (ParentFrame.Location + ChildFrame.Location) * 0.5f;
		const float Swing1Limit = GetSwingVisualHalfAngle(Constraint.Swing1Motion, Constraint.SwingLimitY);
		const float Swing2Limit = GetSwingVisualHalfAngle(Constraint.Swing2Motion, Constraint.SwingLimitZ);
		const float TwistLimit = GetTwistVisualHalfAngle(Constraint);

		// 선택된 constraint는 2배 크기로 렌더
		const float VisualLength = bSelected ? ConstraintAxisLength * 2.0f : ConstraintAxisLength;

		// 짧은 방향 축선 (R=Swing1/Y, G=Swing2/Z, B=Twist/X)
		const float SwingVisualLength = VisualLength * ConstraintSwingVisualScale;
		const float TwistVisualLength = VisualLength * ConstraintTwistVisualScale;
		const float AxisLineLen = SwingVisualLength * 0.35f;
		DrawDebugLineAlwaysOnTop(PreviewWorld, Origin, Origin + AxisY * AxisLineLen, bSelected ? FColor(255, 130, 130) : FColor(220, 60, 60), ConstraintDebugDuration);
		DrawDebugLineAlwaysOnTop(PreviewWorld, Origin, Origin + AxisZ * AxisLineLen, bSelected ? FColor(255, 130, 130) : FColor(220, 60, 60), ConstraintDebugDuration);
		DrawDebugLineAlwaysOnTop(PreviewWorld, Origin, Origin + AxisX * AxisLineLen, bSelected ? FColor(130, 255, 130) : FColor(60, 200, 60), ConstraintDebugDuration);

		// Filled swing/twist limit visuals for the selected constraint are rendered by
		// RenderSelectedConstraintLimitOverlay(). That path rebuilds no UStaticMesh and
		// therefore stays responsive while dragging numeric limit values.
	}
}

void FPhysicsAssetEditorWidget::RenderSelectedConstraintLimitOverlay(
	UPhysicsAsset* PhysicsAsset,
	ImDrawList* DrawList,
	const ImVec2& ViewportPos,
	const ImVec2& ViewportSize,
	float ToolbarHeight)
{
	if (!PhysicsAsset || !DrawList || !bShowConstraintDebug || SelectedConstraintIndex < 0
		|| SelectedConstraintIndex >= static_cast<int32>(PhysicsAsset->GetConstraintSetups().size()))
	{
		return;
	}

	USkeletalMeshComponent* PreviewMeshComponent = ViewportClient.GetPreviewMeshComponent();
	const FSkeletonAsset* SkeletonAsset = PreviewSkeletalMesh ? PreviewSkeletalMesh->GetSkeletonAsset() : nullptr;
	if (!PreviewMeshComponent || !SkeletonAsset)
	{
		return;
	}

	FMinimalViewInfo POV;
	if (!ViewportClient.GetCameraView(POV))
	{
		return;
	}

	const FPhysicsConstraintSetup& Constraint = PhysicsAsset->GetConstraintSetups()[SelectedConstraintIndex];
	const int32 ParentBoneIndex = FindBoneIndexByName(SkeletonAsset, Constraint.ParentBoneName);
	const int32 ChildBoneIndex = FindBoneIndexByName(SkeletonAsset, Constraint.ChildBoneName);
	if (ParentBoneIndex < 0 || ChildBoneIndex < 0)
	{
		return;
	}

	TArray<FMatrix> BoneGlobalMatrices;
	PreviewMeshComponent->GetCurrentBoneGlobalMatrices(BoneGlobalMatrices);
	const FTransform ParentFrame(Constraint.ParentLocalFrame.ToMatrix() * BuildBoneWorldMatrix(PreviewMeshComponent, &BoneGlobalMatrices, ParentBoneIndex));
	const FTransform ChildFrame(Constraint.ChildLocalFrame.ToMatrix() * BuildBoneWorldMatrix(PreviewMeshComponent, &BoneGlobalMatrices, ChildBoneIndex));

	const FRotator Rotation = ParentFrame.GetRotator();
	const FVector AxisX = Rotation.GetForwardVector();
	const FVector AxisY = Rotation.GetRightVector();
	const FVector AxisZ = Rotation.GetUpVector();
	const FVector Origin = (ParentFrame.Location + ChildFrame.Location) * 0.5f;

	const float Swing1Limit = GetSwingVisualHalfAngle(Constraint.Swing1Motion, Constraint.SwingLimitY);
	const float Swing2Limit = GetSwingVisualHalfAngle(Constraint.Swing2Motion, Constraint.SwingLimitZ);
	const float TwistLimit = GetTwistVisualHalfAngle(Constraint);

	const float VisualLength = ConstraintAxisLength * 2.0f;
	const float SwingLength = VisualLength * ConstraintSwingVisualScale;
	const float TwistRadius = VisualLength * ConstraintTwistVisualScale;
	const FMatrix ViewProjection = POV.CalculateViewProjectionMatrix();
	const FVector CameraForward = POV.Rotation.GetForwardVector();

	auto Project = [&](const FVector& WorldPosition, ImVec2& OutScreen) -> bool
	{
		if ((WorldPosition - POV.Location).Dot(CameraForward) <= POV.NearClip)
		{
			return false;
		}

		const FVector Clip = ViewProjection.TransformPositionWithW(WorldPosition);
		OutScreen.x = ViewportPos.x + (Clip.X * 0.5f + 0.5f) * ViewportSize.x;
		OutScreen.y = ViewportPos.y + (1.0f - (Clip.Y * 0.5f + 0.5f)) * ViewportSize.y;
		return std::isfinite(OutScreen.x) && std::isfinite(OutScreen.y);
	};

	auto AddProjectedLine = [&](const FVector& A, const FVector& B, ImU32 Color, float Thickness = 1.5f)
	{
		ImVec2 SA, SB;
		if (Project(A, SA) && Project(B, SB))
		{
			DrawList->AddLine(SA, SB, Color, Thickness);
		}
	};

	auto AddProjectedTriangle = [&](const FVector& A, const FVector& B, const FVector& C, ImU32 FillColor, ImU32 EdgeColor)
	{
		ImVec2 SA, SB, SC;
		if (Project(A, SA) && Project(B, SB) && Project(C, SC))
		{
			DrawList->AddTriangleFilled(SA, SB, SC, FillColor);
			DrawList->AddTriangle(SA, SB, SC, EdgeColor, 0.75f);
		}
	};

	DrawList->PushClipRect(
		ImVec2(ViewportPos.x, ViewportPos.y + ToolbarHeight),
		ImVec2(ViewportPos.x + ViewportSize.x, ViewportPos.y + ViewportSize.y),
		true);

	const ImU32 SwingFill = IM_COL32(255, 70, 70, 58);
	const ImU32 SwingEdge = IM_COL32(255, 135, 135, 230);
	const ImU32 TwistFill = IM_COL32(70, 255, 70, 58);
	const ImU32 TwistEdge = IM_COL32(135, 255, 135, 230);

	// Small local axes stay visible even when the filled surfaces overlap the body.
	const float AxisLineLength = SwingLength * 0.35f;
	AddProjectedLine(Origin, Origin + AxisY * AxisLineLength, SwingEdge, 2.0f);
	AddProjectedLine(Origin, Origin + AxisZ * AxisLineLength, SwingEdge, 2.0f);
	AddProjectedLine(Origin, Origin + AxisX * AxisLineLength, TwistEdge, 2.0f);

	if (ShouldRenderSwingCone(Swing1Limit, Swing2Limit))
	{
		const float RadiusY = sinf((std::clamp)(Swing2Limit, 0.0f, 89.0f) * Pi / 180.0f) * SwingLength;
		const float RadiusZ = sinf((std::clamp)(Swing1Limit, 0.0f, 89.0f) * Pi / 180.0f) * SwingLength;
		const FVector BaseCenter = Origin + AxisX * SwingLength;
		constexpr int32 Segments = 32;
		FVector FirstPoint = FVector::ZeroVector;
		FVector PrevPoint = FVector::ZeroVector;

		for (int32 SegmentIndex = 0; SegmentIndex <= Segments; ++SegmentIndex)
		{
			const float Theta = (2.0f * Pi * static_cast<float>(SegmentIndex)) / static_cast<float>(Segments);
			const FVector Point = BaseCenter
				+ AxisY * (cosf(Theta) * RadiusY)
				+ AxisZ * (sinf(Theta) * RadiusZ);

			if (SegmentIndex == 0)
			{
				FirstPoint = Point;
			}
			else
			{
				AddProjectedTriangle(Origin, PrevPoint, Point, SwingFill, SwingEdge);
				AddProjectedLine(PrevPoint, Point, SwingEdge, 1.35f);
			}

			PrevPoint = Point;
		}

		AddProjectedLine(Origin, BaseCenter, SwingEdge, 1.5f);
		AddProjectedLine(Origin, FirstPoint, SwingEdge, 1.35f);
	}

	if (TwistLimit > 0.0f)
	{
		const float LimitDeg = (std::clamp)(TwistLimit, 0.0f, 179.0f);
		constexpr int32 Segments = 32;
		FVector PrevPoint = FVector::ZeroVector;
		for (int32 SegmentIndex = 0; SegmentIndex <= Segments; ++SegmentIndex)
		{
			const float T = -LimitDeg + (2.0f * LimitDeg * static_cast<float>(SegmentIndex) / static_cast<float>(Segments));
			const float TRad = T * Pi / 180.0f;
			const FVector Point = Origin + (AxisY * cosf(TRad) + AxisZ * sinf(TRad)) * TwistRadius;
			if (SegmentIndex > 0)
			{
				AddProjectedTriangle(Origin, PrevPoint, Point, TwistFill, TwistEdge);
				AddProjectedLine(PrevPoint, Point, TwistEdge, 1.35f);
			}
			PrevPoint = Point;
		}
	}

	DrawList->PopClipRect();
}

void FPhysicsAssetEditorWidget::StartPreviewSimulation(UPhysicsAsset* PhysicsAsset)
{
	if (!PhysicsAsset || bPreviewSimulating)
	{
		return;
	}

	UWorld* PreviewWorld = ViewportClient.GetPreviewWorld();
	USkeletalMeshComponent* MeshComponent = ViewportClient.GetPreviewMeshComponent();
	if (!PreviewWorld || !MeshComponent || !PreviewSkeletalMesh)
	{
		return;
	}

	MeshComponent->SetWorldLocation(FVector::ZeroVector);
	MeshComponent->SetRelativeRotation(FQuat::Identity);
	MeshComponent->SetSkeletalMesh(PreviewSkeletalMesh);
	MeshComponent->SetEnableGravity(true);
	MeshComponent->SetCollisionObjectType(ECollisionChannel::ECC_PhysicsBody);
	MeshComponent->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
	MeshComponent->SetPhysicsAsset(PhysicsAsset);
	MeshComponent->SetRagdollSelfCollisionEnabled(bPreviewEnableSelfCollision);
	MeshComponent->SetSimulatePhysics(true);

	if (!PreviewWorld->HasBegunPlay())
	{
		PreviewWorld->BeginPlay();
	}

	bPreviewSimulating = MeshComponent->IsRagdollActive();
	PreviewSimulationTime = 0.0f;
	SetEditorPreviewOverlaysVisible(true);
}

void FPhysicsAssetEditorWidget::StopPreviewSimulation(bool bResetPose)
{
	USkeletalMeshComponent* MeshComponent = ViewportClient.GetPreviewMeshComponent();
	if (MeshComponent)
	{
		MeshComponent->SetSimulatePhysics(false);
		MeshComponent->SetPhysicsAsset(nullptr);
		if (bResetPose && PreviewSkeletalMesh)
		{
			RestorePreviewMeshToEditorPose();
		}
	}

	bPreviewSimulating = false;
	PreviewSimulationTime = 0.0f;
	SetEditorPreviewOverlaysVisible(true);
}

void FPhysicsAssetEditorWidget::RestorePreviewMeshToEditorPose()
{
	USkeletalMeshComponent* MeshComponent = ViewportClient.GetPreviewMeshComponent();
	if (!MeshComponent)
	{
		return;
	}

	if (AActor* PreviewActor = MeshComponent->GetOwner())
	{
		PreviewActor->SetActorLocation(FVector::ZeroVector);
		PreviewActor->SetActorRotation(FRotator::ZeroRotator);
	}

	MeshComponent->ResetPhysicsInterpolation();
	MeshComponent->SetWorldLocation(FVector::ZeroVector);
	MeshComponent->SetRelativeRotation(FQuat::Identity);

	if (PreviewSkeletalMesh)
	{
		MeshComponent->SetSkeletalMesh(nullptr);
		MeshComponent->SetSkeletalMesh(PreviewSkeletalMesh);
		MeshComponent->ResetBoneEditPose();
	}

	MeshComponent->ResetPhysicsInterpolation();
}

void FPhysicsAssetEditorWidget::ResetPreviewSimulation(UPhysicsAsset* PhysicsAsset)
{
	StopPreviewSimulation(true);
	StartPreviewSimulation(PhysicsAsset);
}

void FPhysicsAssetEditorWidget::TickPreviewSimulation(float DeltaTime)
{
	if (!bPreviewSimulating)
	{
		return;
	}

	USkeletalMeshComponent* MeshComponent = ViewportClient.GetPreviewMeshComponent();
	if (!MeshComponent || !MeshComponent->IsRagdollActive())
	{
		StopPreviewSimulation(true);
		return;
	}

	const float ClampedDeltaTime = (std::clamp)(DeltaTime, 0.0f, 1.0f / 15.0f);
	if (ClampedDeltaTime <= 0.0f)
	{
		return;
	}

	PreviewSimulationTime += ClampedDeltaTime;
}

void FPhysicsAssetEditorWidget::SetEditorPreviewOverlaysVisible(bool bVisible)
{
	for (FPreviewShapeComponentEntry& Entry : PreviewShapeComponents)
	{
		if (Entry.Component)
		{
			Entry.Component->SetVisibility(bVisible && bShowPreviewBodies);
		}
	}

	for (FPreviewConstraintConeEntry& Entry : PreviewConstraintCones)
	{
		if (Entry.Component)
		{
			Entry.Component->SetVisibility(bVisible && bShowConstraintDebug);
		}
	}

	if (UGizmoComponent* Gizmo = ViewportClient.GetGizmo())
	{
		Gizmo->SetVisibility(bVisible);
	}
}

void FPhysicsAssetEditorWidget::Close()
{
	StopPreviewSimulation(false);
	ClearConstraintConeComponents();
	ClearPreviewShapeComponents();
	PreviewShapeActor = nullptr;
	PreviewCubeMesh = nullptr;
	PreviewSphereMesh = nullptr;
	if (PreviewHemisphereMesh)
	{
		GUObjectArray.DestroyObject(PreviewHemisphereMesh);
		PreviewHemisphereMesh = nullptr;
	}
	PreviewCylinderMesh = nullptr;
	if (PreviewOpenCylinderMesh)
	{
		GUObjectArray.DestroyObject(PreviewOpenCylinderMesh);
		PreviewOpenCylinderMesh = nullptr;
	}
	if (PreviewCapsuleMesh)
	{
		GUObjectArray.DestroyObject(PreviewCapsuleMesh);
		PreviewCapsuleMesh = nullptr;
	}
	if (BodyGizmoTarget)
	{
		delete BodyGizmoTarget;
		BodyGizmoTarget = nullptr;
	}
	if (ConstraintGizmoTarget)
	{
		delete ConstraintGizmoTarget;
		ConstraintGizmoTarget = nullptr;
	}

	if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
	{
		FScene& PreviewScene = PreviewWorld->GetScene();
		GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);

		if (PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(PreviewWorldHandle);
		}
	}

	FSlateApplication::Get().UnregisterViewport(&ViewportClient);
	ViewportClient.Release();
	PreviewSkeletalMesh = nullptr;
	bPendingClose = false;

	FAssetEditorWidget::Close();
}

void FPhysicsAssetEditorWidget::Tick(float DeltaTime)
{
	if (ViewportClient.IsRenderable())
	{
		TickPreviewSimulation(DeltaTime);
		ViewportClient.Tick(DeltaTime);
		UPhysicsAsset* PhysicsAsset = static_cast<UPhysicsAsset*>(EditedObject);
		const bool bNeedsConstraintDefinitionRefresh = bConstraintPreviewDefinitionDirty || NeedsConstraintConeRebuild(PhysicsAsset);
		if (bNeedsConstraintDefinitionRefresh)
		{
			RebuildConstraintConeComponents(PhysicsAsset);
		}
		bConstraintPreviewDefinitionDirty = false;
		bConstraintPreviewTransformDirty = false;
		SyncPreviewShapeComponents(PhysicsAsset);
		SyncConstraintConeComponents(PhysicsAsset);
		DrawConstraintDebug(PhysicsAsset);
	}
}

void FPhysicsAssetEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen() && ViewportClient.IsRenderable())
	{
		OutClients.push_back(const_cast<FMeshEditorViewportClient*>(&ViewportClient));
	}
}

void FPhysicsAssetEditorWidget::SelectBoneByIndex(int32 BoneIndex)
{
	SelectedBoneIndex = BoneIndex;
	SelectedConstraintIndex = -1;
	SelectedShapeType = EPhysicsAssetPreviewShapeType::None;
	SelectedShapeIndex = -1;

	UPhysicsAsset* PhysicsAsset = static_cast<UPhysicsAsset*>(EditedObject);
	if (PhysicsAsset && PreviewSkeletalMesh && BoneIndex >= 0)
	{
		if (const FSkeletonAsset* SkeletonAsset = PreviewSkeletalMesh->GetSkeletonAsset())
		{
			if (BoneIndex < static_cast<int32>(SkeletonAsset->Bones.size()))
			{
				SelectedBodyIndex = FindBodySetupIndex(PhysicsAsset, FName(SkeletonAsset->Bones[BoneIndex].Name));
			}
		}
	}

	SyncSelectionToViewport();
}

void FPhysicsAssetEditorWidget::SelectBodyShape(int32 BodyIndex, EPhysicsAssetPreviewShapeType ShapeType, int32 ShapeIndex)
{
	UPhysicsAsset* PhysicsAsset = static_cast<UPhysicsAsset*>(EditedObject);
	if (!PhysicsAsset || BodyIndex < 0 || BodyIndex >= static_cast<int32>(PhysicsAsset->GetBodySetups().size()))
	{
		return;
	}

	SelectedBodyIndex = BodyIndex;
	SelectedConstraintIndex = -1;
	SelectedShapeType = ShapeType;
	SelectedShapeIndex = ShapeIndex;
	if (UPhysicsBodySetup* BodySetup = PhysicsAsset->GetBodySetups()[BodyIndex])
	{
		SelectedBoneIndex = FindBoneIndexByName(PreviewSkeletalMesh ? PreviewSkeletalMesh->GetSkeletonAsset() : nullptr, BodySetup->GetTargetBoneName());
	}

	SyncSelectionToViewport();
}

void FPhysicsAssetEditorWidget::SelectBodyByIndex(int32 BodyIndex)
{
	UPhysicsAsset* PhysicsAsset = static_cast<UPhysicsAsset*>(EditedObject);
	if (!PhysicsAsset || BodyIndex < 0 || BodyIndex >= static_cast<int32>(PhysicsAsset->GetBodySetups().size()))
	{
		return;
	}

	SelectedBodyIndex = BodyIndex;
	SelectedConstraintIndex = -1;
	SelectedShapeType = EPhysicsAssetPreviewShapeType::None;
	SelectedShapeIndex = -1;
	if (UPhysicsBodySetup* BodySetup = PhysicsAsset->GetBodySetups()[BodyIndex])
	{
		SelectedBoneIndex = FindBoneIndexByName(PreviewSkeletalMesh ? PreviewSkeletalMesh->GetSkeletonAsset() : nullptr, BodySetup->GetTargetBoneName());
		FindPreferredShapeSelection(BodySetup, SelectedShapeType, SelectedShapeIndex);
	}


	SyncSelectionToViewport();
}

void FPhysicsAssetEditorWidget::SelectConstraintByIndex(int32 ConstraintIndex)
{
	UPhysicsAsset* PhysicsAsset = static_cast<UPhysicsAsset*>(EditedObject);
	if (!PhysicsAsset || ConstraintIndex < 0 || ConstraintIndex >= static_cast<int32>(PhysicsAsset->GetConstraintSetups().size()))
	{
		return;
	}

	SelectedConstraintIndex = ConstraintIndex;
	SelectedBodyIndex = -1;
	SelectedShapeType = EPhysicsAssetPreviewShapeType::None;
	SelectedShapeIndex = -1;
	SelectedBoneIndex = FindBoneIndexByName(
		PreviewSkeletalMesh ? PreviewSkeletalMesh->GetSkeletonAsset() : nullptr,
		PhysicsAsset->GetConstraintSetups()[ConstraintIndex].ChildBoneName);
	bScrollToSelectedConstraintOnNextRender = true;

	SyncSelectionToViewport();
}

void FPhysicsAssetEditorWidget::ClearSelection()
{
	SelectedBoneIndex = -1;
	SelectedBodyIndex = -1;
	SelectedConstraintIndex = -1;
	SelectedShapeType = EPhysicsAssetPreviewShapeType::None;
	SelectedShapeIndex = -1;

	SyncSelectionToViewport();
}

void FPhysicsAssetEditorWidget::SyncSelectionToViewport()
{
	if (UGizmoComponent* Gizmo = ViewportClient.GetGizmo())
	{
		if (BodyGizmoTarget)
		{
			static_cast<FPhysicsBodyGizmoTarget*>(BodyGizmoTarget)->Clear();
		}
		if (ConstraintGizmoTarget)
		{
			static_cast<FPhysicsConstraintGizmoTarget*>(ConstraintGizmoTarget)->Clear();
		}

		if (BodyGizmoTarget && SelectedBodyIndex >= 0)
		{
			static_cast<FPhysicsBodyGizmoTarget*>(BodyGizmoTarget)->SetSelection(
				ViewportClient.GetPreviewMeshComponent(),
				static_cast<UPhysicsAsset*>(EditedObject),
				SelectedBodyIndex,
				SelectedShapeType,
				SelectedShapeIndex);
			Gizmo->SetTarget(BodyGizmoTarget);
			ViewportClient.ApplyTransformSettingsToGizmo();
		}
		else if (ConstraintGizmoTarget && SelectedConstraintIndex >= 0)
		{
			static_cast<FPhysicsConstraintGizmoTarget*>(ConstraintGizmoTarget)->SetSelection(
				ViewportClient.GetPreviewMeshComponent(),
				static_cast<UPhysicsAsset*>(EditedObject),
				SelectedConstraintIndex);
			Gizmo->SetTarget(ConstraintGizmoTarget);
			ViewportClient.ApplyTransformSettingsToGizmo();
		}
		else
		{
			Gizmo->Deactivate();
		}
	}

	UPhysicsAsset* __SyncPA = static_cast<UPhysicsAsset*>(EditedObject);
	SyncPreviewShapeComponents(__SyncPA);
	SyncConstraintConeComponents(__SyncPA);
}

int32 FPhysicsAssetEditorWidget::FindBoneIndexByName(const FSkeletonAsset* SkeletonAsset, const FName& BoneName) const
{
	if (!SkeletonAsset)
	{
		return -1;
	}

	return FindSkeletonBoneIndexByName(SkeletonAsset, BoneName);
}

void FPhysicsAssetEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (bPendingClose)
	{
		Close();
		return;
	}

	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	UPhysicsAsset* PhysicsAsset = static_cast<UPhysicsAsset*>(EditedObject);
	bool bWindowOpen = true;

	FString VisibleTitle = "Physics Asset Editor";
	if (!PhysicsAsset->GetAssetPathFileName().empty())
	{
		VisibleTitle += " - ";
		VisibleTitle += PhysicsAsset->GetAssetPathFileName();
	}
	if (IsDirty())
	{
		VisibleTitle += " *";
	}

	ImGui::SetNextWindowSize(ImVec2(1440.0f, 860.0f), ImGuiCond_Once);
	FString WindowTitle = VisibleTitle + WindowIdSuffix;
	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			bPendingClose = true;
		}
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		FSlateApplication::Get().BringViewportToFront(&ViewportClient);
	}

	if (ImGui::Button("Save"))
	{
		if (FPhysicsAssetManager::Get().Save(PhysicsAsset))
		{
			ClearDirty();
			ToolbarStatusMessage = "PhysicsAsset saved.";
		}
		else
		{
			ToolbarStatusMessage = "PhysicsAsset save failed.";
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Save as Json"))
	{
		if (FPhysicsAssetManager::Get().SaveAsJson(PhysicsAsset))
		{
			ToolbarStatusMessage = "PhysicsAsset JSON saved next to the asset.";
		}
		else
		{
			ToolbarStatusMessage = "PhysicsAsset JSON save failed.";
		}
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%s", PhysicsAsset->GetPreviewSkeletalMeshPath().empty() ? "Preview Mesh: None" : PhysicsAsset->GetPreviewSkeletalMeshPath().c_str());
	if (!ToolbarStatusMessage.empty())
	{
		ImGui::TextWrapped("%s", ToolbarStatusMessage.c_str());
	}

	const ImGuiStyle& Style = ImGui::GetStyle();
	auto CalcButtonWidth = [&](const char* Label) -> float
	{
		return ImGui::CalcTextSize(Label).x + Style.FramePadding.x * 2.0f;
	};
	float SimControlsWidth = 0.0f;
	if (!bPreviewSimulating)
	{
		SimControlsWidth = CalcButtonWidth("Simulate");
	}
	else
	{
		char SimTimeText[32] = {};
		std::snprintf(SimTimeText, sizeof(SimTimeText), "Sim %.2fs", PreviewSimulationTime);
		SimControlsWidth =
			CalcButtonWidth("Stop Sim") +
			Style.ItemSpacing.x +
			CalcButtonWidth("Reset Sim") +
			Style.ItemSpacing.x +
			ImGui::CalcTextSize(SimTimeText).x;
	}

	ImGui::SameLine();
	const float SimControlsStartX = (std::max)(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - SimControlsWidth);
	ImGui::SetCursorPosX(SimControlsStartX);
	if (!bPreviewSimulating)
	{
		if (ImGui::Button("Simulate"))
		{
			StartPreviewSimulation(PhysicsAsset);
		}
	}
	else
	{
		if (ImGui::Button("Stop Sim"))
		{
			StopPreviewSimulation(true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Sim"))
		{
			ResetPreviewSimulation(PhysicsAsset);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("Sim %.2fs", PreviewSimulationTime);
	}
	ImGui::Separator();

	static float LeftPanelWidth = 360.0f;
	static float RightPanelWidth = 360.0f;
	static float GraphHeight = 320.0f;
	static float RightTopPanelHeight = 420.0f;
	constexpr float PhysicsEditorSplitterThickness = 6.0f;
	constexpr float PhysicsEditorMinSidePanelWidth = 220.0f;
	constexpr float PhysicsEditorMinViewportWidth = 320.0f;
	constexpr float PhysicsEditorMinTopPanelHeight = 220.0f;
	constexpr float PhysicsEditorMinBottomPanelHeight = 160.0f;
	constexpr float PhysicsEditorMinBodyTreeHeight = 160.0f;
	constexpr float PhysicsEditorMinGraphHeight = 100.0f;

	bool bChanged = false;
	bool bGraphLayoutChanged = false;
	bool bPreviewSettingsChanged = false;

	const ImVec2 MainContentSize = ImGui::GetContentRegionAvail();
	const float TotalSplitterWidth = PhysicsEditorSplitterThickness * 2.0f;
	const float MaxCombinedSideWidth = (std::max)(0.0f, MainContentSize.x - PhysicsEditorMinViewportWidth - TotalSplitterWidth);
	if (MaxCombinedSideWidth > 0.0f)
	{
		const float CurrentCombinedSideWidth = (std::max)(1.0f, LeftPanelWidth + RightPanelWidth);
		if (LeftPanelWidth + RightPanelWidth > MaxCombinedSideWidth)
		{
			const float Scale = MaxCombinedSideWidth / CurrentCombinedSideWidth;
			LeftPanelWidth *= Scale;
			RightPanelWidth *= Scale;
		}

		const float MinSidePanelWidth = (std::min)(PhysicsEditorMinSidePanelWidth, (std::max)(120.0f, MaxCombinedSideWidth * 0.5f));
		if (MaxCombinedSideWidth < MinSidePanelWidth * 2.0f)
		{
			LeftPanelWidth = MaxCombinedSideWidth * 0.5f;
			RightPanelWidth = MaxCombinedSideWidth - LeftPanelWidth;
		}
		else
		{
			LeftPanelWidth = (std::clamp)(LeftPanelWidth, MinSidePanelWidth, MaxCombinedSideWidth - MinSidePanelWidth);
			RightPanelWidth = (std::clamp)(RightPanelWidth, MinSidePanelWidth, MaxCombinedSideWidth - LeftPanelWidth);
			if (LeftPanelWidth + RightPanelWidth > MaxCombinedSideWidth)
			{
				RightPanelWidth = MaxCombinedSideWidth - LeftPanelWidth;
			}
		}
	}
	const float ViewportWidth = (std::max)(PhysicsEditorMinViewportWidth, MainContentSize.x - LeftPanelWidth - RightPanelWidth - TotalSplitterWidth);

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

	ImGui::BeginChild("PhysicsAssetLeftPanel", ImVec2(LeftPanelWidth, MainContentSize.y), true);
	{
		const float LeftPanelHeight = ImGui::GetContentRegionAvail().y;
		const float MaxGraphHeight = (std::max)(PhysicsEditorMinGraphHeight, LeftPanelHeight - PhysicsEditorSplitterThickness - PhysicsEditorMinBodyTreeHeight);
		GraphHeight = (std::clamp)(GraphHeight, PhysicsEditorMinGraphHeight, MaxGraphHeight);
		const float SkeletonHeight = LeftPanelHeight - GraphHeight - PhysicsEditorSplitterThickness;

		const ImGuiWindowFlags SkeletonTreeFlags = ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_HorizontalScrollbar;
		ImGui::PushStyleColor(ImGuiCol_ChildBg, PhysicsPanelBodyColor);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, PhysicsPanelContentPadding);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, PhysicsPanelItemSpacing);
		ImGui::BeginChild("PhysicsSkeletonTree", ImVec2(0.0f, SkeletonHeight), true, SkeletonTreeFlags);
		DrawPhysicsPanelHeader("Skeletal Tree");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##PhysicsBoneSearch", "Search Skeleton Tree...", BoneSearchBuffer, sizeof(BoneSearchBuffer));
		if (ImGui::Checkbox("Show Only Bones With Bodies", &bShowBonesWithBodiesOnly))
		{
			bExpandBodyTreeOnNextRender = true;
		}
		ImGui::Separator();

		if (PreviewSkeletalMesh)
		{
			if (const FSkeletonAsset* SkeletonAsset = PreviewSkeletalMesh->GetSkeletonAsset())
			{
				const FString SearchText = BoneSearchBuffer;
				const UPhysicsAsset* TreePhysicsAsset = static_cast<UPhysicsAsset*>(EditedObject);
				BoneTreeRowCounter = 0;
				TArray<int32> RootBoneIndices;
				GatherDisplayedChildBones(
					SkeletonAsset,
					TreePhysicsAsset,
					-1,
					SearchText,
					bShowBonesWithBodiesOnly,
					RootBoneIndices);
				for (int32 BoneIndex : RootBoneIndices)
				{
					RenderSkeletonTree(SkeletonAsset, BoneIndex, SearchText);
				}
				bExpandBodyTreeOnNextRender = false;
			}
		}
		else
		{
			ImGui::TextDisabled("No preview skeletal mesh is associated with this Physics Asset.");
		}
		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();

		const ImVec2 LeftHSplitterPos = ImGui::GetCursorScreenPos();
		ImRect LeftHSplitterRect(
			LeftHSplitterPos,
			ImVec2(LeftHSplitterPos.x + ImGui::GetContentRegionAvail().x, LeftHSplitterPos.y + PhysicsEditorSplitterThickness));
		float SplitTopHeight = SkeletonHeight;
		float SplitBottomHeight = GraphHeight;
		if (ImGui::SplitterBehavior(
				LeftHSplitterRect,
				ImGui::GetID("##PhysicsEditorLeftHorizontalSplitter"),
				ImGuiAxis_Y,
				&SplitTopHeight,
				&SplitBottomHeight,
				PhysicsEditorMinBodyTreeHeight,
				PhysicsEditorMinGraphHeight))
		{
			GraphHeight = SplitBottomHeight;
		}
		ImGui::Dummy(ImVec2(0.0f, PhysicsEditorSplitterThickness));

		ImGui::PushStyleColor(ImGuiCol_ChildBg, PhysicsPanelBodyColor);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, PhysicsPanelContentPadding);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, PhysicsPanelItemSpacing);
		ImGui::BeginChild("PhysicsGraphPanel", ImVec2(0.0f, 0.0f), true);
		ImGui::TextUnformatted("Graph");
		ImGui::Separator();
		bGraphLayoutChanged = RenderGraphPanel(PhysicsAsset);
		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();
	}
	ImGui::EndChild();

	ImGui::SameLine();
	{
		const float MinSidePanelWidth = (std::min)(PhysicsEditorMinSidePanelWidth, (std::max)(120.0f, MaxCombinedSideWidth * 0.5f));
		const ImVec2 SplitterPos = ImGui::GetCursorScreenPos();
		ImRect SplitterRect(
			SplitterPos,
			ImVec2(SplitterPos.x + PhysicsEditorSplitterThickness, SplitterPos.y + MainContentSize.y));
		float LeftWidth = LeftPanelWidth;
		float RemainingWidth = MainContentSize.x - LeftPanelWidth - PhysicsEditorSplitterThickness;
		if (ImGui::SplitterBehavior(
				SplitterRect,
				ImGui::GetID("##PhysicsEditorLeftSplitter"),
				ImGuiAxis_X,
				&LeftWidth,
				&RemainingWidth,
				MinSidePanelWidth,
				PhysicsEditorMinViewportWidth + RightPanelWidth + PhysicsEditorSplitterThickness))
		{
			LeftPanelWidth = LeftWidth;
		}
		ImGui::Dummy(ImVec2(PhysicsEditorSplitterThickness, MainContentSize.y));
	}

	ImGui::SameLine();
	ImGui::BeginChild("PhysicsAssetViewportColumn", ImVec2(ViewportWidth, MainContentSize.y), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	{
		const ImVec2 ViewportSize(ViewportWidth, ImGui::GetContentRegionAvail().y);
		const ImVec2 ViewportPos = ImGui::GetCursorScreenPos();

		if (ViewportClient.IsRenderable())
		{
			constexpr float ToolbarHeight = PhysicsViewportToolbarHeight;
			ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, ViewportSize.x, ViewportSize.y);
			if (FViewport* Viewport = ViewportClient.GetViewport())
			{
				const uint32 NewW = static_cast<uint32>(ViewportSize.x);
				const uint32 NewH = static_cast<uint32>(ViewportSize.y);
				if (Viewport->GetWidth() != NewW || Viewport->GetHeight() != NewH)
				{
					Viewport->RequestResize(NewW, NewH);
					ViewportClient.NotifyViewportResized(static_cast<int32>(NewW), static_cast<int32>(NewH - static_cast<uint32>(ToolbarHeight)));
				}
				PhysicsAssetViewportWindow.SetRect(FRect(ViewportPos.x, ViewportPos.y, ViewportSize.x, ViewportSize.y));

				if (Viewport->GetSRV())
				{
					ImGui::Image((ImTextureID)Viewport->GetSRV(), ViewportSize);
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					const ImVec2 MousePos = ImGui::GetMousePos();
					if (MousePos.y > ViewportPos.y + ToolbarHeight)
					{
						FMinimalViewInfo POV;
						if (ViewportClient.GetCameraView(POV))
						{
							const float LocalMouseX = MousePos.x - ViewportPos.x;
							const float LocalMouseY = MousePos.y - ViewportPos.y;
							FRay Ray = POV.DeprojectScreenToWorld(LocalMouseX, LocalMouseY, ViewportSize.x, ViewportSize.y);
							FHitResult GizmoHit;
							if (!ViewportClient.GetGizmo() || !FRayUtils::RaycastComponent(ViewportClient.GetGizmo(), Ray, GizmoHit))
							{
								// Constraint previews are rendered with depth disabled so the visually topmost
								// fan/cone is determined by draw order, not world depth. Pick using the last
								// hit preview entry so selection matches what the user actually sees.
								int32 BestConstraintIndex = -1;
								int32 BestConstraintDrawOrder = -1;
								float BestConstraintDistance = FLT_MAX;
								for (int32 EntryOrder = 0; EntryOrder < static_cast<int32>(PreviewConstraintCones.size()); ++EntryOrder)
								{
									const FPreviewConstraintConeEntry& Entry = PreviewConstraintCones[EntryOrder];
									if (!Entry.Component || !bShowConstraintDebug)
									{
										continue;
									}

									FHitResult ConstraintHit;
									if (FRayUtils::RaycastComponent(Entry.Component, Ray, ConstraintHit)
										&& (EntryOrder > BestConstraintDrawOrder
											|| (EntryOrder == BestConstraintDrawOrder && ConstraintHit.Distance < BestConstraintDistance)))
									{
										BestConstraintDrawOrder = EntryOrder;
										BestConstraintDistance = ConstraintHit.Distance;
										BestConstraintIndex = Entry.ConstraintIndex;
									}
								}

								// Fallback for the blue twist arc / axis lines, which are debug lines rather than
								// static mesh components. Keep the radius small so fan picking stays faithful.
								if (BestConstraintIndex < 0)
								{
									const FSkeletonAsset* Skel = PreviewSkeletalMesh ? PreviewSkeletalMesh->GetSkeletonAsset() : nullptr;
									USkeletalMeshComponent* MC = ViewportClient.GetPreviewMeshComponent();
									if (Skel && MC)
									{
										TArray<FMatrix> BoneMatrices;
										MC->GetCurrentBoneGlobalMatrices(BoneMatrices);
										const float PickRadius = (std::max)(ConstraintAxisLength * 0.25f, 0.01f);

										for (int32 Ci = 0; Ci < static_cast<int32>(PhysicsAsset->GetConstraintSetups().size()); ++Ci)
										{
											const FPhysicsConstraintSetup& CS = PhysicsAsset->GetConstraintSetups()[Ci];
											const int32 PBoneIdx = FindBoneIndexByName(Skel, CS.ParentBoneName);
											const int32 CBoneIdx = FindBoneIndexByName(Skel, CS.ChildBoneName);
											if (PBoneIdx < 0 || CBoneIdx < 0) continue;

											const FTransform PFrame(CS.ParentLocalFrame.ToMatrix() * BuildBoneWorldMatrix(MC, &BoneMatrices, PBoneIdx));
											const FTransform CFrame(CS.ChildLocalFrame.ToMatrix()  * BuildBoneWorldMatrix(MC, &BoneMatrices, CBoneIdx));
											const FVector ConstraintOrigin = (PFrame.Location + CFrame.Location) * 0.5f;
											const FVector ToOrigin = ConstraintOrigin - Ray.Origin;
											const float RayT = ToOrigin.Dot(Ray.Direction);
											if (RayT <= 0.0f) continue;

											const FVector ClosestOnRay = Ray.Origin + Ray.Direction * RayT;
											if (FVector::Distance(ConstraintOrigin, ClosestOnRay) < PickRadius && RayT < BestConstraintDistance)
											{
												BestConstraintDistance = RayT;
												BestConstraintIndex = Ci;
											}
										}
									}
								}

								if (BestConstraintIndex >= 0)
								{
									SelectConstraintByIndex(BestConstraintIndex);
								}
								else
								{
									const FPreviewShapeComponentEntry* BestEntry = nullptr;
									float BestDistance = FLT_MAX;
									for (const FPreviewShapeComponentEntry& Entry : PreviewShapeComponents)
									{
										if (!Entry.Component)
										{
											continue;
										}

										FHitResult ShapeHit;
										if (FRayUtils::RaycastComponent(Entry.Component, Ray, ShapeHit) && ShapeHit.Distance < BestDistance)
										{
											BestDistance = ShapeHit.Distance;
											BestEntry = &Entry;
										}
									}

									if (BestEntry)
										SelectBodyShape(BestEntry->BodyIndex, BestEntry->ShapeType, BestEntry->ShapeIndex);
									else
										ClearSelection();
								}
							}
						}
					}
				}

				ImDrawList* DrawList = ImGui::GetWindowDrawList();
				DrawList->AddRectFilled(ViewportPos, ImVec2(ViewportPos.x + ViewportSize.x, ViewportPos.y + ToolbarHeight), IM_COL32(40, 40, 40, 255));

				FViewportToolbarContext Context;
				Context.Renderer = &GEngine->GetRenderer();
				Context.Gizmo = ViewportClient.GetGizmo();
				Context.Settings = &FEditorSettings::Get().MeshEditorViewportSettings;
				Context.RenderOptions = &ViewportClient.GetRenderOptions();
				Context.ToolbarLeft = ViewportPos.x;
				Context.ToolbarTop = ViewportPos.y;
				Context.ToolbarWidth = ViewportSize.x;
				Context.ToolbarHeight = ToolbarHeight;
				Context.bReservePlayStopSpace = false;
				Context.bShowAddActor = false;
				Context.OnCoordSystemToggled = [&]()
				{
					FGizmoToolSettings& Settings = FEditorSettings::Get().MeshEditorViewportSettings.Gizmo;
					Settings.CoordSystem = (Settings.CoordSystem == EEditorCoordSystem::World) ? EEditorCoordSystem::Local : EEditorCoordSystem::World;
					ViewportClient.ApplyTransformSettingsToGizmo();
				};
				Context.OnSettingsChanged = [&]()
				{
					ViewportClient.ApplyTransformSettingsToGizmo();
				};
				Context.OnRenderViewModeExtras = [&]()
				{
					ImGui::TextUnformatted("Physics Shapes");
					ImGui::TextDisabled("White translucent bodies follow the current bone pose.");
					if (SelectedBodyIndex >= 0 && SelectedBodyIndex < static_cast<int32>(PhysicsAsset->GetBodySetups().size()))
					{
						if (const UPhysicsBodySetup* BodySetup = PhysicsAsset->GetBodySetups()[SelectedBodyIndex])
						{
							ImGui::Text("Selected Body: %s", BodySetup->GetTargetBoneName().ToString().c_str());
						}
					}
					ImGui::Separator();
					bPreviewSettingsChanged |= RenderPreviewSettingsPanel(false);
				};

				FViewportToolbar::Render(Context);
				RenderSelectedConstraintLimitOverlay(PhysicsAsset, DrawList, ViewportPos, ViewportSize, ToolbarHeight);
				RenderStatsOverlay(
					DrawList,
					ViewportPos,
					PhysicsAsset,
					PreviewSkeletalMesh,
					ViewportClient.GetPreviewWorld(),
					bPreviewSimulating);
				if (bPreviewSimulating)
				{
					const FString SimText = "Simulating Ragdoll";
					const float SimTextWidth = ImGui::CalcTextSize(SimText.c_str()).x;
					const ImVec2 SimTextPos(
						ViewportPos.x + ViewportSize.x - SimTextWidth - 8.0f,
						ViewportPos.y + ToolbarHeight + 8.0f);
					DrawList->AddText(ImVec2(SimTextPos.x + 1.0f, SimTextPos.y + 1.0f), IM_COL32(0, 0, 0, 220), SimText.c_str());
					DrawList->AddText(SimTextPos, IM_COL32(120, 230, 255, 255), SimText.c_str());
				}
			}
		}
		else
		{
			ImGui::BeginChild("PhysicsViewportPlaceholder", ViewportSize, true);
			ImGui::TextDisabled("Preview viewport unavailable. Reopen the asset after assigning a preview skeletal mesh.");
			ImGui::EndChild();
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();
	{
		const float MinSidePanelWidth = (std::min)(PhysicsEditorMinSidePanelWidth, (std::max)(120.0f, MaxCombinedSideWidth * 0.5f));
		const ImVec2 SplitterPos = ImGui::GetCursorScreenPos();
		ImRect SplitterRect(
			SplitterPos,
			ImVec2(SplitterPos.x + PhysicsEditorSplitterThickness, SplitterPos.y + MainContentSize.y));
		float LeftRegionWidth = MainContentSize.x - RightPanelWidth - PhysicsEditorSplitterThickness;
		float RightWidth = RightPanelWidth;
		if (ImGui::SplitterBehavior(
				SplitterRect,
				ImGui::GetID("##PhysicsEditorRightSplitter"),
				ImGuiAxis_X,
				&LeftRegionWidth,
				&RightWidth,
				LeftPanelWidth + PhysicsEditorSplitterThickness + PhysicsEditorMinViewportWidth,
				MinSidePanelWidth))
		{
			RightPanelWidth = RightWidth;
		}
		ImGui::Dummy(ImVec2(PhysicsEditorSplitterThickness, MainContentSize.y));
	}

	ImGui::SameLine();
	ImGui::BeginChild("PhysicsAssetRightPanel", ImVec2(RightPanelWidth, MainContentSize.y), true);
	{
		const float RightPanelHeight = ImGui::GetContentRegionAvail().y;
		const float MaxTopPanelHeight = (std::max)(PhysicsEditorMinTopPanelHeight, RightPanelHeight - PhysicsEditorSplitterThickness - PhysicsEditorMinBottomPanelHeight);
		RightTopPanelHeight = (std::clamp)(RightTopPanelHeight, PhysicsEditorMinTopPanelHeight, MaxTopPanelHeight);

		ImGui::PushStyleColor(ImGuiCol_ChildBg, PhysicsPanelBodyColor);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, PhysicsPanelContentPadding);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, PhysicsPanelItemSpacing);
		ImGui::BeginChild("PhysicsDetailsPanel", ImVec2(0.0f, RightTopPanelHeight), true);
		bChanged |= RenderDetailsPanel(PhysicsAsset);
		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();

		const ImVec2 HorizontalSplitterPos = ImGui::GetCursorScreenPos();
		ImRect HorizontalSplitterRect(
			HorizontalSplitterPos,
			ImVec2(HorizontalSplitterPos.x + ImGui::GetContentRegionAvail().x, HorizontalSplitterPos.y + PhysicsEditorSplitterThickness));
		float TopHeight = RightTopPanelHeight;
		float BottomHeight = RightPanelHeight - RightTopPanelHeight - PhysicsEditorSplitterThickness;
		if (ImGui::SplitterBehavior(
				HorizontalSplitterRect,
				ImGui::GetID("##PhysicsEditorRightHorizontalSplitter"),
				ImGuiAxis_Y,
				&TopHeight,
				&BottomHeight,
				PhysicsEditorMinTopPanelHeight,
				PhysicsEditorMinBottomPanelHeight))
		{
			RightTopPanelHeight = TopHeight;
		}
		ImGui::Dummy(ImVec2(0.0f, PhysicsEditorSplitterThickness));

		ImGui::PushStyleColor(ImGuiCol_ChildBg, PhysicsPanelBodyColor);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, PhysicsToolsPanelContentPadding);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, PhysicsToolsPanelItemSpacing);
		ImGui::BeginChild("PhysicsToolsPanel", ImVec2(0.0f, 0.0f), true);
		bChanged |= RenderToolsPanel(PhysicsAsset);
		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();
	}
	ImGui::EndChild();

	ImGui::PopStyleVar();

	if (bChanged)
	{
		if (bPreviewSimulating)
		{
			StopPreviewSimulation(true);
		}
		const bool bEditingConstraint = SelectedConstraintIndex >= 0
			&& SelectedConstraintIndex < static_cast<int32>(PhysicsAsset->GetConstraintSetups().size());
		if (!bEditingConstraint && !bConstraintPreviewDefinitionDirty && !bConstraintPreviewTransformDirty)
		{
			RebuildPreviewShapeComponents(PhysicsAsset);
		}
		MarkDirty();
	}
	else if (bPreviewSettingsChanged)
	{
		SyncPreviewShapeComponents(PhysicsAsset);
		SyncConstraintConeComponents(PhysicsAsset);
	}
	else if (bGraphLayoutChanged)
	{
		MarkDirty();
	}

	ImGui::End();

	if (!bWindowOpen)
	{
		bPendingClose = true;
	}
}

void FPhysicsAssetEditorWidget::RenderSkeletonTree(const FSkeletonAsset* SkeletonAsset, int32 BoneIndex, const FString& SearchText)
{
	if (!SkeletonAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(SkeletonAsset->Bones.size()))
	{
		return;
	}

	const FBone& Bone = SkeletonAsset->Bones[BoneIndex];
	const UPhysicsAsset* PhysicsAsset = static_cast<UPhysicsAsset*>(EditedObject);
	const int32 BodyIndex = PhysicsAsset ? FindBodySetupIndex(PhysicsAsset, FName(Bone.Name)) : -1;
	if (!BoneMatchesTreeFilterRecursive(SkeletonAsset, PhysicsAsset, BoneIndex, SearchText, bShowBonesWithBodiesOnly))
	{
		return;
	}

	TArray<int32> DisplayedChildBones;
	GatherDisplayedChildBones(
		SkeletonAsset,
		PhysicsAsset,
		BoneIndex,
		SearchText,
		bShowBonesWithBodiesOnly,
		DisplayedChildBones);
	const bool bHasChildren = !DisplayedChildBones.empty();

	// --- Row background (alternating + selection highlight) ---
	const bool bIsSelected = (BodyIndex >= 0 && BodyIndex == SelectedBodyIndex);
	const int32 CurrentRow = BoneTreeRowCounter++;

	{
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImVec2 RowMin = ImGui::GetCursorScreenPos();
		const float RowHeight = ImGui::GetFrameHeight();
		const ImVec2 WindowPos = ImGui::GetWindowPos();
		const float WindowWidth = ImGui::GetWindowSize().x;

		ImU32 BgColor;
		if (bIsSelected)
			BgColor = IM_COL32(50, 110, 210, 210);
		else if (CurrentRow % 2 == 0)
			BgColor = IM_COL32(62, 62, 62, 255);  // 밝은 회색
		else
			BgColor = IM_COL32(54, 54, 54, 255);  // 어두운 회색

		DrawList->AddRectFilled(
			ImVec2(WindowPos.x, RowMin.y),
			ImVec2(WindowPos.x + WindowWidth, RowMin.y + RowHeight),
			BgColor
		);
	}

	// --- Tree node flags ---
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (!bHasChildren)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}
	else if (bExpandBodyTreeOnNextRender || !SearchText.empty())
	{
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
	}

	// Header 색상을 투명하게 해서 우리가 그린 배경이 보이도록
	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(1.0f, 1.0f, 1.0f, 0.12f));

	const FString Label = Bone.Name;
	const bool bOpen = ImGui::TreeNodeEx(Label.c_str(), Flags);

	ImGui::PopStyleColor(3);

	if (ImGui::IsItemClicked() && BodyIndex >= 0)
	{
		SelectBodyByIndex(BodyIndex);
	}

	// --- 텍스트 바로 우측에 캡슐 아이콘 (body가 있는 경우) ---
	if (BodyIndex >= 0 && CapsuleBodyIcon)
	{
		const float IconSize = 14.0f;
		const ImVec2 ItemMin = ImGui::GetItemRectMin();
		const ImVec2 ItemMax = ImGui::GetItemRectMax();

		// 트리 노드 화살표/들여쓰기 이후 텍스트 시작 X
		const float TextStartX = ItemMin.x + ImGui::GetTreeNodeToLabelSpacing();
		const float TextWidth = ImGui::CalcTextSize(Label.c_str()).x;
		const float IconX = TextStartX + TextWidth + 6.0f;
		const float IconY = ItemMin.y + (ItemMax.y - ItemMin.y - IconSize) * 0.5f;

		ImGui::GetWindowDrawList()->AddImage(
			(ImTextureID)CapsuleBodyIcon,
			ImVec2(IconX, IconY),
			ImVec2(IconX + IconSize, IconY + IconSize)
		);
	}

	if (bOpen && bHasChildren)
	{
		for (int32 ChildBoneIndex : DisplayedChildBones)
		{
			RenderSkeletonTree(SkeletonAsset, ChildBoneIndex, SearchText);
		}
		ImGui::TreePop();
	}
}

bool FPhysicsAssetEditorWidget::RenderGraphPanel(UPhysicsAsset* PhysicsAsset)
{
	if (!PhysicsAsset)
	{
		return false;
	}

	FPhysicsAssetGraphViewState& GraphViewState = PhysicsAsset->GetMutableGraphViewState();
	bool bLayoutChanged = false;
	const float ClampedZoom = (std::max)(PhysicsGraphMinZoom, (std::min)(GraphViewState.Zoom, PhysicsGraphMaxZoom));
	if (std::fabs(ClampedZoom - GraphViewState.Zoom) > 1.0e-4f)
	{
		GraphViewState.Zoom = ClampedZoom;
		bLayoutChanged = true;
	}

	bool bRequestResetView = false;
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, PhysicsGraphControlItemSpacing);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, PhysicsGraphControlFramePadding);
	if (ImGui::Button("Reset View"))
	{
		bRequestResetView = true;
	}
	ImGui::SameLine();
	bool bRequestAutoArrange = false;
	if (ImGui::Button("Auto Arrange"))
	{
		GraphViewState.NodeLayouts.clear();
		bRequestAutoArrange = true;
		bLayoutChanged = true;
	}
	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("LMB drag node | Wheel zoom | MMB/RMB pan");
	ImGui::PopStyleVar(2);

	const TArray<FPhysicsConstraintSetup>& ConstraintSetups = PhysicsAsset->GetConstraintSetups();
	const TArray<UPhysicsBodySetup*>& BodySetups = PhysicsAsset->GetBodySetups();
	if (ConstraintSetups.empty() && BodySetups.empty())
	{
		ImGui::TextDisabled("No generated bodies or constraints.");
		return bLayoutChanged;
	}

	const FSkeletonAsset* SkeletonAsset = PreviewSkeletalMesh ? PreviewSkeletalMesh->GetSkeletonAsset() : nullptr;
	TArray<int32> OrderedBodyIndices;
	OrderedBodyIndices.reserve(BodySetups.size());
	for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(BodySetups.size()); ++BodyIndex)
	{
		if (BodySetups[BodyIndex])
		{
			OrderedBodyIndices.push_back(BodyIndex);
		}
	}
	std::sort(OrderedBodyIndices.begin(), OrderedBodyIndices.end(),
		[&](int32 LeftBodyIndex, int32 RightBodyIndex)
		{
			const UPhysicsBodySetup* LeftBodySetup = BodySetups[LeftBodyIndex];
			const UPhysicsBodySetup* RightBodySetup = BodySetups[RightBodyIndex];
			const int32 LeftBoneIndex = FindSkeletonBoneIndexByName(SkeletonAsset, LeftBodySetup ? LeftBodySetup->GetTargetBoneName() : FName::None);
			const int32 RightBoneIndex = FindSkeletonBoneIndexByName(SkeletonAsset, RightBodySetup ? RightBodySetup->GetTargetBoneName() : FName::None);
			const int32 LeftDepth = ComputeBoneDepth(SkeletonAsset, LeftBoneIndex);
			const int32 RightDepth = ComputeBoneDepth(SkeletonAsset, RightBoneIndex);
			if (LeftDepth != RightDepth)
			{
				return LeftDepth < RightDepth;
			}
			if (LeftBoneIndex >= 0 && RightBoneIndex >= 0 && LeftBoneIndex != RightBoneIndex)
			{
				return LeftBoneIndex < RightBoneIndex;
			}
			return LeftBodyIndex < RightBodyIndex;
		});

	TArray<FVector2> DefaultBodyPositions;
	DefaultBodyPositions.resize(BodySetups.size(), FVector2(0.0f, 0.0f));
	for (int32 OrderedIndex = 0; OrderedIndex < static_cast<int32>(OrderedBodyIndices.size()); ++OrderedIndex)
	{
		const int32 BodyIndex = OrderedBodyIndices[OrderedIndex];
		const UPhysicsBodySetup* BodySetup = BodySetups[BodyIndex];
		const int32 BoneIndex = FindSkeletonBoneIndexByName(SkeletonAsset, BodySetup ? BodySetup->GetTargetBoneName() : FName::None);
		const int32 Depth = ComputeBoneDepth(SkeletonAsset, BoneIndex);
		DefaultBodyPositions[BodyIndex] = FVector2(
			Depth * PhysicsGraphBodyColumnWidth,
			OrderedIndex * PhysicsGraphRowHeight);
	}

	TArray<FPhysicsGraphNodeVisual> GraphNodes;
	GraphNodes.reserve(BodySetups.size() + ConstraintSetups.size());
	TMap<int32, int32> BodyNodeByBodyIndex;
	TMap<int32, int32> ConstraintNodeByConstraintIndex;

	for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(BodySetups.size()); ++BodyIndex)
	{
		UPhysicsBodySetup* BodySetup = BodySetups[BodyIndex];
		if (!BodySetup)
		{
			continue;
		}

		const FVector2 DefaultPosition =
			BodyIndex < static_cast<int32>(DefaultBodyPositions.size())
			? DefaultBodyPositions[BodyIndex]
			: FVector2(0.0f, BodyIndex * PhysicsGraphRowHeight);
		const FString NodeKey = MakeBodyGraphNodeKey(BodySetup);
		const int32 LayoutIndex = GetOrCreateGraphNodeLayoutIndex(PhysicsAsset, NodeKey, DefaultPosition);
		const FPhysicsAssetGraphNodeLayout& Layout = GraphViewState.NodeLayouts[LayoutIndex];

		GraphNodes.emplace_back();
		FPhysicsGraphNodeVisual& Node = GraphNodes.back();
		Node.NodeKey = NodeKey;
		Node.Title = "Body";
		Node.Subtitle = BodySetup->GetTargetBoneName().ToString();
		Node.Detail = std::to_string(CountBodyShapes(BodySetup)) + " shape(s)";
		Node.Position = Layout.Position;
		Node.Size = ImVec2(PhysicsGraphBodyNodeWidth, PhysicsGraphBodyNodeHeight);
		Node.LayoutIndex = LayoutIndex;
		Node.DataIndex = BodyIndex;
		Node.NodeKind = EPhysicsGraphNodeKind::Body;
		Node.bSelected = BodyIndex == SelectedBodyIndex;

		BodyNodeByBodyIndex[BodyIndex] = static_cast<int32>(GraphNodes.size()) - 1;
	}

	TMap<FString, int32> ConstraintOffsetByPairKey;
	for (int32 ConstraintIndex = 0; ConstraintIndex < static_cast<int32>(ConstraintSetups.size()); ++ConstraintIndex)
	{
		const FPhysicsConstraintSetup& ConstraintSetup = ConstraintSetups[ConstraintIndex];
		const int32 ParentBodyIndex = FindBodySetupIndex(PhysicsAsset, ConstraintSetup.ParentBoneName);
		const int32 ChildBodyIndex = FindBodySetupIndex(PhysicsAsset, ConstraintSetup.ChildBoneName);
		const FVector2 ParentPosition =
			(ParentBodyIndex >= 0 && ParentBodyIndex < static_cast<int32>(DefaultBodyPositions.size()))
			? DefaultBodyPositions[ParentBodyIndex]
			: FVector2(0.0f, ConstraintIndex * PhysicsGraphRowHeight);
		const FVector2 ChildPosition =
			(ChildBodyIndex >= 0 && ChildBodyIndex < static_cast<int32>(DefaultBodyPositions.size()))
			? DefaultBodyPositions[ChildBodyIndex]
			: FVector2(ParentPosition.X + PhysicsGraphBodyColumnWidth, ParentPosition.Y);

		const FString PairKey = ConstraintSetup.ParentBoneName.ToString() + "->" + ConstraintSetup.ChildBoneName.ToString();
		int32 PairOffset = 0;
		auto OffsetIt = ConstraintOffsetByPairKey.find(PairKey);
		if (OffsetIt != ConstraintOffsetByPairKey.end())
		{
			PairOffset = OffsetIt->second;
			OffsetIt->second = PairOffset + 1;
		}
		else
		{
			ConstraintOffsetByPairKey[PairKey] = 1;
		}

		const FVector2 DefaultPosition(
			(ParentPosition.X + ChildPosition.X) * 0.5f,
			(ParentPosition.Y + ChildPosition.Y) * 0.5f + PairOffset * 72.0f);
		const FString NodeKey = MakeConstraintGraphNodeKey(ConstraintSetup);
		const int32 LayoutIndex = GetOrCreateGraphNodeLayoutIndex(PhysicsAsset, NodeKey, DefaultPosition);
		const FPhysicsAssetGraphNodeLayout& Layout = GraphViewState.NodeLayouts[LayoutIndex];

		GraphNodes.emplace_back();
		FPhysicsGraphNodeVisual& Node = GraphNodes.back();
		Node.NodeKey = NodeKey;
		Node.Title = "Constraint";
		Node.Subtitle = ConstraintSetup.ChildBoneName.ToString() + " : " + ConstraintSetup.ParentBoneName.ToString();
		Node.Detail.clear();
		Node.Position = Layout.Position;
		Node.Size = ImVec2(PhysicsGraphConstraintNodeWidth, PhysicsGraphConstraintNodeHeight);
		Node.LayoutIndex = LayoutIndex;
		Node.DataIndex = ConstraintIndex;
		Node.NodeKind = EPhysicsGraphNodeKind::Constraint;
		Node.bSelected = ConstraintIndex == SelectedConstraintIndex;

		ConstraintNodeByConstraintIndex[ConstraintIndex] = static_cast<int32>(GraphNodes.size()) - 1;
	}

	ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
	ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
	CanvasSize.x = (std::max)(CanvasSize.x, 240.0f);
	CanvasSize.y = (std::max)(CanvasSize.y, 180.0f);
	const bool bApplyInitialFrame = bFrameGraphOnNextRender;
	if (bRequestResetView || bRequestAutoArrange || bApplyInitialFrame)
	{
		FramePhysicsGraphNodes(GraphNodes, CanvasSize, GraphViewState);
		bFrameGraphOnNextRender = false;
		if (bRequestResetView || bRequestAutoArrange)
		{
			bLayoutChanged = true;
		}
	}

	if (bScrollToSelectedConstraintOnNextRender && SelectedConstraintIndex >= 0)
	{
		bScrollToSelectedConstraintOnNextRender = false;
		for (const FPhysicsGraphNodeVisual& Node : GraphNodes)
		{
			if (Node.NodeKind == EPhysicsGraphNodeKind::Constraint && Node.DataIndex == SelectedConstraintIndex)
			{
				const float NodeCenterX = Node.Position.X + Node.Size.x * 0.5f;
				const float NodeCenterY = Node.Position.Y + Node.Size.y * 0.5f;
				GraphViewState.Pan.X = CanvasSize.x * 0.5f - NodeCenterX * GraphViewState.Zoom;
				GraphViewState.Pan.Y = CanvasSize.y * 0.5f - NodeCenterY * GraphViewState.Zoom;
				bLayoutChanged = true;
				break;
			}
		}
	}
	const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);
	ImGui::InvisibleButton("##PhysicsGraphCanvas", CanvasSize, ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
	const bool bCanvasHovered = ImGui::IsItemHovered();
	const ImGuiIO& IO = ImGui::GetIO();
	if (bCanvasHovered && IO.MouseWheel != 0.0f && !IO.WantTextInput)
	{
		float MouseGraphX = 0.0f;
		float MouseGraphY = 0.0f;
		ScreenToPhysicsGraph(IO.MousePos, CanvasMin, GraphViewState, MouseGraphX, MouseGraphY);

		GraphViewState.Zoom = (std::max)(PhysicsGraphMinZoom, (std::min)(GraphViewState.Zoom * std::pow(1.12f, IO.MouseWheel), PhysicsGraphMaxZoom));
		GraphViewState.Pan.X = IO.MousePos.x - CanvasMin.x - MouseGraphX * GraphViewState.Zoom;
		GraphViewState.Pan.Y = IO.MousePos.y - CanvasMin.y - MouseGraphY * GraphViewState.Zoom;
		bLayoutChanged = true;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(CanvasMin, CanvasMax, true);
	DrawPhysicsGraphGrid(CanvasMin, CanvasMax, GraphViewState, DrawList);

	auto GetNodeMin = [&](const FPhysicsGraphNodeVisual& Node)
	{
		return PhysicsGraphToScreen(Node.Position, CanvasMin, GraphViewState);
	};
	auto GetNodeMax = [&](const FPhysicsGraphNodeVisual& Node)
	{
		const ImVec2 NodeMin = GetNodeMin(Node);
		return ImVec2(NodeMin.x + Node.Size.x * GraphViewState.Zoom, NodeMin.y + Node.Size.y * GraphViewState.Zoom);
	};
	auto GetInputPin = [&](const FPhysicsGraphNodeVisual& Node)
	{
		const ImVec2 NodeMin = GetNodeMin(Node);
		const ImVec2 NodeMax = GetNodeMax(Node);
		return ImVec2(NodeMin.x, (NodeMin.y + NodeMax.y) * 0.5f);
	};
	auto GetOutputPin = [&](const FPhysicsGraphNodeVisual& Node)
	{
		const ImVec2 NodeMin = GetNodeMin(Node);
		const ImVec2 NodeMax = GetNodeMax(Node);
		return ImVec2(NodeMax.x, (NodeMin.y + NodeMax.y) * 0.5f);
	};

	for (int32 ConstraintIndex = 0; ConstraintIndex < static_cast<int32>(ConstraintSetups.size()); ++ConstraintIndex)
	{
		const auto ConstraintNodeIt = ConstraintNodeByConstraintIndex.find(ConstraintIndex);
		if (ConstraintNodeIt == ConstraintNodeByConstraintIndex.end())
		{
			continue;
		}

		const FPhysicsConstraintSetup& ConstraintSetup = ConstraintSetups[ConstraintIndex];
		const int32 ParentBodyIndex = FindBodySetupIndex(PhysicsAsset, ConstraintSetup.ParentBoneName);
		const int32 ChildBodyIndex = FindBodySetupIndex(PhysicsAsset, ConstraintSetup.ChildBoneName);
		const auto ParentBodyNodeIt = BodyNodeByBodyIndex.find(ParentBodyIndex);
		const auto ChildBodyNodeIt = BodyNodeByBodyIndex.find(ChildBodyIndex);
		if (ParentBodyNodeIt == BodyNodeByBodyIndex.end() || ChildBodyNodeIt == BodyNodeByBodyIndex.end())
		{
			continue;
		}

		const FPhysicsGraphNodeVisual& ParentNode = GraphNodes[ParentBodyNodeIt->second];
		const FPhysicsGraphNodeVisual& ConstraintNode = GraphNodes[ConstraintNodeIt->second];
		const FPhysicsGraphNodeVisual& ChildNode = GraphNodes[ChildBodyNodeIt->second];
		const bool bEmphasized = ConstraintIndex == SelectedConstraintIndex
			|| ParentBodyIndex == SelectedBodyIndex
			|| ChildBodyIndex == SelectedBodyIndex;
		const ImU32 LinkColor = bEmphasized
			? IM_COL32(255, 214, 111, 255)
			: IM_COL32(194, 198, 206, 215);
		const float LinkThickness = (std::max)(1.0f, (bEmphasized ? 3.0f : 2.0f) * GraphViewState.Zoom);

		const ImVec2 ParentPin = GetOutputPin(ParentNode);
		const ImVec2 ConstraintInputPin = GetInputPin(ConstraintNode);
		const ImVec2 ConstraintOutputPin = GetOutputPin(ConstraintNode);
		const ImVec2 ChildPin = GetInputPin(ChildNode);
		const float ParentToConstraintTangent = (std::max)(48.0f * GraphViewState.Zoom, std::fabs(ConstraintInputPin.x - ParentPin.x) * 0.45f);
		const float ConstraintToChildTangent = (std::max)(48.0f * GraphViewState.Zoom, std::fabs(ChildPin.x - ConstraintOutputPin.x) * 0.45f);
		DrawList->AddBezierCubic(
			ParentPin,
			ImVec2(ParentPin.x + ParentToConstraintTangent, ParentPin.y),
			ImVec2(ConstraintInputPin.x - ParentToConstraintTangent, ConstraintInputPin.y),
			ConstraintInputPin,
			LinkColor,
			LinkThickness,
			24);
		DrawList->AddBezierCubic(
			ConstraintOutputPin,
			ImVec2(ConstraintOutputPin.x + ConstraintToChildTangent, ConstraintOutputPin.y),
			ImVec2(ChildPin.x - ConstraintToChildTangent, ChildPin.y),
			ChildPin,
			LinkColor,
			LinkThickness,
			24);
	}

	bool bMouseOverNode = false;
	bool bNodeActive = false;
	for (FPhysicsGraphNodeVisual& Node : GraphNodes)
	{
		const ImVec2 NodeMin = GetNodeMin(Node);
		const ImVec2 NodeMax = GetNodeMax(Node);
		const float Zoom = GraphViewState.Zoom;
		const float HeaderHeight = PhysicsGraphNodeHeaderHeight * Zoom;
		const float PaddingX = 10.0f * Zoom;
		const float PaddingY = 8.0f * Zoom;
		const ImU32 FillColor = Node.NodeKind == EPhysicsGraphNodeKind::Body
			? IM_COL32(138, 187, 112, 232)
			: IM_COL32(214, 206, 137, 236);
		const ImU32 HeaderColor = Node.NodeKind == EPhysicsGraphNodeKind::Body
			? IM_COL32(99, 138, 77, 255)
			: IM_COL32(188, 178, 103, 255);
		const ImU32 BorderColor = Node.bSelected
			? IM_COL32(255, 193, 58, 255)
			: IM_COL32(104, 108, 120, 255);
		const ImU32 TitleColor = IM_COL32(24, 25, 28, 255);
		const ImU32 TextColor = IM_COL32(29, 31, 35, 245);
		const ImU32 DetailColor = IM_COL32(42, 44, 48, 215);
		const float BorderThickness = (std::max)(1.0f, (Node.bSelected ? 2.5f : 1.2f) * Zoom);

		DrawList->AddRectFilled(NodeMin, NodeMax, FillColor, PhysicsGraphNodeRounding * Zoom);
		DrawList->AddRectFilled(
			NodeMin,
			ImVec2(NodeMax.x, NodeMin.y + HeaderHeight),
			HeaderColor,
			PhysicsGraphNodeRounding * Zoom,
			ImDrawFlags_RoundCornersTop);
		DrawList->AddRect(NodeMin, NodeMax, BorderColor, PhysicsGraphNodeRounding * Zoom, 0, BorderThickness);

		const float PinRadius = (std::max)(3.0f, 4.5f * Zoom);
		const ImVec2 InputPin = GetInputPin(Node);
		const ImVec2 OutputPin = GetOutputPin(Node);
		DrawList->AddCircleFilled(InputPin, PinRadius, IM_COL32(235, 237, 240, 255));
		DrawList->AddCircleFilled(OutputPin, PinRadius, IM_COL32(235, 237, 240, 255));
		DrawList->AddCircle(InputPin, PinRadius, IM_COL32(30, 32, 36, 255), 0, (std::max)(1.0f, Zoom));
		DrawList->AddCircle(OutputPin, PinRadius, IM_COL32(30, 32, 36, 255), 0, (std::max)(1.0f, Zoom));

		const float ContentWidth = (NodeMax.x - NodeMin.x) - PaddingX * 2.0f;
		DrawClippedText(
			DrawList,
			ImVec2(NodeMin.x + PaddingX, NodeMin.y + (HeaderHeight - ImGui::GetTextLineHeight() * Zoom) * 0.5f),
			ContentWidth,
			TitleColor,
			Node.Title,
			Zoom);
		DrawClippedText(
			DrawList,
			ImVec2(NodeMin.x + PaddingX, NodeMin.y + HeaderHeight + PaddingY),
			ContentWidth,
			TextColor,
			Node.Subtitle,
			Zoom);
		if (!Node.Detail.empty())
		{
			DrawClippedText(
				DrawList,
				ImVec2(NodeMin.x + PaddingX, NodeMin.y + HeaderHeight + PaddingY + 18.0f * Zoom),
				ContentWidth,
				DetailColor,
				Node.Detail,
				Zoom);
		}

		ImGui::PushID(Node.NodeKey.c_str());
		ImGui::SetCursorScreenPos(NodeMin);
		ImGui::InvisibleButton("##PhysicsGraphNode", ImVec2(NodeMax.x - NodeMin.x, NodeMax.y - NodeMin.y), ImGuiButtonFlags_MouseButtonLeft);
		const bool bHovered = ImGui::IsItemHovered();
		const bool bActive = ImGui::IsItemActive();
		bMouseOverNode |= bHovered;
		bNodeActive |= bActive;

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			if (Node.NodeKind == EPhysicsGraphNodeKind::Body)
			{
				SelectBodyByIndex(Node.DataIndex);
			}
			else
			{
				SelectConstraintByIndex(Node.DataIndex);
			}
		}

		if (bActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && Node.LayoutIndex >= 0 && Node.LayoutIndex < static_cast<int32>(GraphViewState.NodeLayouts.size()))
		{
			GraphViewState.NodeLayouts[Node.LayoutIndex].Position.X += IO.MouseDelta.x / GraphViewState.Zoom;
			GraphViewState.NodeLayouts[Node.LayoutIndex].Position.Y += IO.MouseDelta.y / GraphViewState.Zoom;
			Node.Position = GraphViewState.NodeLayouts[Node.LayoutIndex].Position;
			bLayoutChanged = true;
		}
		ImGui::PopID();
	}

	if (bCanvasHovered && !bNodeActive && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f)))
	{
		GraphViewState.Pan.X += IO.MouseDelta.x;
		GraphViewState.Pan.Y += IO.MouseDelta.y;
		bLayoutChanged = true;
	}

	if (bCanvasHovered && !bMouseOverNode && !bNodeActive && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		ClearSelection();
	}

	if (GraphNodes.empty())
	{
		const char* EmptyText = "No graph nodes available";
		const ImVec2 TextSize = ImGui::CalcTextSize(EmptyText);
		DrawList->AddText(
			ImVec2(CanvasMin.x + (CanvasSize.x - TextSize.x) * 0.5f, CanvasMin.y + (CanvasSize.y - TextSize.y) * 0.5f),
			IM_COL32(126, 130, 138, 200),
			EmptyText);
	}

	DrawList->PopClipRect();
	ImGui::SetCursorScreenPos(ImVec2(CanvasMin.x, CanvasMax.y + 4.0f));
	ImGui::TextDisabled("Zoom %.0f%% | Nodes %d", GraphViewState.Zoom * 100.0f, static_cast<int32>(GraphNodes.size()));
	return bLayoutChanged;
}

bool FPhysicsAssetEditorWidget::RenderDetailsPanel(UPhysicsAsset* PhysicsAsset)
{
	if (!PhysicsAsset)
	{
		return false;
	}

	bool bChanged = false;
	DrawPhysicsPanelHeader("Details");

	TArray<UPhysicsBodySetup*>& BodySetups = PhysicsAsset->GetMutableBodySetups();
	TArray<FPhysicsConstraintSetup>& ConstraintSetups = PhysicsAsset->GetMutableConstraintSetups();

	if (SelectedBodyIndex >= 0 && SelectedBodyIndex < static_cast<int32>(BodySetups.size()))
	{
		ImGui::Text("Body: %s", BodySetups[SelectedBodyIndex] ? BodySetups[SelectedBodyIndex]->GetTargetBoneName().ToString().c_str() : "None");
		ImGui::Separator();
		bChanged |= RenderBodySetupInspector(BodySetups[SelectedBodyIndex]);
	}
	else if (SelectedConstraintIndex >= 0 && SelectedConstraintIndex < static_cast<int32>(ConstraintSetups.size()))
	{
		bool bPreviewDefinitionChanged = false;
		bool bPreviewTransformChanged = false;
		ImGui::Text("Constraint: %s", ConstraintSetups[SelectedConstraintIndex].ConstraintName.ToString().c_str());
		ImGui::Separator();
		bChanged |= RenderConstraintInspector(
			ConstraintSetups[SelectedConstraintIndex],
			&bPreviewDefinitionChanged,
			&bPreviewTransformChanged);
		bConstraintPreviewDefinitionDirty |= bPreviewDefinitionChanged;
		bConstraintPreviewTransformDirty |= bPreviewTransformChanged;
	}
	else if (SelectedBoneIndex >= 0 && PreviewSkeletalMesh && PreviewSkeletalMesh->GetSkeletonAsset())
	{
		const FSkeletonAsset* SkeletonAsset = PreviewSkeletalMesh->GetSkeletonAsset();
		const FBone& Bone = SkeletonAsset->Bones[SelectedBoneIndex];
		ImGui::Text("Bone: %s", Bone.Name.c_str());
		ImGui::Text("Index: %d", SelectedBoneIndex);
		ImGui::Text("Parent Index: %d", Bone.ParentIndex);
		ImGui::Separator();

		const int32 ExistingBodyIndex = FindBodySetupIndex(PhysicsAsset, FName(Bone.Name));
		if (ExistingBodyIndex >= 0)
		{
			ImGui::TextDisabled("A physics body already exists for this bone.");
		}
		else if (ImGui::Button("Create Body For Selected Bone"))
		{
			AddFallbackBodyForBone(PhysicsAsset, Bone.Name);
			SelectBodyByIndex(static_cast<int32>(PhysicsAsset->GetBodySetups().size()) - 1);
			bChanged = true;
		}
	}
	else
	{
		ImGui::TextDisabled("Select a bone, body, or constraint.");
	}

	return bChanged;
}

bool FPhysicsAssetEditorWidget::RenderPreviewSettingsPanel(bool bShowHeader)
{
	bool bChanged = false;
	if (bShowHeader)
	{
		ImGui::TextUnformatted("Preview Settings");
		ImGui::Separator();
	}

	if (ImGui::Checkbox("Show Bodies", &bShowPreviewBodies))
	{
		bChanged = true;
	}

	if (ImGui::Checkbox("Show Constraints", &bShowConstraintDebug))
	{
		if (bShowConstraintDebug)
		{
			ViewportClient.GetRenderOptions().ShowFlags.bDebugDraw = true;
		}
		bChanged = true;
	}

	if (ImGui::Checkbox("Enable Self Collision", &bPreviewEnableSelfCollision))
	{
		if (USkeletalMeshComponent* MeshComponent = ViewportClient.GetPreviewMeshComponent())
		{
			MeshComponent->SetRagdollSelfCollisionEnabled(bPreviewEnableSelfCollision);
		}
		bChanged = true;
	}

	if (bShowConstraintDebug && !ViewportClient.GetRenderOptions().ShowFlags.bDebugDraw)
	{
		ImGui::TextDisabled("Constraint overlays use Show Flags > Debug Draw.");
	}

	if (bShowConstraintDebug)
	{
		bChanged |= ImGui::DragFloat("Constraint Size", &ConstraintAxisLength, 0.01f, 0.01f, 200.0f, "%.2f");
	}

	float ShapeTint[3] = { PreviewShapeTint.X, PreviewShapeTint.Y, PreviewShapeTint.Z };
	if (ImGui::ColorEdit3("Shape Color", ShapeTint))
	{
		PreviewShapeTint = FVector(ShapeTint[0], ShapeTint[1], ShapeTint[2]);
		bChanged = true;
	}

	bChanged |= ImGui::SliderFloat("Body Shape Opacity", &PreviewBaseShapeOpacity, 0.0f, 1.0f, "%.2f");
	bChanged |= ImGui::SliderFloat("Constraint Opacity", &PreviewConstraintShapeOpacity, 0.0f, 1.0f, "%.2f");

	if (ImGui::Button("Reset Preview Style"))
	{
		PreviewShapeTint = FVector(1.0f, 1.0f, 1.0f);
		PreviewBaseShapeOpacity = 0.3f;
		PreviewConstraintShapeOpacity = 0.35f;
		ConstraintAxisLength = 0.1f;
		bChanged = true;
	}

	ImGui::Separator();
	ImGui::TextWrapped("These settings affect only the editor preview. Self collision recreates the preview ragdoll when changed.");
	return bChanged;
}

bool FPhysicsAssetEditorWidget::RenderToolsPanel(UPhysicsAsset* PhysicsAsset)
{
	if (!PhysicsAsset)
	{
		return false;
	}

	bool bChanged = false;
	DrawPhysicsPanelHeader("Tools");

	ImGui::Text("Preview Mesh");
	ImGui::TextWrapped("%s", PhysicsAsset->GetPreviewSkeletalMeshPath().empty() ? "None" : PhysicsAsset->GetPreviewSkeletalMeshPath().c_str());
	ImGui::Separator();

	if (ImGui::CollapsingHeader("Body Creation", ImGuiTreeNodeFlags_DefaultOpen))
	{
		RenderCreateParamsEditor(ToolSettings);
		if (ToolSettings.RagdollMode != PhysicsAsset->GetRagdollMode())
		{
			ImGui::TextDisabled("Current Grouping: %s", PhysicsAssetRagdollModeToString(PhysicsAsset->GetRagdollMode()));
			ImGui::TextDisabled("Pending Regenerate: %s", PhysicsAssetRagdollModeToString(ToolSettings.RagdollMode));
		}
		else
		{
			ImGui::TextDisabled("Current Grouping: %s", PhysicsAssetRagdollModeToString(PhysicsAsset->GetRagdollMode()));
		}

		if (ImGui::Button("Regenerate From Preview Mesh"))
		{
			if (!PhysicsAsset->GetPreviewSkeletalMeshPath().empty() && GEngine)
			{
				if (FAssetFactory::PopulatePhysicsAssetFromSkeletalMesh(
					PhysicsAsset,
					PhysicsAsset->GetPreviewSkeletalMeshPath(),
					ToolSettings,
					GEngine->GetRenderer().GetFD3DDevice().GetDevice()))
				{
					SelectedConstraintIndex = -1;
					SelectedBoneIndex = -1;
					if (!PhysicsAsset->GetBodySetups().empty())
					{
						SelectBodyByIndex(0);
					}
					bChanged = true;
				}
			}
		}

		ImGui::Separator();

		if (ImGui::Button("Add Empty Body"))
		{
			UPhysicsBodySetup* NewBodySetup = GUObjectArray.CreateObject<UPhysicsBodySetup>(PhysicsAsset);
			NewBodySetup->SetTargetBoneName(FName("NewBody"));
			FPhysicsCapsuleShapeSetup CapsuleShape;
			CapsuleShape.Name = FName("NewBody_Capsule");
			CapsuleShape.Radius = 8.0f;
			CapsuleShape.Length = 24.0f;
			NewBodySetup->GetMutableShapeSetup().CapsuleShapeSetups.push_back(CapsuleShape);
			PhysicsAsset->GetMutableBodySetups().push_back(NewBodySetup);
			SelectBodyByIndex(static_cast<int32>(PhysicsAsset->GetBodySetups().size()) - 1);
			bChanged = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Add Empty Constraint"))
		{
			FPhysicsConstraintSetup NewConstraint;
			NewConstraint.ConstraintName = FName("NewConstraint");
			PhysicsAsset->GetMutableConstraintSetups().push_back(NewConstraint);
			SelectConstraintByIndex(static_cast<int32>(PhysicsAsset->GetConstraintSetups().size()) - 1);
			bChanged = true;
		}

		if (SelectedBodyIndex >= 0 && SelectedBodyIndex < static_cast<int32>(PhysicsAsset->GetBodySetups().size()))
		{
			if (ImGui::Button("Remove Selected Body"))
			{
				if (UPhysicsBodySetup* RemovedBody = PhysicsAsset->GetMutableBodySetups()[SelectedBodyIndex])
				{
					GUObjectArray.DestroyObject(RemovedBody);
				}
				PhysicsAsset->GetMutableBodySetups().erase(PhysicsAsset->GetMutableBodySetups().begin() + SelectedBodyIndex);
				SelectedBodyIndex = -1;
				bChanged = true;
			}
		}

		if (SelectedConstraintIndex >= 0 && SelectedConstraintIndex < static_cast<int32>(PhysicsAsset->GetConstraintSetups().size()))
		{
			if (ImGui::Button("Remove Selected Constraint"))
			{
				PhysicsAsset->GetMutableConstraintSetups().erase(PhysicsAsset->GetMutableConstraintSetups().begin() + SelectedConstraintIndex);
				SelectedConstraintIndex = -1;
				bChanged = true;
			}
		}
	}

	bChanged |= RenderBatchBodyPropertyTools(PhysicsAsset);

	return bChanged;
}

bool FPhysicsAssetEditorWidget::RenderBatchBodyPropertyTools(UPhysicsAsset* PhysicsAsset)
{
	if (!PhysicsAsset)
	{
		return false;
	}

	if (!ImGui::CollapsingHeader("Batch Body Properties", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return false;
	}

	const int32 BodyCount = static_cast<int32>(PhysicsAsset->GetBodySetups().size());
	ImGui::TextDisabled("Apply selected properties to all %d bodies in this asset.", BodyCount);

	const bool bHasSelectedBody = SelectedBodyIndex >= 0
		&& SelectedBodyIndex < BodyCount
		&& PhysicsAsset->GetBodySetups()[SelectedBodyIndex] != nullptr;
	if (bHasSelectedBody && ImGui::Button("Copy Values From Selected Body"))
	{
		LoadBatchBodyPropertyValues(PhysicsAsset->GetBodySetups()[SelectedBodyIndex]);
		BatchBodyEditSettings.bApplyOverrideMass = true;
		BatchBodyEditSettings.bApplyMass = true;
		BatchBodyEditSettings.bApplyLinearDamping = true;
		BatchBodyEditSettings.bApplyAngularDamping = true;
		BatchBodyEditSettings.bApplyEnableGravity = true;
		BatchBodyEditSettings.bApplyGravityGroupIndex = true;
		BatchBodyEditSettings.bApplyBodyType = true;
		ToolbarStatusMessage = "Loaded batch values from the selected body.";
	}

	if (ImGui::BeginTable("BatchBodyPropertyTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
	{
		auto Row = [](const char* Label)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(Label);
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1.0f);
		};

		Row("Override Mass");
		ImGui::Checkbox("##BatchApplyOverrideMass", &BatchBodyEditSettings.bApplyOverrideMass);
		ImGui::SameLine();
		ImGui::BeginDisabled(!BatchBodyEditSettings.bApplyOverrideMass);
		ImGui::Checkbox("##BatchOverrideMass", &BatchBodyEditSettings.bOverrideMass);
		ImGui::EndDisabled();

		Row("Mass (kg)");
		ImGui::Checkbox("##BatchApplyMass", &BatchBodyEditSettings.bApplyMass);
		ImGui::SameLine();
		ImGui::BeginDisabled(!BatchBodyEditSettings.bApplyMass);
		ImGui::DragFloat("##BatchMass", &BatchBodyEditSettings.Mass, 0.05f, 0.0f, 100000.0f);
		ImGui::EndDisabled();

		Row("Linear Damping");
		ImGui::Checkbox("##BatchApplyLinearDamping", &BatchBodyEditSettings.bApplyLinearDamping);
		ImGui::SameLine();
		ImGui::BeginDisabled(!BatchBodyEditSettings.bApplyLinearDamping);
		ImGui::DragFloat("##BatchLinearDamping", &BatchBodyEditSettings.LinearDamping, 0.01f, 0.0f, 1000.0f);
		ImGui::EndDisabled();

		Row("Angular Damping");
		ImGui::Checkbox("##BatchApplyAngularDamping", &BatchBodyEditSettings.bApplyAngularDamping);
		ImGui::SameLine();
		ImGui::BeginDisabled(!BatchBodyEditSettings.bApplyAngularDamping);
		ImGui::DragFloat("##BatchAngularDamping", &BatchBodyEditSettings.AngularDamping, 0.01f, 0.0f, 1000.0f);
		ImGui::EndDisabled();

		Row("Enable Gravity");
		ImGui::Checkbox("##BatchApplyEnableGravity", &BatchBodyEditSettings.bApplyEnableGravity);
		ImGui::SameLine();
		ImGui::BeginDisabled(!BatchBodyEditSettings.bApplyEnableGravity);
		ImGui::Checkbox("##BatchEnableGravity", &BatchBodyEditSettings.bEnableGravity);
		ImGui::EndDisabled();

		Row("Gravity Group Index");
		ImGui::Checkbox("##BatchApplyGravityGroupIndex", &BatchBodyEditSettings.bApplyGravityGroupIndex);
		ImGui::SameLine();
		ImGui::BeginDisabled(!BatchBodyEditSettings.bApplyGravityGroupIndex);
		ImGui::InputInt("##BatchGravityGroupIndex", &BatchBodyEditSettings.GravityGroupIndex);
		ImGui::EndDisabled();

		Row("Physics Type");
		ImGui::Checkbox("##BatchApplyBodyType", &BatchBodyEditSettings.bApplyBodyType);
		ImGui::SameLine();
		ImGui::BeginDisabled(!BatchBodyEditSettings.bApplyBodyType);
		RenderBodyTypeCombo(BatchBodyEditSettings.BodyType, "##BatchBodyType");
		ImGui::EndDisabled();

		ImGui::EndTable();
	}

	ImGui::TextDisabled("Check a property to include it in the batch update.");
	if (ImGui::Button("Apply To All Bodies"))
	{
		return ApplyBatchBodyProperties(PhysicsAsset);
	}

	return false;
}

bool FPhysicsAssetEditorWidget::ApplyBatchBodyProperties(UPhysicsAsset* PhysicsAsset)
{
	if (!PhysicsAsset)
	{
		return false;
	}

	const bool bHasSelectedProperty =
		BatchBodyEditSettings.bApplyOverrideMass
		|| BatchBodyEditSettings.bApplyMass
		|| BatchBodyEditSettings.bApplyLinearDamping
		|| BatchBodyEditSettings.bApplyAngularDamping
		|| BatchBodyEditSettings.bApplyEnableGravity
		|| BatchBodyEditSettings.bApplyGravityGroupIndex
		|| BatchBodyEditSettings.bApplyBodyType;
	if (!bHasSelectedProperty)
	{
		ToolbarStatusMessage = "Select at least one body property to apply.";
		return false;
	}

	TArray<UPhysicsBodySetup*>& BodySetups = PhysicsAsset->GetMutableBodySetups();
	if (BodySetups.empty())
	{
		ToolbarStatusMessage = "There are no bodies to update.";
		return false;
	}

	constexpr float FloatTolerance = 1.0e-4f;
	int32 UpdatedBodyCount = 0;
	for (UPhysicsBodySetup* BodySetup : BodySetups)
	{
		if (!BodySetup)
		{
			continue;
		}

		bool bBodyChanged = false;
		if (BatchBodyEditSettings.bApplyOverrideMass && BodySetup->bOverrideMass != BatchBodyEditSettings.bOverrideMass)
		{
			BodySetup->bOverrideMass = BatchBodyEditSettings.bOverrideMass;
			bBodyChanged = true;
		}

		if (BatchBodyEditSettings.bApplyMass
			&& std::fabs(BodySetup->GetMass() - BatchBodyEditSettings.Mass) > FloatTolerance)
		{
			BodySetup->SetMass(BatchBodyEditSettings.Mass);
			bBodyChanged = true;
		}

		if (BatchBodyEditSettings.bApplyLinearDamping
			&& std::fabs(BodySetup->GetLinearDamping() - BatchBodyEditSettings.LinearDamping) > FloatTolerance)
		{
			BodySetup->SetLinearDamping(BatchBodyEditSettings.LinearDamping);
			bBodyChanged = true;
		}

		if (BatchBodyEditSettings.bApplyAngularDamping
			&& std::fabs(BodySetup->GetAngularDamping() - BatchBodyEditSettings.AngularDamping) > FloatTolerance)
		{
			BodySetup->SetAngularDamping(BatchBodyEditSettings.AngularDamping);
			bBodyChanged = true;
		}

		if (BatchBodyEditSettings.bApplyEnableGravity && BodySetup->bEnableGravity != BatchBodyEditSettings.bEnableGravity)
		{
			BodySetup->bEnableGravity = BatchBodyEditSettings.bEnableGravity;
			bBodyChanged = true;
		}

		if (BatchBodyEditSettings.bApplyGravityGroupIndex
			&& BodySetup->GravityGroupIndex != BatchBodyEditSettings.GravityGroupIndex)
		{
			BodySetup->GravityGroupIndex = BatchBodyEditSettings.GravityGroupIndex;
			bBodyChanged = true;
		}

		if (BatchBodyEditSettings.bApplyBodyType && BodySetup->GetBodyType() != BatchBodyEditSettings.BodyType)
		{
			BodySetup->SetBodyType(BatchBodyEditSettings.BodyType);
			bBodyChanged = true;
		}

		if (bBodyChanged)
		{
			++UpdatedBodyCount;
		}
	}

	if (UpdatedBodyCount == 0)
	{
		ToolbarStatusMessage = "All bodies already match the selected values.";
		return false;
	}

	ToolbarStatusMessage = "Applied body properties to " + std::to_string(UpdatedBodyCount) + " bodies.";
	return true;
}

void FPhysicsAssetEditorWidget::LoadBatchBodyPropertyValues(const UPhysicsBodySetup* BodySetup)
{
	if (!BodySetup)
	{
		return;
	}

	BatchBodyEditSettings.bOverrideMass = BodySetup->bOverrideMass;
	BatchBodyEditSettings.Mass = BodySetup->GetMass();
	BatchBodyEditSettings.LinearDamping = BodySetup->GetLinearDamping();
	BatchBodyEditSettings.AngularDamping = BodySetup->GetAngularDamping();
	BatchBodyEditSettings.bEnableGravity = BodySetup->bEnableGravity;
	BatchBodyEditSettings.GravityGroupIndex = BodySetup->GravityGroupIndex;
	BatchBodyEditSettings.BodyType = BodySetup->GetBodyType();
}
