#include "Mesh/Fbx/FbxSectionBuilder.h"

#include <fbxsdk.h>

namespace
{
    // mesh의 첫 material layer element를 찾는다.
    FbxLayerElementMaterial* FindFirstMaterialLayerElement(FbxMesh* Mesh)
    {
        if (!Mesh)
        {
            return nullptr;
        }

        const int32 LayerCount = Mesh->GetLayerCount();
        for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
        {
            FbxLayer* Layer = Mesh->GetLayer(LayerIndex);
            if (!Layer)
            {
                continue;
            }

            FbxLayerElementMaterial* MaterialElement = Layer->GetMaterials();
            if (MaterialElement)
            {
                return MaterialElement;
            }
        }

        return nullptr;
    }
}

// material index에 해당하는 임시 section을 찾거나 새로 추가한다.
FFbxImportedSectionBuild* FFbxSectionBuilder::FindOrAddSection(int32 MaterialIndex)
{
    for (FFbxImportedSectionBuild& Section : Sections)
    {
        if (Section.MaterialIndex == MaterialIndex)
        {
            return &Section;
        }
    }

    FFbxImportedSectionBuild NewSection;
    NewSection.MaterialIndex = MaterialIndex;
    Sections.push_back(NewSection);
    return &Sections.back();
}

// 임시 section 배열을 최종 index buffer와 section 배열로 변환한다.
void FFbxSectionBuilder::BuildFinalSections(const TArray<FStaticMaterial>& Materials, TArray<uint32>& OutIndices, TArray<FStaticMeshSection>& OutSections) const
{
    OutIndices.clear();
    OutSections.clear();

    for (const FFbxImportedSectionBuild& BuildSection : Sections)
    {
        if (BuildSection.Indices.empty())
        {
            continue;
        }

        FStaticMeshSection Section;
        Section.MaterialIndex = BuildSection.MaterialIndex;
        Section.FirstIndex    = static_cast<uint32>(OutIndices.size());
        Section.NumTriangles  = static_cast<uint32>(BuildSection.Indices.size() / 3);

        if (BuildSection.MaterialIndex >= 0 && BuildSection.MaterialIndex < static_cast<int32>(Materials.size()))
        {
            Section.MaterialSlotName = Materials[BuildSection.MaterialIndex].MaterialSlotName;
        }
        else
        {
            Section.MaterialSlotName = "None";
        }

        OutIndices.insert(OutIndices.end(), BuildSection.Indices.begin(), BuildSection.Indices.end());
        OutSections.push_back(Section);
    }
}

// 임시 section이 비어 있는지 확인한다.
bool FFbxSectionBuilder::IsEmpty() const
{
    return Sections.empty();
}

int32 FFbxMaterialSlotResolver::GetPolygonMaterialIndex(FbxMesh* Mesh, int32 PolygonIndex)
{
    FbxLayerElementMaterial* MaterialElement = FindFirstMaterialLayerElement(Mesh);
    if (!MaterialElement)
    {
        return 0;
    }

    const FbxLayerElement::EMappingMode   MappingMode   = MaterialElement->GetMappingMode();
    const FbxLayerElement::EReferenceMode ReferenceMode = MaterialElement->GetReferenceMode();

    int32 ElementIndex = 0;

    switch (MappingMode)
    {
    case FbxLayerElement::eByPolygon:
        ElementIndex = PolygonIndex;
        break;

    case FbxLayerElement::eAllSame:
        ElementIndex = 0;
        break;

    default:
        return 0;
    }

    if (ElementIndex < 0)
    {
        return 0;
    }

    if (ReferenceMode == FbxLayerElement::eIndexToDirect || ReferenceMode == FbxLayerElement::eIndex)
    {
        if (ElementIndex >= 0 && ElementIndex < MaterialElement->GetIndexArray().GetCount())
        {
            return MaterialElement->GetIndexArray().GetAt(ElementIndex);
        }

        return 0;
    }

    return 0;
}

FbxSurfaceMaterial* FFbxMaterialSlotResolver::ResolvePolygonMaterial(FbxNode* MeshNode, FbxMesh* Mesh, int32 PolygonIndex)
{
    if (!MeshNode || !Mesh)
    {
        return nullptr;
    }

    const int32 LocalMaterialIndex = FFbxMaterialSlotResolver::GetPolygonMaterialIndex(Mesh, PolygonIndex);
    if (LocalMaterialIndex < 0 || LocalMaterialIndex >= MeshNode->GetMaterialCount())
    {
        return nullptr;
    }

    return MeshNode->GetMaterial(LocalMaterialIndex);
}
