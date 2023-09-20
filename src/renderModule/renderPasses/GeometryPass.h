﻿#pragma once
#include <mutex>
#include <thread>
#include <vector>

#include "renderModule/RenderPass.h"

namespace Engine::RenderModule::RenderPasses {
	class GeometryPass : public RenderPass {
	public:
		struct Data {
			unsigned int mGBuffer = 0;
			unsigned int gPosition = 0;
			unsigned int gViewPosition = 0;
			unsigned int gOutlines = 0;
			unsigned int gNormal = 0;
			unsigned int gAlbedoSpec = 0;
			unsigned int rboDepth = 0;
			unsigned int gDepthTexture = 0;
		};

		struct OutlinesData {
			unsigned int mFramebuffer = 0;
			unsigned int depth = 0;

		};
		void init();
		void render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) override;
	private:
		bool mInited = false;
		Data mData;
		OutlinesData mOData;
		bool needClearOutlines = false;
		std::vector<std::thread> threads;
		std::mutex mtx;
	};
}
