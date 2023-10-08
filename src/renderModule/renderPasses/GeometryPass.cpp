﻿#include "GeometryPass.h"

#include "componentsModule/ModelComponent.h"
#include "renderModule/Renderer.h"
#include "assetsModule/TextureHandler.h"
#include "renderModule/Utils.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/OutlineComponent.h"
#include "componentsModule/ShaderComponent.h"
#include "core/ECSHandler.h"
#include "core/ThreadPool.h"
#include "..\..\ecss\Registry.h"
#include "logsModule/logger.h"
#include "systemsModule/CameraSystem.h"
#include "systemsModule/RenderSystem.h"
#include "systemsModule/SystemManager.h"

using namespace Engine::RenderModule::RenderPasses;

void GeometryPass::init() {
	if (mInited) {
		return;
	}
	mInited = true;

	// configure g-buffer framebuffer
	// ------------------------------

	glGenFramebuffers(1, &mData.mGBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, mData.mGBuffer);

	// position color buffer
	glGenTextures(1, &mData.gPosition);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mData.gPosition, 0);

	// normal color buffer
	glGenTextures(1, &mData.gNormal);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mData.gNormal, 0);

	// color + specular color buffer
	glGenTextures(1, &mData.gAlbedoSpec);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gAlbedoSpec);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, mData.gAlbedoSpec, 0);

	// color + specular color buffer
	glGenTextures(1, &mData.gViewPosition);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gViewPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, mData.gViewPosition, 0);

	glGenTextures(1, &mData.gOutlines);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gOutlines);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, mData.gOutlines, 0);

	glGenTextures(1, &mData.gLights);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gLights);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, mData.gLights, 0);


	// Attach a depth texture to the framebuffer

	glGenTextures(1, &mData.gDepthTexture);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gDepthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
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
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT);
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mData.gOutlines, 0);

	/*AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gLights);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mData.gLights, 0);*/

	// tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
	constexpr unsigned int Oattachments[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, Oattachments);

	// finally check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		LogsModule::Logger::LOG_WARNING("Framebuffer not complete!");
	}
	// tell OpenGL which color attachments we'll use (of this framebuffer) for rendering

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

void GeometryPass::render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) {
	if (!mInited) {
		return;
	}
	if (renderDataHandle.mRenderType == SystemsModule::RenderMode::WIREFRAME) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}


	renderDataHandle.mGeometryPassData = mData;
	auto batcher = renderer->getBatcher();

	auto future = ThreadPool::instance()->addRenderTask([&](std::mutex& poolMutex) {
		for (auto [isDraw, modelComp, transform] : ECSHandler::registry()->getComponentsArray<const IsDrawableComponent, ModelComponent, const TransformComponent>()) {
			if (!&isDraw) {
				continue;
			}

			if (!&modelComp) {
				continue;
			}

			auto& model = modelComp.getModel();
			for (auto& mesh : model.mMeshHandles) {
				if (mesh.mBounds && mesh.mBounds->isOnFrustum(renderDataHandle.mCamFrustum, transform.getTransform())) {
					batcher->addToDrawList(mesh.mData->mVao, mesh.mData->mVertices.size(), mesh.mData->mIndices.size(), *mesh.mMaterial, transform.getTransform(), false);
				}
			}
		}
	});
		
	

	glViewport(0, 0, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT);


	glBindFramebuffer(GL_FRAMEBUFFER, mData.mGBuffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	auto shaderGeometryPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer.vs", "shaders/g_buffer.fs");
	shaderGeometryPass->use();
	shaderGeometryPass->setMat4("P", renderDataHandle.mProjection);
	shaderGeometryPass->setMat4("V", renderDataHandle.mView);
	shaderGeometryPass->setMat4("PV", renderDataHandle.mProjection * renderDataHandle.mView);
	shaderGeometryPass->setInt("texture_diffuse1", 0);
	shaderGeometryPass->setInt("normalMap", 1);
	shaderGeometryPass->setBool("outline", false);

	
	
	
	auto curCam = ECSHandler::systemManager()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera();
	future.get();

	batcher->flushAll(true, ECSHandler::registry()->getComponent<TransformComponent>(curCam)->getPos());

	auto lightObjectsPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer_light.vs", "shaders/g_buffer_light.fs");
	lightObjectsPass->use();
	lightObjectsPass->setMat4("PV", renderDataHandle.mProjection * renderDataHandle.mView);

	for (auto [lightSource, modelComp, transform] : ECSHandler::registry()->getComponentsArray<LightSourceComponent, ModelComponent, TransformComponent>()) {
		if (&modelComp) {
			auto& model = modelComp.getModel();
			for (auto& mesh : model.mMeshHandles) {
				if (mesh.mBounds->isOnFrustum(renderDataHandle.mCamFrustum, transform.getTransform())) {
					batcher->addToDrawList(mesh.mData->mVao, mesh.mData->mVertices.size(), mesh.mData->mIndices.size(), *mesh.mMaterial, transform.getTransform(), false);
				}
			}
		}
	}

	
	batcher->flushAll(true, ECSHandler::registry()->getComponent<TransformComponent>(ECSHandler::systemManager()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera())->getPos());

	
	auto outlineNodes = ECSHandler::registry()->getComponentsArray<OutlineComponent>();
	if (!outlineNodes.empty()) {
		needClearOutlines = true;

		glBindFramebuffer(GL_FRAMEBUFFER, mOData.mFramebuffer);
		//glClear(GL_COLOR_BUFFER_BIT);

		auto g_buffer_outlines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer_outlines.vs", "shaders/g_buffer_outlines.fs");
		g_buffer_outlines->use();
		g_buffer_outlines->setMat4("PV", renderDataHandle.mProjection * renderDataHandle.mView);

		for (auto [outline, modelComp, transform] : ECSHandler::registry()->getComponentsArray<OutlineComponent, ModelComponent, TransformComponent>()) {
			if (!&modelComp || !&transform) {
				continue;
			}

			auto& model = modelComp.getModel();
			for (auto& mesh : model.mMeshHandles) {
				if (mesh.mBounds && mesh.mBounds->isOnFrustum(renderDataHandle.mCamFrustum, transform.getTransform())) {
					batcher->addToDrawList(mesh.mData->mVao, mesh.mData->mVertices.size(), mesh.mData->mIndices.size(), *mesh.mMaterial, transform.getTransform(), false);
				}
			}
		}
		
		batcher->flushAll(true, ECSHandler::registry()->getComponent<TransformComponent>(curCam)->getPos());

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
