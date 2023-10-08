﻿#include "ShaderSystem.h"

#include <algorithm>

#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/ShaderComponent.h"
#include "core/ECSHandler.h"
#include "..\ecss\Registry.h"

namespace Engine::SystemsModule {
	void ShaderSystem::update(float_t dt) {
		auto shaderController = ShaderModule::ShaderController::instance();
		drawableEntities.clear();
		
		for (auto [shaderComponent] : ECSHandler::registry()->getComponentsArray<ComponentsModule::ShaderComponent>()) {
			auto shader = shaderController->getShader(shaderComponent.shaderId);
			if (!shader) {
				continue;
			}

			if (shaderComponent.updateFunction) {
				shaderComponent.updateFunction(&shaderComponent);
			}

			for (const auto typedUniforms : shaderComponent.uniforms) {
				typedUniforms->apply(shader);
			}

			drawableEntities.emplace_back(shaderComponent.shaderId, shaderComponent.getEntityId());
		}

		std::stable_sort(drawableEntities.begin(), drawableEntities.end(), [](const auto& a, const auto& b) {
			return a.first > b.first;
		});
	}
}
