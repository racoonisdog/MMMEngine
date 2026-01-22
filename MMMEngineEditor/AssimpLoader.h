#pragma once
#ifndef NOMINMAX
	#define NOMINMAX 
#endif // !NOMINMAX

#include "Singleton.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <unordered_map>
#include <string>
#include <SimpleMath.h>
#include "RenderShared.h"
#include "ResourceManager.h"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace MMMEngine {
	enum class ModelType {
		Static,
		Animated
	};

	class StaticMesh;
	class SkeletalMesh;
	class AssimpLoader : public Utility::Singleton<MMMEngine::AssimpLoader>
	{
	public:
		struct ImportOptions {
			ModelType type;
			unsigned int assimpFlags;
		};

		struct NodeData {
			std::string name;
			int parentIndex = -1;
			std::vector<int> children;
			DirectX::SimpleMath::Matrix bindLocal;
		};

		struct NodeTreeAsset {
			std::vector<NodeData> nodes;
			std::unordered_map<std::string, int> nodeIndexByName;
			int rootIndex = -1;
		};

		struct MeshBasicInfo
		{
			UINT vertexCount = 0;
			UINT indexCount = 0;
		};

		struct SubMeshAsset
		{
			std::vector<Mesh_Vertex> vertices;
			std::vector<UINT> indices;
			int nodeIndex = -1;
			int materialIndex = -1;
			bool skinned = false;
		};

		enum class TextureSemantic {
			BaseColor, Normal, Metallic, Roughness, AO, Emissive, Opacity
		};
		struct TextureRef {
			std::string path; // resolve된 경로(또는 embedded 표기)
			bool srgb = false;
		};

		struct MaterialAsset {
			std::unordered_map<TextureSemantic, TextureRef> textures;
		};

		struct BoneAsset
		{
			std::string name;
			int nodeIndex = -1;
			DirectX::SimpleMath::Matrix offset;
		};

		struct SkinningAsset
		{
			std::unordered_map<std::string, int> boneIndexByName;
			std::vector<BoneAsset> bones;
		};

		struct VecKey
		{
			float timeSec = 0.f;
			DirectX::SimpleMath::Vector3 value{};
		};
		struct QuatKey
		{
			float timeSec = 0.f;
			DirectX::SimpleMath::Vector4 value{};
		};
		struct NodeAnimTrack
		{
			int nodeIndex = -1;
			std::vector<VecKey> posKeys;
			std::vector<QuatKey> rotKeys;
			std::vector<VecKey> scaleKeys;
		};
		struct AnimationClipAsset
		{
			std::string name;
			float durationSec = 0.f;
			float ticksPerSecond = 25.f;
			std::vector<NodeAnimTrack> tracks;
		};
		struct ModelAsset {
			NodeTreeAsset nodeTree;
			std::vector<SubMeshAsset> subMeshes;
			std::vector<MaterialAsset> materials;
			SkinningAsset skin;
			std::vector<AnimationClipAsset> clips;
			ModelType type = ModelType::Static;
		};

		// 모델 등록하기 (경로, 타입)
		void RegisterModel(const std::wstring path, ModelType type);

		// 출력 경로 (상대경로만 넣어야함, 절대경로 변경시 문의)
		std::wstring m_exportPath = L"Assets/";
	protected:
		const aiScene* ImportScene(std::wstring path, ModelType type);
		bool ExtractNodeTree(const aiScene* scene, NodeTreeAsset& out);
		bool ExtractMeshNodeLinks(const aiScene* scene, const NodeTreeAsset& nodes, std::vector<int>& outMeshToNode);
		bool ExtractMeshBasicInfo(const aiScene* scene, std::vector<MeshBasicInfo>& outInfos);
		bool ExtractSubMeshes(const aiScene* scene, const std::vector<int>& meshToNodeIndex, std::vector<SubMeshAsset>& outSubMeshes);
		bool ExtractMaterials(const aiScene* scene, const std::string& modelDir, std::vector<MaterialAsset>& outMaterials);
		bool ExtractSkinning(const aiScene* scene, const NodeTreeAsset& nodes, std::vector<SubMeshAsset>& inoutSubMeshes, SkinningAsset& outSkinning);
		bool ExtractAnimationClips(const aiScene* scene, const NodeTreeAsset& nodes, std::vector<AnimationClipAsset>& outClips);
		bool ImportModel(const std::wstring& path, ModelType type, ModelAsset& out);

		//const ModelAsset* GetModel(const std::string& id) const;
	private:

		Assimp::Importer m_importer;
		Assimp::Importer& GetImporter() { return m_importer; };

		ResPtr<StaticMesh> ConvertStaticMesh(ModelAsset* _model);
		ResPtr<SkeletalMesh> ConvertSkeletalMesh(ModelAsset* _model);
		bool ConvertMaterial(const TextureSemantic _sementic, const TextureRef* _ref, Material* _out);

		int BuildNodeRecursive(const aiNode* node, int parentIndex, NodeTreeAsset& out);
		void FillMeshToNodeRecursive(const aiNode* node, int nodeIndex, std::vector<int>& meshToNode);
		bool GetTexturePath(const aiMaterial* mat, aiTextureType type, std::string& outPath);
		std::string ResolveTexturePath(const std::string& modelDir, const std::string& raw);
		void AddBoneWeight(Mesh_Vertex& v, int boneIndex, float w);
		void NormalizeWeights(Mesh_Vertex& v);
		inline ImportOptions StaticModelOptions() {
			return{
				ModelType::Static,
				aiProcess_Triangulate |
				aiProcess_GenNormals |
				aiProcess_CalcTangentSpace |
				aiProcess_ConvertToLeftHanded |
				aiProcess_PreTransformVertices
			};
		}
		inline ImportOptions AnimatedModelOptions() {
			return{
				ModelType::Animated,
				aiProcess_Triangulate |
				aiProcess_GenNormals |
				aiProcess_CalcTangentSpace |
				aiProcess_ConvertToLeftHanded
			};
		}
		//std::unordered_map<std::string, ModelAsset>sModelCache;
	};
}

#pragma warning(pop)