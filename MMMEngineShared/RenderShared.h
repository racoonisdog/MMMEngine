#pragma once

#ifndef NOMINMAX
	#define NOMINMAX
#endif

#include <SimpleMath.h>
#include <memory>
#include <d3d11_4.h>
#include <wrl/client.h>
#include <vector>
#include <array>

#define BONE_MAXSIZE 256

namespace MMMEngine {
	enum RenderType {
		R_SHADOWMAP = 0,
		R_PREDEPTH = 1,
		R_SKYBOX = 2,
		R_GEOMETRY = 3,
		R_TRANSCULANT = 4,
		R_ADDTIVE = 5,
		R_PARTICLE = 6,
		R_POSTPROCESS = 7,
		R_UI = 8,
		R_NONE = 9,
		R_END
	};

	struct Render_CamBuffer {
		DirectX::SimpleMath::Matrix mView = DirectX::SimpleMath::Matrix::Identity;			// 카메라좌표계 변환행렬
		DirectX::SimpleMath::Matrix mProjection = DirectX::SimpleMath::Matrix::Identity;	// ndc좌표계 변환행렬
		DirectX::SimpleMath::Vector4 camPos;
	};

	struct Render_TransformBuffer
	{
		DirectX::SimpleMath::Matrix mWorld;
		DirectX::SimpleMath::Matrix mNormalMatrix;
	};

	struct Mesh_Vertex
	{
		DirectX::SimpleMath::Vector3 Pos;		// 정점 위치 정보
		DirectX::SimpleMath::Vector3 Normal;	// 노멀
		DirectX::SimpleMath::Vector3 Tangent;	// 탄젠트
		DirectX::SimpleMath::Vector2 UV;		// 텍스쳐 UV
		std::array<int, 4> BoneIndices{ -1, -1, -1, -1 };		// 버텍스와 연결된 본들의 인덱스
		std::array<float, 4> BoneWeights{ .0f, .0f, .0f, .0f };	// 각 본들의 가중치
	};

	struct Mesh_BoneBuffer
	{
		std::array<DirectX::SimpleMath::Matrix, BONE_MAXSIZE> BoneMat;

		Mesh_BoneBuffer() {
			for (auto& mat : BoneMat)
				mat = DirectX::SimpleMath::Matrix::Identity;
		}
	};

	struct MeshData {
		std::vector<std::vector<Mesh_Vertex>> vertices;
		std::vector<std::vector<UINT>> indices;
	};

	struct MeshGPU {
		std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> vertexBuffers;	// 버텍스 버퍼 (idx 메시그룹
		std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> indexBuffers;		// 인덱스 버퍼
	};
}
