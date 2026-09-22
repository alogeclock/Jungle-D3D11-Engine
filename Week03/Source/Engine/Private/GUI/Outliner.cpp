#pragma once
#include "Source/Editor/Public/Application.h"
#include "Source/Engine/Public/GUI/Outliner.h"

void FOutliner::ShowOutliner()
{
    USelection* Selection = GEditor->GetSelection();
    ShowObjectInfo(Selection->GetSelectedObject());
    ImGui::SetCursorPosY(120.f);
    ImGui::BeginChild("OutlinerRegion", ImVec2(0, outlinerHeight), true);
    {
        ShowOutliner(GUObjectArray);
    }
    ImGui::EndChild();

    ImGui::InvisibleButton("H_Splitter", ImVec2(-1, splitterThickness));
    if (ImGui::IsItemActive())
    {
        outlinerHeight += ImGui::GetIO().MouseDelta.y;
    }

    float availHeight = ImGui::GetContentRegionAvail().y;

    float minTop = 100.0f;
    float minBottom = 100.0f;
    float maxTop = availHeight - splitterThickness - minBottom;
    if (outlinerHeight < minTop)
        outlinerHeight = minTop;
    if (outlinerHeight > maxTop)
        outlinerHeight = maxTop;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    draw->AddRectFilled(min, max, IM_COL32(90, 90, 90, 255));

    ImGui::BeginChild("InspectorRegion", ImVec2(0, 0), true);
    {
        USelection* Selection = GEditor->GetSelection();
        if (Selection->GetCount() > 0)
            ShowObjectProperty((*Selection)[0]);
    }
    ImGui::EndChild();
}

void FOutliner::ShowObjectInfo(UObject* InObject)
{
    if (InObject == nullptr)
        return;

    if (!InObject->IsValid())
        return;

    ImGui::Text("Class: ");
    ImGui::SameLine();
    ImGui::SetCursorPosX(100.f);
    ImGui::Text(InObject->GetClass()->GetName());

    ImGui::Text("Object UUID: %d", InObject->GetUUID());

    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    strncpy_s(buffer, InObject->GetName().ToString().c_str(), sizeof(buffer) - 1);

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;

    ImGui::Text("Name: ");
    ImGui::SameLine();
    ImGui::SetCursorPosX(100.f);
    if (ImGui::InputText("##Name", buffer, sizeof(buffer), flags))
    {
        InObject->SetName(FString(buffer));
    }
}

void FOutliner::ShowObjectProperty(UObject* InObject)
{
    if (InObject == nullptr)
        return;

    if (!InObject->IsValid())
        return;

    USelection* Selection = GEditor->GetSelection();

    TArray<FProperty>& Properties = InObject->GetClass()->GetProperties();
    for (const auto& Property : Properties)
    {
        ImGui::Text(Property.Name.c_str());
        if (Property.Type == EPropertyType::UObjectPtr)
        {
            UObject** ObjectPtr = reinterpret_cast<UObject**>(Property.GetValuePtr(InObject));
            UObject* Object = (ObjectPtr != nullptr) ? *ObjectPtr : nullptr;

            if (Object == nullptr)
                continue;

            if (ImGui::Button(Object->GetName().ToString().c_str(), {100.f, 15.f}))
            {
                Selection->Clear();
                Selection->AddObject(Object);
            }
        }
        else if (Property.Type == EPropertyType::UObjectDetail)
        {
            UObject** ObjectPtr = reinterpret_cast<UObject**>(Property.GetValuePtr(InObject));
            UObject* Object = (ObjectPtr != nullptr) ? *ObjectPtr : nullptr;
            if (Object == nullptr)
                continue;

            FString DisplayText = ": ";
            DisplayText  += Object->GetName().ToString();
            ImGui::SameLine();
            ImGui::Text(DisplayText.c_str());
            ShowObjectProperty(Object);

        }
        else if (Property.Type == EPropertyType::Transform)
        {
            FTransform* Transform = reinterpret_cast<FTransform*>(Property.GetValuePtr(InObject));
            float align = 100.f;

            float RadToDeg = 57.2958f;
            float DegToRad = 1 / RadToDeg;
            float value;

            ImGui::Text("Location");
            ImGui::SameLine();
            ImGui::SetCursorPosX(align);
            ImGui::DragFloat3("##Location", &Transform->Location.X, 0.01f);

            ImGui::Text("Rotation");
            ImGui::SameLine();
            ImGui::SetCursorPosX(align);

            FVector<float> TempRotation = Transform->Rotation * RadToDeg;
            ImGui::DragFloat3("##Rotation", &TempRotation.X, 0.1f);
            Transform->Rotation = TempRotation * DegToRad;

            ImGui::Text("Scale");
            ImGui::SameLine();
            ImGui::SetCursorPosX(align);
            ImGui::DragFloat3("##Scale", &Transform->Scale.X, 0.01f, 0.f, FLT_MAX);

            USceneComponent* component = static_cast<USceneComponent*>(InObject);
            component->SetTransform(*Transform);
        }
        else if (Property.Type == EPropertyType::Float)
        {
            float* FloatType = reinterpret_cast<float*>(Property.GetValuePtr(InObject));
            ImGui::DragFloat(Property.Name.c_str(), FloatType);
        }
        else if (Property.Type == EPropertyType::UObjectPtrArray)
        {
            TArray<UObject*>* ArrayPtr = reinterpret_cast<TArray<UObject*>*>(Property.GetValuePtr(InObject));

            if (!ArrayPtr)
                continue;
            for (UObject* Object : *ArrayPtr)
            {
                if (!Object)
                    continue;

                if (ImGui::SmallButton(Object->GetName().ToString().c_str()))
                {
                    Selection->Clear();
                    Selection->AddObject(Object);
                }
            }
        }
        else if (Property.Type == EPropertyType::String)
        {
            FString* StringPtr = reinterpret_cast<FString*>(Property.GetValuePtr(InObject));
            if (!StringPtr)
                return;

            char buffer[256];
            memset(buffer, 0, sizeof(buffer));
            strncpy_s(buffer, StringPtr->c_str(), sizeof(buffer) - 1);

            if (ImGui::InputText(Property.Name.c_str(), buffer, sizeof(buffer)))
            {
                *StringPtr = FString(buffer);
            }
        }
        else if (Property.Type == EPropertyType::Bool)
        {
            bool* BoolPtr = reinterpret_cast<bool*>(Property.GetValuePtr(InObject));
            if (!BoolPtr)
                return;

            ImGui::Checkbox(Property.Name.c_str(), BoolPtr);
        }
        else
        {
            ImGui::Text(Property.Name.c_str());
        }
        ImGui::Separator();
    }
}

void FOutliner::ShowOutliner(TArray<UObject*>& ObjectArray)
{
    TMap<UObject*, TArray<UObject*>> OuterGraph;
    TArray<UObject*> RootObjects;

    // 1. 그래프 구성 (기존과 동일)
    for (UObject* Object : ObjectArray)
    {
        if (!Object) continue;

        FName Name = Object->GetName();
        if (!Object->GetOuter() && Name == FName("World"))
        {
            RootObjects.push_back(Object);
            continue;
        }

        OuterGraph[Object->GetOuter()].push_back(Object);
    }

    USelection* Selection = GEditor->GetSelection();
    if (!Selection) return;

    ImGuiIO& io = ImGui::GetIO();
    TSet<UObject*> Visited;

    // -------------------------------------------------------------
    // [최적화 1단계] 보여지는 노드만 1차원 배열로 평탄화 (Flatten)
    // -------------------------------------------------------------
    struct FFlatNode 
    {
        UObject* Object;
        int32 Depth;
        int32 Index;
    };
    TArray<FFlatNode> FlatTree;
    TArray<UObject*> Sequence;

    std::function<void(UObject*, int32)> FlattenTree = [&](UObject* Current, int32 Depth) {
        if (!Current || Visited.contains(Current)) return;
        Visited.insert(Current);

        // 현재 노드를 1차원 배열에 추가
        FlatTree.push_back({ Current, Depth, static_cast<int32>(Sequence.size()) });
        Sequence.push_back(Current);

        // 노드가 열려(Expanded) 있을 때만 자식 노드들을 배열에 추가 (재귀)
        if (ExpandedNodes.contains(Current))
        {
            for (UObject* Child : OuterGraph[Current])
            {
                FlattenTree(Child, Depth + 1);
            }
        }
    };

    for (UObject* Root : RootObjects)
    {
        FlattenTree(Root, 0);
    }

    // -------------------------------------------------------------
    // [최적화 2단계] ImGuiListClipper 적용 및 렌더링
    // -------------------------------------------------------------
    bool bShiftClick = false;
    bool bCtrlClick = false;
    int32 ClickedIndex = -1;
    int32 AnchorIndex = -1;

    ImGuiListClipper Clipper;
    Clipper.Begin(FlatTree.size()); // 화면에 보일 수 있는 전체 리스트 개수 전달

    while (Clipper.Step())
    {
        for (int i = Clipper.DisplayStart; i < Clipper.DisplayEnd; ++i)
        {
            const FFlatNode& NodeData = FlatTree[i];
            UObject* Current = NodeData.Object;
            int32 Depth = NodeData.Depth;
            int32 CurrentIndex = NodeData.Index;

            if (!Selection->IsEmpty() && Current == Selection->GetSelectedObject())
                AnchorIndex = CurrentIndex;

            TArray<UObject*>& Children = OuterGraph[Current];

            ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
            if (Selection->IsSelected(Current)) Flags |= ImGuiTreeNodeFlags_Selected;
            if (Children.empty()) Flags |= ImGuiTreeNodeFlags_Leaf;

            // 트리 계층 구조의 들여쓰기(Indent)를 수동으로 적용
            float IndentWidth = Depth * ImGui::GetStyle().IndentSpacing;
            if (IndentWidth > 0.0f) ImGui::Indent(IndentWidth);

            // ImGui에게 이 노드가 열려야 하는지 강제로 알려줌
            bool bIsExpanded = ExpandedNodes.contains(Current);
            ImGui::SetNextItemOpen(bIsExpanded);

            ImGui::PushID(Current);
            FString Label = Current->GetName().ToString();
            
            bool bOpened = ImGui::TreeNodeEx(Label.c_str(), Flags);

            // 화살표를 눌러서 노드를 열거나 닫았을 때 상태 업데이트
            if (ImGui::IsItemToggledOpen())
            {
                if (bIsExpanded) ExpandedNodes.erase(Current);
                else ExpandedNodes.insert(Current);
            }

            // 1차원 루프를 돌고 있으므로, 열렸다고 하더라도 내부 중첩을 막기 위해 즉시 Pop
            if (bOpened)
            {
                ImGui::TreePop();
            }

            // 들여쓰기 복구
            if (IndentWidth > 0.0f) ImGui::Unindent(IndentWidth);

            // 클릭 및 선택 처리
            const bool bSelectableType = Current->IsA(AActor::StaticClass()) || Current->IsA(USceneComponent::StaticClass());
            
            // 화살표를 누른 것이 아니고(토글X), 아이템 자체를 클릭했을 때
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() && bSelectableType)
            {
                bShiftClick = io.KeyShift;
                bCtrlClick = io.KeyCtrl;
                ClickedIndex = CurrentIndex;
            }

            ImGui::PopID();
        }
    }
    Clipper.End();

    // 3. 클릭 결과 반영 (기존 코드와 완벽히 동일하게 동작)
    if (ClickedIndex < 0 || ClickedIndex >= static_cast<int32>(Sequence.size()))
        return;

    if (bShiftClick)
    {
        if (AnchorIndex < 0) AnchorIndex = ClickedIndex;

        int32 MinIndex = (ClickedIndex < AnchorIndex) ? ClickedIndex : AnchorIndex;
        int32 MaxIndex = (ClickedIndex > AnchorIndex) ? ClickedIndex : AnchorIndex;

        Selection->Clear();
        Selection->AddObject(Sequence[AnchorIndex]);
        for (int32 i = MinIndex; i <= MaxIndex; ++i)
        {
            UObject* Target = Sequence[i];
            if (!Target) continue;

            if (Target->IsA(AActor::StaticClass()) || Target->IsA(USceneComponent::StaticClass()))
            {
                Selection->AddObject(Target);
            }
        }
    }
    else if (bCtrlClick)
    {
        UObject* Target = Sequence[ClickedIndex];
        if (Target)
        {
            if (!Selection->IsSelected(Target)) Selection->AddObject(Target);
            else Selection->RemoveObject(Target);
        }
    }
    else
    {
        UObject* Target = Sequence[ClickedIndex];
        if (Target)
        {
            Selection->Clear();
            Selection->AddObject(Target);
        }
    }
}