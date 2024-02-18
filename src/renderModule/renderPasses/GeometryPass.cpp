﻿#include "GeometryPass.h"

#include "imgui.h"
#include "componentsModule/ModelComponent.h"
#include "renderModule/Renderer.h"
#include "assetsModule/TextureHandler.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "renderModule/Utils.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/OutlineComponent.h"
#include "componentsModule/ShaderComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/ThreadPool.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "logsModule/logger.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"

using namespace SFE::RenderModule::RenderPasses;

void GeometryPass::prepare() {
	auto curPassData = getContainer().getCurrentPassData();
	auto outlineData = mOutlineData.getCurrentPassData();
	//mStatus = RenderPreparingStatus::PREPARING;

	auto& renderData = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->getRenderData();
	currentLock = ThreadPool::instance()->addTask<WorkerType::RENDER>([this, curPassData, camFrustum = renderData.mNextCamFrustum, camPos = renderData.mCameraPos, entities = std::vector<unsigned>(), outlineData]() mutable {
		FUNCTION_BENCHMARK
		curPassData->getBatcher().drawList.clear();

		{
			FUNCTION_BENCHMARK_NAMED(octree);
			const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
			std::mutex addMtx;
			auto aabbOctrees = octreeSys->getAABBOctrees(camFrustum.generateAABB());
			ThreadPool::instance()->addBatchTasks(aabbOctrees.size(), 5, [aabbOctrees, octreeSys, &addMtx, &camFrustum, &entities](size_t it) {
				if (auto treeIt = octreeSys->getOctree(aabbOctrees[it])) {
					auto lock = treeIt->readLock();
					treeIt->forEachObjectInFrustum(camFrustum, [&entities, &camFrustum, &addMtx](const auto& obj) {
						if (FrustumModule::AABB::isOnFrustum(camFrustum, obj.pos, obj.size)) {
							std::unique_lock lock(addMtx);
							entities.emplace_back(obj.data.getID());
						}
					});
				}
				
			}).waitAll();
		}

		if (entities.empty()) {
			return;
		}
		std::ranges::sort(entities);

		{
			auto& batcher = curPassData->getBatcher();
			FUNCTION_BENCHMARK_NAMED(addedToBatcher)
			for (auto [ent, transform, modelComp, animComp] : ECSHandler::registry().getComponentsArray<TransformComponent, ModelComponent, ComponentsModule::AnimationComponent>(entities)) {
				if (!modelComp) {
					continue;
				}

				for (auto& mesh : modelComp->getModel().mMeshHandles) {
					batcher.addToDrawList(mesh.mData->mVao, mesh.mData->mVertices.size(), mesh.mData->mIndices.size(), *mesh.mMaterial, transform->getTransform(), modelComp->boneMatrices, false);
				}
			}
			batcher.sort(camPos);
		}

		{
			auto& outlineBatcher = outlineData->getBatcher();
			FUNCTION_BENCHMARK_NAMED(addedToBatcherOutline)
			for (const auto& [entity, outline, transform, modelComp, animComp] : ECSHandler::registry().getComponentsArray<OutlineComponent, TransformComponent, ModelComponent, ComponentsModule::AnimationComponent>()) {
				if (!modelComp || !transform) {
					continue;
				}

				if (std::find(entities.begin(), entities.end(), entity) == entities.end()) {
					continue;
				}
			
				for (auto& mesh : modelComp->getModel().mMeshHandles) {
					outlineBatcher.addToDrawList(mesh.mData->mVao, mesh.mData->mVertices.size(), mesh.mData->mIndices.size(), *mesh.mMaterial, transform->getTransform(), modelComp->boneMatrices, false);
				}
			}

			outlineBatcher.sort(ECSHandler::registry().getComponent<TransformComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getPos());
		}
	});
}

void GeometryPass::init() {
	if (mInited) {
		return;
	}
	mInited = true;

	mOutlineData.init(2);
	getContainer().init(2);

	// configure g-buffer framebuffer
	// ------------------------------

	glGenFramebuffers(1, &mData.mGBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, mData.mGBuffer);

	// position color buffer
	glGenTextures(1, &mData.gPosition);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, Renderer::SCR_RENDER_W, Renderer::SCR_RENDER_H, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mData.gPosition, 0);

	// normal color buffer
	glGenTextures(1, &mData.gNormal);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Renderer::SCR_RENDER_W, Renderer::SCR_RENDER_H, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mData.gNormal, 0);

	// color + specular color buffer
	glGenTextures(1, &mData.gAlbedoSpec);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gAlbedoSpec);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_RENDER_W, Renderer::SCR_RENDER_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, mData.gAlbedoSpec, 0);

	// viewPos buffer
	glGenTextures(1, &mData.gViewPosition);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gViewPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, Renderer::SCR_RENDER_W, Renderer::SCR_RENDER_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, mData.gViewPosition, 0);

	glGenTextures(1, &mData.gOutlines);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gOutlines);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_RENDER_W, Renderer::SCR_RENDER_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, mData.gOutlines, 0);

	glGenTextures(1, &mData.gLights);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gLights);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_RENDER_W, Renderer::SCR_RENDER_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, mData.gLights, 0);


	// Attach a depth texture to the framebuffer

	glGenTextures(1, &mData.gDepthTexture);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gDepthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, Renderer::SCR_RENDER_W, Renderer::SCR_RENDER_H, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mData.gDepthTexture, 0);


	// tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
	constexpr unsigned int attachments[6] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5 };
	glDrawBuffers(6, attachments);

	// create and attach depth buffer (renderbuffer)
	glGenRenderbuffers(1, &mData.rboDepth);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, mData.rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, Renderer::SCR_RENDER_W, Renderer::SCR_RENDER_H);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mData.rboDepth);

	// finally check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		LogsModule::Logger::LOG_WARNING("Framebuffer not complete!");
	}
	// tell OpenGL which color attachments we'll use (of this framebuffer) for rendering

	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	glGenFramebuffers(1, &mOData.mFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, mOData.mFramebuffer);

	// outlines buffer
	//glGenTextures(1, &mData.gOutlines);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gOutlines);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_RENDER_W, Renderer::SCR_RENDER_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mData.gOutlines, 0);

	/*AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gLights);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_RENDER_W, Renderer::SCR_RENDER_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mData.gLights, 0);*/


	constexpr unsigned int Oattachments[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, Oattachments);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		LogsModule::Logger::LOG_WARNING("Framebuffer not complete!");
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

void GeometryPass::render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) {
	if (!mInited) {
		return;
	}
	FUNCTION_BENCHMARK
	if (renderDataHandle.mRenderType == SystemsModule::RenderMode::WIREFRAME) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}


	renderDataHandle.mGeometryPassData = mData;
	if (currentLock.valid()) {
		FUNCTION_BENCHMARK_NAMED(_lock_wait)
		currentLock.wait();
	}

	const auto curPassData = getContainer().getCurrentPassData();
	const auto outlineData = mOutlineData.getCurrentPassData();
	getContainer().rotate();
	mOutlineData.rotate();
	prepare();

	glViewport(0, 0, Renderer::SCR_RENDER_W, Renderer::SCR_RENDER_H);

	glBindFramebuffer(GL_FRAMEBUFFER, mData.mGBuffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (!curPassData->getBatcher().drawList.empty()) {
		auto shaderGeometryPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer.vs", "shaders/g_buffer.fs");
		shaderGeometryPass->use();
		shaderGeometryPass->setInt("texture_diffuse1", 0);
		shaderGeometryPass->setInt("normalMap", 1);
		shaderGeometryPass->setInt("texture_specular1", 2);
		shaderGeometryPass->setBool("outline", false);
	/*	for (auto i = 0; i < 100; i++) {
			shaderGeometryPass->setMat4(("finalBonesMatrices[" + std::to_string(i) + "]").c_str(), Math::Mat4{1.f});
		}
		shaderGeometryPass->setMat4(("finalBonesMatrices[" + std::to_string(0) + "]").c_str(), Math::Mat4{{1.f,0.f,0.f, 0.f}, { 0.f,0.f,1.f, 0.f }, { 0.f,-1.f,0.f, 0.f }, {100.f,100.f,100.f,1.f}});
		shaderGeometryPass->setMat4(("finalBonesMatrices[" + std::to_string(1) + "]").c_str(), Math::Mat4{0.f});*/
		curPassData->getBatcher().flushAll(true);
	}

	if (!batcher.drawList.empty()) {
		auto lightObjectsPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer_light.vs", "shaders/g_buffer_light.fs");
		lightObjectsPass->use();
		batcher.flushAll(true);
	}

	if (!outlineData->getBatcher().drawList.empty()) {
		needClearOutlines = true;

		glBindFramebuffer(GL_FRAMEBUFFER, mOData.mFramebuffer);
		//glClear(GL_COLOR_BUFFER_BIT);

		auto g_buffer_outlines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer_outlines.vs", "shaders/g_buffer_outlines.fs");
		g_buffer_outlines->use();
		g_buffer_outlines->setMat4("PV", renderDataHandle.current.PV);

		outlineData->getBatcher().flushAll(true);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, mOData.mFramebuffer);

		auto outlineG = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_outline.vs", "shaders/g_outline.fs");
		outlineG->use();
		AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE26, GL_TEXTURE_2D, mData.gNormal);
		AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE27, GL_TEXTURE_2D, mData.gOutlines);
		AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE25, GL_TEXTURE_2D, mData.gLights);
		outlineG->setInt("gDepth", 26);
		outlineG->setInt("gOutlinesP", 27);
		outlineG->setInt("gLightsP", 25);

		Utils::renderQuad();
	}
	else {
		if (needClearOutlines) {
			needClearOutlines = false;
			glBindFramebuffer(GL_FRAMEBUFFER, mOData.mFramebuffer);
			glClear(GL_COLOR_BUFFER_BIT);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (renderDataHandle.mRenderType == SystemsModule::RenderMode::WIREFRAME) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	
}
