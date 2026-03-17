#include "Engine/Source/Runtime/Engine/Public/Classes/MeshManager.h"

void UMeshManager::Initialize(URenderer &Renderer)
{   
	VertexData.emplace(EPrimitiveType::Cube, &cube_vertices);
	VertexData.emplace(EPrimitiveType::Sphere, &sphere_vertices);
	VertexData.emplace(EPrimitiveType::Triangle, &triangle_vertices);
	VertexData.emplace(EPrimitiveType::Plane, &plane_vertices);
	VertexData.emplace(EPrimitiveType::Arrow, &arrow_vertices);
	VertexData.emplace(EPrimitiveType::CubeArrow, &cube_arrow_vertices);
	VertexData.emplace(EPrimitiveType::Ring, &ring_vertices);
	VertexData.emplace(EPrimitiveType::Axis, &axis_vertices);

	VertexBuffers.emplace(EPrimitiveType::Cube, Renderer.CreateVertexBuffer(
		cube_vertices.data(), static_cast<int>(cube_vertices.size()) * sizeof(FVertex)));
	VertexBuffers.emplace(EPrimitiveType::Sphere, Renderer.CreateVertexBuffer(
		sphere_vertices.data(), static_cast<int>(sphere_vertices.size() * sizeof(FVertex))));
	VertexBuffers.emplace(EPrimitiveType::Triangle, Renderer.CreateVertexBuffer(
		triangle_vertices.data(), static_cast<int>(triangle_vertices.size() * sizeof(FVertex))));
	VertexBuffers.emplace(EPrimitiveType::Plane, Renderer.CreateVertexBuffer(
		plane_vertices.data(), static_cast<int>(plane_vertices.size() * sizeof(FVertex))));
	VertexBuffers.emplace(EPrimitiveType::Arrow, Renderer.CreateVertexBuffer(
		arrow_vertices.data(), static_cast<int>(arrow_vertices.size() * sizeof(FVertex))));
	VertexBuffers.emplace(EPrimitiveType::CubeArrow, Renderer.CreateVertexBuffer(
		cube_arrow_vertices.data(), static_cast<int>(cube_arrow_vertices.size() * sizeof(FVertex))));
	VertexBuffers.emplace(EPrimitiveType::Ring, Renderer.CreateVertexBuffer(
		ring_vertices.data(), static_cast<int>(ring_vertices.size() * sizeof(FVertex))));
	VertexBuffers.emplace(EPrimitiveType::Axis, Renderer.CreateVertexBuffer(
		axis_vertices.data(), static_cast<int>(axis_vertices.size() * sizeof(FVertex))));

	NumVertices.emplace(EPrimitiveType::Cube, static_cast<uint32>(cube_vertices.size()));
	NumVertices.emplace(EPrimitiveType::Sphere, static_cast<uint32>(sphere_vertices.size()));
	NumVertices.emplace(EPrimitiveType::Triangle, static_cast<uint32>(triangle_vertices.size()));
	NumVertices.emplace(EPrimitiveType::Plane, static_cast<uint32>(plane_vertices.size()));
	NumVertices.emplace(EPrimitiveType::Arrow, static_cast<uint32>(arrow_vertices.size()));
	NumVertices.emplace(EPrimitiveType::CubeArrow, static_cast<uint32>(cube_arrow_vertices.size()));
	NumVertices.emplace(EPrimitiveType::Ring, static_cast<uint32>(ring_vertices.size()));
	NumVertices.emplace(EPrimitiveType::Axis, static_cast<uint32>(axis_vertices.size()));
}

void UMeshManager::Release(URenderer &renderer)
{
	for (auto& Pair : VertexBuffers)
	{
		renderer.ReleaseVertexBuffer(Pair.second);
	}
	//TMap.Empty()
	VertexBuffers.clear();
}


TArray<FVertex>* UMeshManager::GetVertexData(EPrimitiveType Type) const
{
	return VertexData.at(Type);
}

ID3D11Buffer* UMeshManager::GetVertexBuffer(EPrimitiveType Type) const
{
	return VertexBuffers.at(Type);
}

uint32 UMeshManager::GetNumVertices(EPrimitiveType Type) const
{
	return NumVertices.at(Type);
}