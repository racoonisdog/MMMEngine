#include "StaticMesh.h"
#include <rttr/registration.h>
#include "ResourceSerializer.h"
#include <RendererTools.h>
#include "RenderManager.h"

//#include <wrl/client.h>
//#include <d3d11_4.h>

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<StaticMesh>("StaticMesh")
		.constructor<>()(policy::ctor::as_std_shared_ptr)
		.property("castShadows", &StaticMesh::castShadows)
		.property("receiveShadows", &StaticMesh::receiveShadows)
		.property("meshData", &StaticMesh::meshData)
		.property("meshGroupData", &StaticMesh::meshGroupData);

	type::register_converter_func(
		[](std::shared_ptr<Resource> from, bool& ok) -> std::shared_ptr<StaticMesh>
		{
			auto result = std::dynamic_pointer_cast<StaticMesh>(from);
			ok = (result != nullptr);
			return result;
		}
	);
}

Microsoft::WRL::ComPtr<ID3D11Buffer> CreateVertexBuffer(const std::vector<MMMEngine::Mesh_Vertex>& _vertices)
{
	// 예외 확인
	if (_vertices.empty())
		return nullptr;

	// 출력할 버퍼 생성
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;

	// 버텍스 버퍼 생성
	D3D11_BUFFER_DESC bd = {};
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vbData = {};
	bd.ByteWidth = UINT(sizeof(MMMEngine::Mesh_Vertex) * _vertices.size());
	vbData.pSysMem = _vertices.data();

	MMMEngine::HR_T(MMMEngine::RenderManager::Get().GetDevice()->CreateBuffer(&bd, &vbData, buffer.GetAddressOf()));

	if(!buffer)
		throw std::runtime_error("StaticMesh::Creating VertexBuffer Failed !!");

	return buffer;
}

Microsoft::WRL::ComPtr<ID3D11Buffer> CreateIndexBuffer(const std::vector<UINT>& _indices)
{
	// 출력할 버퍼 생성
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;

	if (_indices.empty())
		return nullptr; // 안전 처리

	// 인덱스 버퍼 생성
	D3D11_BUFFER_DESC bd = {};
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.CPUAccessFlags = 0;
	bd.ByteWidth = UINT(sizeof(UINT) * _indices.size());

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = _indices.data();

	MMMEngine::HR_T(
		MMMEngine::RenderManager::Get().GetDevice()->CreateBuffer(&bd, &ibData, buffer.GetAddressOf())
	);

	if (!buffer)
		throw std::runtime_error("StaticMesh::Creating IndexBuffer Failed !!");

	return buffer;

}

bool MMMEngine::StaticMesh::LoadFromFilePath(const std::wstring& filePath)
{
	// 역직렬화
	ResourceSerializer::Get().DeSerialize_StaticMesh(this, filePath);

	// 버퍼 만들기
	for (auto& submesh : meshData.vertices) {
		Microsoft::WRL::ComPtr<ID3D11Buffer> subMeshBuffer = CreateVertexBuffer(submesh);
		gpuBuffer.vertexBuffers.push_back(subMeshBuffer);
	}
	for (auto& indices : meshData.indices) {
		Microsoft::WRL::ComPtr<ID3D11Buffer> subMeshBuffer = CreateIndexBuffer(indices);
		gpuBuffer.indexBuffers.push_back(subMeshBuffer);
		indexSizes.push_back(indices.size());
	}

	// CPU 데이터 정리
	// WARNING::필요하면 지우거나 주석처리할것 (런타임 메모리 최적화용)
	meshData.vertices.clear();
	meshData.indices.clear();

	return true;
}
