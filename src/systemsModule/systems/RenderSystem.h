﻿#pragma once

#include <vector>

#include "core/BoundingVolume.h"
#include "systemsModule/SystemBase.h"
#include "renderModule/RenderPass.h"
#include "renderModule/renderPasses/CascadedShadowPass.h"
#include "renderModule/renderPasses/GeometryPass.h"
#include "renderModule/renderPasses/PointLightPass.h"
#include "renderModule/renderPasses/SSAOPass.h"

namespace SFE {
	namespace Render {
		class Renderer;
	}
}

namespace SFE::SystemsModule {

	enum class RenderMode {
		DEFAULT,
		WIREFRAME,
		NORMALS,
	};

	struct RenderMatrices {
		Math::Mat4 projection = {};
		Math::Mat4 view = {};
		Math::Mat4 PV = {};
	};

	struct RenderData {
		FrustumModule::Frustum mCamFrustum;
		FrustumModule::Frustum mNextCamFrustum;

		RenderMatrices current;
		RenderMatrices next;

		Math::Vec3 mCameraPos = {};
		Math::Vec3 mViewDir = {};
		Math::Vec3 mNextCameraPos = {};
		Math::Vec3 mNextViewDir = {};

		ProjectionModule::PerspectiveProjection cameraProjection = {};
		ProjectionModule::PerspectiveProjection nextCameraProjection =  {};

		Render::RenderPasses::CascadedShadowPass::Data mCascadedShadowsPassData;
		Render::RenderPasses::PointLightPass::Data mPointPassData;
		Render::RenderPasses::GeometryPass::Data mGeometryPassData;
		Render::RenderPasses::SSAOPass::Data mSSAOPassData;

		RenderMode mRenderType = RenderMode::DEFAULT;
	};

	class RenderSystem : public ecss::System {
	public:
		RenderSystem(Render::Renderer* renderer);
		~RenderSystem() override;

		void update(float_t dt) override;
		void debugUpdate(float dt) override;
		inline void setRenderType(RenderMode type) { mRenderData.mRenderType = type; }
		inline RenderData& getRenderData() { return mRenderData; }

		bool isShadowsDebugData() const {
			return mShadowsDebugDataDraw;
		}

	private:
		bool mGeometryPassDataWindow = false;
		bool mShadowsDebugDataDraw = false;

		template<typename PassType>
		inline void addRenderPass();

		Render::Renderer* mRenderer = nullptr;

		RenderData mRenderData;
		std::vector<Render::RenderPass*> mRenderPasses;

		Render::Buffer cameraMatricesUBO{Render::UNIFORM_BUFFER};
	};
}
