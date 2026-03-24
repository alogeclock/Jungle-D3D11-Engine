#include "Source/Editor/Public/Application.h"
#include "Source/Engine/Public/GUI/Outliner.h"

void Outliner::ShowOutliner()
{
    USelection* Selection = GEditor->GetSelection();

    ShowObjectInfo(Selection->GetSelectedObject());
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

void Outliner::ShowObjectInfo(UObject* InObject)
{
    if (InObject == nullptr)
        return;

    if (!InObject->IsValid())
        return;

    ImGui::Text("Object UUID: %d", InObject->GetUUID());


    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    strncpy_s(buffer, InObject->GetName().ToString().c_str(), sizeof(buffer) - 1);

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;

    if (ImGui::InputText("Name", buffer, sizeof(buffer), flags))
    {
        InObject->SetName(FString(buffer));
    }
}

void Outliner::ShowObjectProperty(UObject* InObject)
{
    if (InObject == nullptr)
        return;

    if (!InObject->IsValid())
        return;

    USelection* Selection = GEditor->GetSelection();

    TArray<FProperty>& Properties = InObject->GetClass()->GetProperties();
    for (const auto& Property : Properties)
    {
        if (Property.Type == EPropertyType::UObjectPtr)
        {
            UObject** ObjectPtr = reinterpret_cast<UObject**>(Property.GetValuePtr(InObject));
            UObject* Object = (ObjectPtr != nullptr) ? *ObjectPtr : nullptr;

            if (Object == nullptr)
                continue;

            ImGui::Separator();
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
            // ImGui::Separator();
            ShowObjectProperty(Object);
        }
        else if (Property.Type == EPropertyType::Transform)
        {
            FTransform* Transform = reinterpret_cast<FTransform*>(Property.GetValuePtr(InObject));
            ImGui::Separator();
            ImGui::Text("Transform");
            ImGui::DragFloat3("Location", &Transform->Location.X, 0.01f);
            ImGui::DragFloat3("Rotation", &Transform->Rotation.X, 0.01f);
            ImGui::DragFloat3("Scale", &Transform->Scale.X, 0.01f, 0.f, FLT_MAX);

            USceneComponent* component = static_cast<USceneComponent*>(InObject);
            component->SetTransform(*Transform);
        }
        else if (Property.Type == EPropertyType::Float)
        {
            float* FloatType = reinterpret_cast<float*>(Property.GetValuePtr(InObject));
            ImGui::Separator();
            ImGui::DragFloat(Property.Name.c_str(), FloatType);
        }
        else if (Property.Type == EPropertyType::UObjectPtrArray)
        {
            TArray<UObject*>* ArrayPtr = reinterpret_cast<TArray<UObject*>*>(Property.GetValuePtr(InObject));

            if (!ArrayPtr)
                continue;
            ImGui::Separator();
            ImGui::Text(Property.Name.c_str());
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
    }
}

void Outliner::ShowOutliner(TArray<UObject*>& ObjectArray)
{
    TMap<UObject*, TArray<UObject*>> OuterGraph;
    TArray<UObject*> RootObjects;

    // 1. 그래프 구성
    for (UObject* Object : ObjectArray)
    {
        if (!Object)
            continue;

        FName Name = Object->GetName();
        if (Name == FName("EditorGrid") || Name == FName("EditorAxis") || Name == FName("EditorGizmo"))
        {
            continue;
        }

        if (!Object->GetOuter() && Name == FName("World"))
        {
            RootObjects.push_back(Object);
            continue;
        }

        OuterGraph[Object->GetOuter()].push_back(Object);
    }

    USelection* Selection = GEditor->GetSelection();
    if (!Selection)
        return;

    ImGuiIO& io = ImGui::GetIO();

    TSet<UObject*> Visited;
    TArray<UObject*> Sequence;

    bool bShiftClick = false;
    bool bCtrlClick = false;
    int32 ClickedIndex = -1;
    int32 AnchorIndex = -1;
    int32 CurrentIndex = 0;

    // anchor = 현재 선택된 첫 번째 오브젝트
    auto FindAnchorIndex = [&](UObject* Current) {
        if (AnchorIndex >= 0)
            return;

        if (Selection->IsSelected(Current))
        {
            AnchorIndex = CurrentIndex;
        }
    };

    std::function<void(UObject*)> DrawNode = [&](UObject* Current) {
        if (!Current)
            return;
        if (Visited.contains(Current))
            return;

        Visited.insert(Current);
        Sequence.push_back(Current);

        if (!Selection->IsEmpty() && Current == Selection->GetSelectedObject())
            AnchorIndex = CurrentIndex;

        FString Label = Current->GetName().ToString(); //        +" UUID: " + std::to_string(Current->GetUUID());

        TArray<UObject*>& Children = OuterGraph[Current];

        ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAvailWidth;
        if (Selection->IsSelected(Current))
            Flags |= ImGuiTreeNodeFlags_Selected;
        if (Children.empty())
            Flags |= ImGuiTreeNodeFlags_Leaf;

        ImGui::PushID(Current);

        bool bOpened = false;
        if (Children.empty())
            bOpened = ImGui::TreeNodeEx(Label.c_str(), Flags);
        else
            bOpened = ImGui::TreeNodeEx(Label.c_str(),
                                        Flags | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow);

        const bool bSelectableType =
            Current->IsA(AActor::StaticClass()) || Current->IsA(USceneComponent::StaticClass());

        if (ImGui::IsItemClicked() && bSelectableType)
        {
            bShiftClick = io.KeyShift;
            bCtrlClick = io.KeyCtrl;
            ClickedIndex = CurrentIndex;
        }

        ++CurrentIndex;

        if (bOpened)
        {
            for (UObject* Child : Children)
            {
                DrawNode(Child);
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    };

    // 2. 루트부터 렌더 + 평탄화
    for (UObject* Root : RootObjects)
    {
        DrawNode(Root);
    }

    // 3. 클릭 결과 반영
    if (ClickedIndex < 0 || ClickedIndex >= static_cast<int32>(Sequence.size()))
        return;

    if (bShiftClick)
    {
        if (AnchorIndex < 0)
            AnchorIndex = ClickedIndex;

        int32 MinIndex = (ClickedIndex < AnchorIndex) ? ClickedIndex : AnchorIndex;
        int32 MaxIndex = (ClickedIndex > AnchorIndex) ? ClickedIndex : AnchorIndex;

        Selection->Clear();
        Selection->AddObject(Sequence[AnchorIndex]);
        for (int32 i = MinIndex; i <= MaxIndex; ++i) // <= 중요
        {
            UObject* Target = Sequence[i];
            if (!Target)
                continue;

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
            // RemoveObject가 있으면 토글 추천
            if (!Selection->IsSelected(Target))
            {
                Selection->AddObject(Target);
            }
            else
            {
                Selection->RemoveObject(Target);
            }
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

void Outliner::ShowOutliner(UObject* Object, TMap<UObject*, TArray<UObject*>>& Dependencies,
                                 TSet<UObject*>& Visited)
{
    if (Object == nullptr)
        return;
    if (Visited.contains(Object))
        return;

    FString Name = "";
    Name += Object->GetName().ToString() + " UUID: " + std::to_string(Object->GetUUID());

    Visited.insert(Object);

    TArray<UObject*> childs = Dependencies[Object];
    ImVec2 ButtonSize(100, 10);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
    bool opened = false;

    USelection* Selection = GEditor->GetSelection();
    if (Selection->IsSelected(Object))
        flags |= ImGuiTreeNodeFlags_Selected;

    if (childs.size() <= 0)
    {
        opened = ImGui::TreeNodeEx(Name.c_str(), flags | ImGuiTreeNodeFlags_Leaf);
        if (ImGui::IsItemClicked() &&
            (Object->IsA(AActor::StaticClass()) || Object->IsA(USceneComponent::StaticClass())))
        {
            ImGuiIO& io = ImGui::GetIO();
            if (!io.KeyCtrl)
                Selection->Clear();
            Selection->AddObject(Object);
        }

        if (opened)
        {
            ImGui::TreePop();
        }
    }
    else
    {
        opened = ImGui::TreeNodeEx(Name.c_str(), flags | ImGuiTreeNodeFlags_DefaultOpen);
        if (ImGui::IsItemClicked() &&
            (Object->IsA(AActor::StaticClass()) || Object->IsA(USceneComponent::StaticClass())))
        {
            ImGuiIO& io = ImGui::GetIO();
            if (!io.KeyCtrl)
                Selection->Clear();
            Selection->AddObject(Object);
        }
        if (opened)
        {
            for (const auto& child : childs)
                ShowOutliner(child, Dependencies, Visited);

            ImGui::TreePop();
        }
    }
}
