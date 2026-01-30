#pragma once
#include "Export.h"
#include "Renderer.h"
#include "ResourceManager.h"

#include "rttr/type"

namespace MMMEngine {
	class Texture2D;
	class Material;
	class StaticMesh;
	class MMMENGINE_API SkyRenderer : public Renderer
	{
		RTTR_ENABLE(Renderer)
		RTTR_REGISTRATION_FRIEND
	public:
		ResPtr<Texture2D>& GetSkyTexture();
		ResPtr<Texture2D>& GetSkyIrr();
		ResPtr<Texture2D>& GetSkySpecular();
		ResPtr<Texture2D>& GetSkyBrdf();

		void SetSkyTexture(ResPtr<Texture2D>& _res);
		void SetSkyIrr(ResPtr<Texture2D>& _res);
		void SetSkySpecular(ResPtr<Texture2D>& _res);
		void SetSkyBrdf(ResPtr<Texture2D>& _res);

		void Initialize() override;
		void Init() override {};
		void UnInitialize() override;

		void Render() override;
	private:
		ResPtr<Texture2D> m_pSkyTexture;
		ResPtr<Texture2D> m_pSkyIrr;
		ResPtr<Texture2D> m_pSkySpecular;
		ResPtr<Texture2D> m_pSkyBrdf;

		std::shared_ptr<Material> m_pSkyMaterial;
		ResPtr<StaticMesh> m_pMesh;

		void UpdateSRVs();
	};
}


