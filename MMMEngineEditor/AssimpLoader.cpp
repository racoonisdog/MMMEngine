#include "AssimpLoader.h"
#include <functional>
#include <filesystem>
#include "StringHelper.h"
#include "StaticMesh.h"
#include "SkeletalMesh.h"
#include "Material.h"
#include "Texture2D.h"

#include <rttr/registration.h>
#include "MaterialSerializer.h"
#include <RendererTools.h>
#include "RenderManager.h"
#include "ResourceSerializer.h"
#include "ProjectManager.h"
#include "ShaderInfo.h"

namespace fs = std::filesystem;

#pragma warning(disable: 4251)
#pragma warning(disable: 4834)
#pragma warning(disable: 4244)

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	rttr::registration::class_<AssimpLoader>("AssimpLoader")
		.property("exportPath", &AssimpLoader::m_exportPath)
		.method("registerModel", &AssimpLoader::RegisterModel);
}

const aiScene* MMMEngine::AssimpLoader::ImportScene(const std::wstring path, ModelType type)
{
	// 옵션 선택
	const ImportOptions opt = (type == ModelType::Static) ? StaticModelOptions() : AnimatedModelOptions();

	// 중요: 이전 scene 메모리 정리(선택이지만 안정성에 도움)
	// (Assimp::Importer는 새로 ReadFile하면 내부에서 교체되기도 하지만 명시적으로 해도 됨)
	m_importer.FreeScene();

	const aiScene* scene = m_importer.ReadFile(Utility::StringHelper::WStringToString(path), opt.assimpFlags);

	return scene;
}

DirectX::SimpleMath::Vector3 ToVector3(const aiVector3D& v)
{
	return DirectX::XMFLOAT3(v.x, v.y, v.z);
}

DirectX::SimpleMath::Vector4 ToVector4(const aiQuaternion& q)
{
	// DirectX는 (x,y,z,w) 형태로 들고 가는 게 일반적
	return DirectX::XMFLOAT4(q.x, q.y, q.z, q.w);
}

DirectX::SimpleMath::Matrix ToMatrix(const aiMatrix4x4& m)
{
	DirectX::XMFLOAT4X4 out;
	out._11 = m.a1; out._12 = m.a2; out._13 = m.a3; out._14 = m.a4;
	out._21 = m.b1; out._22 = m.b2; out._23 = m.b3; out._24 = m.b4;
	out._31 = m.c1; out._32 = m.c2; out._33 = m.c3; out._34 = m.c4;
	out._41 = m.d1; out._42 = m.d2; out._43 = m.d3; out._44 = m.d4;
	return out;
}

float SafeTicksPerSecond(double tps)
{
	return (tps != 0.0) ? (float)tps : 25.f;
}



MMMEngine::ResPtr<MMMEngine::StaticMesh> MMMEngine::AssimpLoader::ConvertStaticMesh(ModelAsset* _model)
{
	auto staticMesh = std::make_shared<StaticMesh>();

	// 메테리얼 매핑
	std::vector<ResPtr<Material>> matList;
	for (const auto& mat : _model->materials) {
		// 렌더러 메테리얼으로 변환 -> 리스트 작성
		ResPtr<Material> material = std::make_shared<Material>();
		for (const auto& [sementic, ref] : mat.textures) {
			if (!ConvertMaterial(sementic, &ref, material.get()))
				throw std::runtime_error("AssimpLoader::MaterialMapping Failed!!");
			material->SetPShader(ShaderInfo::Get().GetDefaultPShader());
			material->SetVShader(ShaderInfo::Get().GetDefaultVShader());
		}
		matList.push_back(material);
	}
	
	// SubMeshAsset → MeshData
	for (size_t i = 0; i < _model->subMeshes.size(); ++i)
	{
		const auto& sub = _model->subMeshes[i];

		// CPU 데이터
		staticMesh->meshData.vertices.push_back(sub.vertices);
		staticMesh->meshData.indices.push_back(sub.indices);

		// Material 매핑
		if (sub.materialIndex >= 0 && sub.materialIndex < _model->materials.size())
		{
			// 메테리얼 등록
			staticMesh->materials.push_back(matList[sub.materialIndex]);

			// 메테리얼:메시그룹 등록
			staticMesh->meshGroupData[sub.materialIndex].push_back((UINT)i);
		}
	}

	return staticMesh;
}

MMMEngine::ResPtr<MMMEngine::SkeletalMesh> MMMEngine::AssimpLoader::ConvertSkeletalMesh(ModelAsset* _model)
{
	auto skeletalMesh = std::make_shared<SkeletalMesh>();

	// 메테리얼 매핑
	std::vector<ResPtr<Material>> matList;
	for (const auto& mat : _model->materials) {
		// 렌더러 메테리얼으로 변환 -> 리스트 작성
		ResPtr<Material> material = std::make_shared<Material>();
		for (const auto& [sementic, ref] : mat.textures) {
			if (!ConvertMaterial(sementic, &ref, material.get()))
				throw std::runtime_error("AssimpLoader::MaterialMapping Failed!!");
			material->SetPShader(ShaderInfo::Get().GetDefaultPShader());
			material->SetVShader(ShaderInfo::Get().GetDefaultVShader());
		}

		material->SetVShader(ShaderInfo::Get().GetDefaultVShader());
		material->SetPShader(ShaderInfo::Get().GetDefaultPShader());

		matList.push_back(material);
	}

	// SubMeshAsset → MeshData
	for (size_t i = 0; i < _model->subMeshes.size(); ++i)
	{
		const auto& sub = _model->subMeshes[i];

		// CPU 데이터
		skeletalMesh->meshData.vertices.push_back(sub.vertices);
		skeletalMesh->meshData.indices.push_back(sub.indices);

		// Material 매핑
		if (sub.materialIndex >= 0 && sub.materialIndex < _model->materials.size())
		{
			// 메테리얼 등록
			skeletalMesh->materials.push_back(matList[sub.materialIndex]);

			// 메테리얼:메시그룹 등록
			skeletalMesh->meshGroupData[sub.materialIndex].push_back((UINT)i);
		}
	}

	// 본 매트릭스, 오프셋 전달
	if (!_model->skin.bones.empty())
	{
		for (size_t i = 0; i < _model->skin.bones.size() && i < BONE_MAXSIZE; ++i)
		{
			const auto& bone = _model->skin.bones[i];
			skeletalMesh->boneBuffer.BoneMat[i] = DirectX::SimpleMath::Matrix::Identity; // 애니메이션 시점에서 갱신됨
			skeletalMesh->offsetBuffer.BoneMat[i] = bone.offset; // 바인드 포즈 오프셋
		}
	}

	// 애니메이션 클립 전달

	return skeletalMesh;
}

bool MMMEngine::AssimpLoader::ConvertMaterial(const TextureSemantic _sementic, const TextureRef* _ref, Material* _out)
{
	ResPtr<Texture2D> resource;
	std::wstring property;

	static const std::unordered_map<TextureSemantic, std::wstring> semanticMap = {
	{TextureSemantic::BaseColor, L"_albedo"},
	{TextureSemantic::Normal,    L"_normal"},
	{TextureSemantic::Metallic,  L"_metallic"},
	{TextureSemantic::Roughness, L"_roughness"},
	{TextureSemantic::AO,        L"_ambientOcclusion"},
	{TextureSemantic::Emissive,  L"_emissive"},
	{TextureSemantic::Opacity,   L"_opacity"}
	};

	auto it = semanticMap.find(_sementic);
	if (it == semanticMap.end())
		return false;

	resource = ResourceManager::Get().Load<Texture2D>(Utility::StringHelper::StringToWString(_ref->path));
	if (!resource)
		return false;

	_out->AddProperty(it->second, resource);
	return true;
}

int MMMEngine::AssimpLoader::BuildNodeRecursive(const aiNode* node, int parentIndex, NodeTreeAsset& out)
{
	const int myIndex = static_cast<int>(out.nodes.size());

	NodeData nd;
	nd.name = node->mName.C_Str();
	nd.parentIndex = parentIndex;
	nd.bindLocal = ToMatrix(node->mTransformation);
	out.nodes.push_back(std::move(nd));

	if (out.nodeIndexByName.find(out.nodes[myIndex].name) == out.nodeIndexByName.end())
		out.nodeIndexByName.emplace(out.nodes[myIndex].name, myIndex);

	out.nodes[myIndex].children.reserve(node->mNumChildren);

	for (unsigned i = 0; i < node->mNumChildren; ++i)
	{
		int childIndex = BuildNodeRecursive(node->mChildren[i], myIndex, out);
		if (childIndex < 0) return -1;
		out.nodes[myIndex].children.push_back(childIndex);
	}
	return myIndex;
}

bool MMMEngine::AssimpLoader::ExtractNodeTree(const aiScene* scene, NodeTreeAsset& out)
{
	out.nodes.clear();
	out.nodeIndexByName.clear();
	out.rootIndex = -1;

	out.rootIndex = BuildNodeRecursive(scene->mRootNode, -1, out);
	if (out.rootIndex < 0)
		return false;
	return true;
}

void MMMEngine::AssimpLoader::FillMeshToNodeRecursive(const aiNode* node, int nodeIndex, std::vector<int>& meshToNode)
{
	if (!node) return;

	for (unsigned i = 0; i < node->mNumMeshes; ++i)
	{
		unsigned meshIdx = node->mMeshes[i];
		if (meshIdx < meshToNode.size())
		{
			if (meshToNode[meshIdx] == -1)
				meshToNode[meshIdx] = nodeIndex;
		}
	}
}

bool MMMEngine::AssimpLoader::ExtractMeshNodeLinks(const aiScene* scene, const NodeTreeAsset& nodes, std::vector<int>& outMeshToNode)
{
	outMeshToNode.clear();

	if (!scene || !scene->mRootNode) return false;

	outMeshToNode.resize(scene->mNumMeshes, -1);

	std::function<void(const aiNode*)> walk = [&](const aiNode* n)
		{
			if (!n) return;

			int nodeIndex = -1;
			auto it = nodes.nodeIndexByName.find(n->mName.C_Str());
			if (it != nodes.nodeIndexByName.end())
				nodeIndex = it->second;

			if (nodeIndex != -1)
			{
				for (unsigned i = 0; i < n->mNumMeshes; ++i)
				{
					unsigned meshIdx = n->mMeshes[i];
					if (meshIdx < outMeshToNode.size() && outMeshToNode[meshIdx] == -1)
						outMeshToNode[meshIdx] = nodeIndex;
				}
			}
			for (unsigned i = 0; i < n->mNumChildren; ++i)
				walk(n->mChildren[i]);
		};
	walk(scene->mRootNode);
	return true;
}

bool MMMEngine::AssimpLoader::ExtractMeshBasicInfo(const aiScene* scene, std::vector<MeshBasicInfo>& outInfos)
{
	if (!scene) return false;

	outInfos.clear();
	outInfos.resize(scene->mNumMeshes);

	for (unsigned i = 0; i < scene->mNumMeshes; ++i)
	{
		const aiMesh* mesh = scene->mMeshes[i];
		if (!mesh) continue;

		MeshBasicInfo& info = outInfos[i];
		info.vertexCount = mesh->mNumVertices;

		UINT indexCount = 0;
		for (unsigned f = 0; f < mesh->mNumFaces; ++f)
			indexCount += mesh->mFaces[f].mNumIndices;

		info.indexCount = indexCount;
	}
	return true;
}

bool MMMEngine::AssimpLoader::ExtractSubMeshes(const aiScene* scene, const std::vector<int>& meshToNodeIndex, std::vector<SubMeshAsset>& outSubMeshes)
{
	outSubMeshes.clear();
	if (!scene || !scene->mMeshes)
		return false;

	const unsigned meshCount = scene->mNumMeshes;

	const bool hasMeshToNode = (meshToNodeIndex.size() == meshCount);

	outSubMeshes.resize(meshCount);

	for (unsigned mi = 0; mi < meshCount; ++mi)
	{
		const aiMesh* mesh = scene->mMeshes[mi];
		if (!mesh)
			continue;

		SubMeshAsset& sm = outSubMeshes[mi];

		sm.materialIndex = static_cast<int>(mesh->mMaterialIndex);
		sm.nodeIndex = hasMeshToNode ? meshToNodeIndex[mi] : -1;

		sm.vertices.resize(mesh->mNumVertices);

		const bool hasNormals = mesh->HasNormals();
		const bool hasUV0 = mesh->HasTextureCoords(0);
		const bool hasTangents = (mesh->mTangents != nullptr);
		const bool hasBitangents = (mesh->mBitangents != nullptr);

		for (unsigned vi = 0; vi < mesh->mNumVertices; ++vi)
		{
			Mesh_Vertex v; // boneIDs/weights는 기본 0

			// Pos
			v.Pos = DirectX::XMFLOAT3(mesh->mVertices[vi].x,
				mesh->mVertices[vi].y,
				mesh->mVertices[vi].z);

			// Normal (없을 수도 있음)
			if (hasNormals)
			{
				v.Normal = DirectX::XMFLOAT3(mesh->mNormals[vi].x,
					mesh->mNormals[vi].y,
					mesh->mNormals[vi].z);
			}
			else
			{
				v.Normal = DirectX::XMFLOAT3(0.f, 1.f, 0.f);
			}

			if (hasUV0)
			{
				v.UV = DirectX::XMFLOAT2(mesh->mTextureCoords[0][vi].x,
					mesh->mTextureCoords[0][vi].y);
			}
			else
			{
				v.UV = DirectX::XMFLOAT2(0.f, 0.f);
			}

			if (hasTangents)
			{
				v.Tangent = DirectX::XMFLOAT3(mesh->mTangents[vi].x,
					mesh->mTangents[vi].y,
					mesh->mTangents[vi].z);
			}
			else
			{
				v.Tangent = DirectX::XMFLOAT3(0.f, 0.f, 0.f);
			}

			sm.vertices[vi] = v;
		}
		UINT totalIndexCount = 0;
		for (unsigned fi = 0; fi < mesh->mNumFaces; ++fi)
			totalIndexCount += mesh->mFaces[fi].mNumIndices;

		sm.indices.clear();
		sm.indices.reserve(totalIndexCount);

		for (unsigned fi = 0; fi < mesh->mNumFaces; ++fi)
		{
			const aiFace& face = mesh->mFaces[fi];
			for (unsigned k = 0; k < face.mNumIndices; ++k)
			{
				sm.indices.push_back(static_cast<UINT>(face.mIndices[k]));
			}
		}
	}
	return true;
}

bool MMMEngine::AssimpLoader::GetTexturePath(const aiMaterial* mat, aiTextureType type, std::string& outPath)
{
	if (!mat) return false;

	aiString str;
	if (mat->GetTextureCount(type) == 0) return false;
	if (mat->GetTexture(type, 0, &str) != AI_SUCCESS) return false;

	fs::path filename(str.C_Str());
	filename.filename();

	outPath = filename.filename().string();
	return !outPath.empty();
}

std::string MMMEngine::AssimpLoader::ResolveTexturePath(const std::string& modelDir, const std::string& raw)
{
	if (!raw.empty() && raw[0] == '*')
		return raw;
	std::filesystem::path p(raw);
	if (p.is_absolute())
		return p.lexically_normal().string();
	std::filesystem::path base(modelDir);
	return (base / p).lexically_normal().string();
}

bool MMMEngine::AssimpLoader::ExtractMaterials(const aiScene* scene, const std::string& textureDir, std::vector<MaterialAsset>& outMaterials)
{
	outMaterials.clear();
	if (!scene || !scene->mMaterials) return false;

	outMaterials.resize(scene->mNumMaterials);

	for (unsigned mi = 0; mi < scene->mNumMaterials; ++mi)
	{
		const aiMaterial* mat = scene->mMaterials[mi];
		MaterialAsset& dst = outMaterials[mi];

		auto put = [&](TextureSemantic sem, const std::string& rawPath, bool srgb)
			{
				if (rawPath.empty()) return;
				TextureRef ref;
				ref.path = ResolveTexturePath(textureDir, rawPath);
				ref.srgb = srgb;
				dst.textures[sem] = std::move(ref);
			};

		std::string raw;

		// BaseColor
		if (GetTexturePath(mat, aiTextureType_BASE_COLOR, raw) || GetTexturePath(mat, aiTextureType_DIFFUSE, raw))
			put(TextureSemantic::BaseColor, raw, true);

		// Normal (fallback: HEIGHT로 들어오는 경우)
		raw.clear();
		if (GetTexturePath(mat, aiTextureType_NORMALS, raw) || GetTexturePath(mat, aiTextureType_HEIGHT, raw))
			put(TextureSemantic::Normal, raw, false);

		// Metallic
		raw.clear();
		if (GetTexturePath(mat, aiTextureType_METALNESS, raw))
			put(TextureSemantic::Metallic, raw, false);

		// Roughness
		raw.clear();
		if (GetTexturePath(mat, aiTextureType_DIFFUSE_ROUGHNESS, raw))
			put(TextureSemantic::Roughness, raw, false);

		// AO
		raw.clear();
		if (GetTexturePath(mat, aiTextureType_AMBIENT_OCCLUSION, raw))
			put(TextureSemantic::AO, raw, false);

		// Emissive
		raw.clear();
		if (GetTexturePath(mat, aiTextureType_EMISSIVE, raw))
			put(TextureSemantic::Emissive, raw, true);

		// Opacity
		raw.clear();
		if (GetTexturePath(mat, aiTextureType_OPACITY, raw))
			put(TextureSemantic::Opacity, raw, false);
	}
	return true;
}

void MMMEngine::AssimpLoader::AddBoneWeight(Mesh_Vertex& v, int boneIndex, float w)
{
	if (w <= 0.f) return;
	for (int i = 0; i < 4; ++i)
	{
		if (v.BoneWeights[i] == 0.0f)
		{
			v.BoneWeights[i] = boneIndex;
			v.BoneWeights[i] = w;
			return;
		}
	}

	int minIdx = 0;
	float minW = v.BoneWeights[0];
	for (int i = 1; i < 4; ++i) {
		minW = v.BoneWeights[i];
		minIdx = i;
	}
	if (w > minW)
	{
		v.BoneIndices[minIdx] = boneIndex;
		v.BoneWeights[minIdx] = w;
	}
}

void MMMEngine::AssimpLoader::NormalizeWeights(Mesh_Vertex& v)
{
	float sum = 0.f;
	for (int i = 0; i < 4; ++i) sum += v.BoneWeights[i];
	if (sum <= 0.f) return;
	float inv = 1.f / sum;
	for (int i = 0; i < 4; ++i) v.BoneWeights[i] *= inv;
}

bool MMMEngine::AssimpLoader::ExtractSkinning(const aiScene* scene, const NodeTreeAsset& nodes, std::vector<SubMeshAsset>& inoutSubMeshes, SkinningAsset& outSkinning)
{
	outSkinning.boneIndexByName.clear();
	outSkinning.bones.clear();

	if (!scene || !scene->mMeshes) return false;
	if (inoutSubMeshes.size() != scene->mNumMeshes) return false; // mesh 1:1 전제(지금 구조)

	auto getOrCreateBoneIndex = [&](const std::string& boneName, const aiMatrix4x4& offsetM) -> int
		{
			auto it = outSkinning.boneIndexByName.find(boneName);
			if (it != outSkinning.boneIndexByName.end())
				return it->second;

			int newIndex = (int)outSkinning.bones.size();
			outSkinning.boneIndexByName.emplace(boneName, newIndex);

			BoneAsset b;
			b.name = boneName;
			auto nit = nodes.nodeIndexByName.find(boneName);
			if (nit != nodes.nodeIndexByName.end())
				b.nodeIndex = nit->second;
			else
				b.nodeIndex = -1;
			b.offset = ToMatrix(offsetM);
			outSkinning.bones.push_back(std::move(b));
			return newIndex;
		};

	// 모든 메시에 대해
	for (unsigned mi = 0; mi < scene->mNumMeshes; ++mi)
	{
		const aiMesh* mesh = scene->mMeshes[mi];
		if (!mesh) continue;

		SubMeshAsset& sm = inoutSubMeshes[mi];
		if (sm.vertices.size() != mesh->mNumVertices) return false;

		if (mesh->mNumBones == 0)
		{
			sm.skinned = false;
			continue;
		}

		sm.skinned = true;

		// aiBone 단위로 weight를 vertex에 누적
		for (unsigned bi = 0; bi < mesh->mNumBones; ++bi)
		{
			const aiBone* bone = mesh->mBones[bi];
			if (!bone) continue;

			std::string boneName = bone->mName.C_Str();
			int boneIndex = getOrCreateBoneIndex(boneName, bone->mOffsetMatrix);

			for (unsigned wi = 0; wi < bone->mNumWeights; ++wi)
			{
				const aiVertexWeight& vw = bone->mWeights[wi];
				unsigned vId = vw.mVertexId;
				float w = vw.mWeight;

				if (vId < sm.vertices.size())
					AddBoneWeight(sm.vertices[vId], boneIndex, w);
			}
		}

		// 정규화
		for (auto& v : sm.vertices)
			NormalizeWeights(v);
	}

	return true;
}

bool MMMEngine::AssimpLoader::ExtractAnimationClips(const aiScene* scene, const NodeTreeAsset& nodes, std::vector<AnimationClipAsset>& outClips)
{
	outClips.clear();
	if (!scene) return false;
	if (scene->mNumAnimations == 0 || !scene->mAnimations) return true;

	outClips.reserve(scene->mNumAnimations);

	for (unsigned ai = 0; ai < scene->mNumAnimations; ++ai)
	{
		const aiAnimation* anim = scene->mAnimations[ai];
		if (!anim) continue;

		AnimationClipAsset clip;
		clip.name = anim->mName.C_Str();
		clip.ticksPerSecond = SafeTicksPerSecond(anim->mTicksPerSecond);

		clip.durationSec = (float)anim->mDuration / clip.ticksPerSecond;

		clip.tracks.reserve(anim->mNumChannels);

		for (unsigned ci = 0; ci < anim->mNumChannels; ++ci)
		{
			const aiNodeAnim* ch = anim->mChannels[ci];
			if (!ch) continue;

			const char* nodeName = ch->mNodeName.C_Str();

			auto it = nodes.nodeIndexByName.find(nodeName);
			if (it == nodes.nodeIndexByName.end())
			{
				continue;
			}
			NodeAnimTrack track;
			track.nodeIndex = it->second;

			if (ch->mNumPositionKeys > 0 && ch->mPositionKeys)
			{
				track.posKeys.reserve(ch->mNumPositionKeys);
				for (unsigned k = 0; k < ch->mNumPositionKeys; ++k)
				{
					VecKey vk;
					vk.timeSec = (float)ch->mPositionKeys[k].mTime / clip.ticksPerSecond;
					vk.value = ToVector3(ch->mPositionKeys[k].mValue);
					track.posKeys.push_back(vk);
				}
			}
			if (ch->mNumRotationKeys > 0 && ch->mRotationKeys)
			{
				track.rotKeys.reserve(ch->mNumRotationKeys);
				for (unsigned k = 0; k < ch->mNumRotationKeys; ++k)
				{
					QuatKey qk;
					qk.timeSec = (float)ch->mRotationKeys[k].mTime / clip.ticksPerSecond;
					qk.value = ToVector4(ch->mRotationKeys[k].mValue);
					track.rotKeys.push_back(qk);
				}
			}
			if (ch->mNumScalingKeys > 0 && ch->mScalingKeys)
			{
				track.scaleKeys.reserve(ch->mNumScalingKeys);
				for (unsigned k = 0; k < ch->mNumScalingKeys; ++k)
				{
					VecKey sk;
					sk.timeSec = (float)ch->mScalingKeys[k].mTime / clip.ticksPerSecond;
					sk.value = ToVector3(ch->mScalingKeys[k].mValue);
					track.scaleKeys.push_back(sk);
				}
			}
			clip.tracks.push_back(std::move(track));
		}
		outClips.push_back(std::move(clip));
	}
	return true;
}

bool MMMEngine::AssimpLoader::ImportModel(const std::wstring& path, ModelType type, ModelAsset& out)
{
	out = ModelAsset{};
	out.type = type;

	fs::path localPath(path);
	fs::path realPath;

	if (localPath.is_relative()) {
		realPath = ResourceManager::Get().GetCurrentRootPath();
		realPath = realPath / localPath;
	}
	else
		realPath = localPath;
	

	const aiScene* scene = ImportScene(realPath.wstring(), type);
	if (!scene) return false;
	if (!ExtractNodeTree(scene, out.nodeTree)) return false;
	std::vector<int> meshToNode;
	if (!ExtractMeshNodeLinks(scene, out.nodeTree, meshToNode)) return false;
	if (!ExtractSubMeshes(scene, meshToNode, out.subMeshes)) return false;
	std::string modelDir;
	std::string texDir;
	try
	{
		modelDir = realPath.has_parent_path() ? realPath.parent_path().string() : std::string{};
		texDir = localPath.has_parent_path() ? localPath.parent_path().string() : std::string{};
	}
	catch (...)
	{
		modelDir.clear();
		texDir.clear();
	}
	if (!ExtractMaterials(scene, texDir, out.materials)) return false;
	if (type == ModelType::Animated)
	{
		if (!ExtractSkinning(scene, out.nodeTree, out.subMeshes, out.skin)) return false;
		if (!ExtractAnimationClips(scene, out.nodeTree, out.clips)) return false;
	}
	return true;
}

void MMMEngine::AssimpLoader::RegisterModel(const std::wstring path, ModelType type)
{
	if (path.empty())
		return;

	ModelAsset model;

	if (!ImportModel(path, type, model))
		return;

	ResPtr<StaticMesh> staticMesh;
	ResPtr<SkeletalMesh> skeletalMesh;

	fs::path fPath(path);
	std::wstring filename;

	if (fPath.has_stem()) {
		filename = fPath.stem().wstring();
	}

	switch (type)
	{
	case MMMEngine::ModelType::Static:
		staticMesh = ConvertStaticMesh(&model);
		ResourceSerializer::Get().Serialize_StaticMesh(staticMesh.get(), m_exportPath, filename);
		break;
	case MMMEngine::ModelType::Animated:
		skeletalMesh = ConvertSkeletalMesh(&model);
		//TODO::Skeletalmesh 직렬화
		//ResourceSerializer::Get().Serialize_StaticMesh(skeletalMesh.get(), m_exportPath);
		break;
	default:
		return;
		break;
	}
}

//const MMMEngine::AssimpLoader::ModelAsset* MMMEngine::AssimpLoader::GetModel(const std::string& id) const
//{
//	/*auto it = sModelCache.find(id);
//	if (it == sModelCache.end()) return nullptr;
//	return &it->second;*/
//}
