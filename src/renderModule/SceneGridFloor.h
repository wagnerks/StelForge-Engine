﻿#pragma once
#include "shaderModule/Shader.h"
#include "mat4x4.hpp"

namespace Engine::RenderModule {
	class SceneGridFloor {
	public:
		SceneGridFloor(float size);
		~SceneGridFloor();
		void init();
		void draw();
	private:
		Engine::ShaderModule::ShaderBase* floorShader = nullptr;
		unsigned VAO = 0;
		unsigned VBO = 0;
		float size = 0;
		glm::mat4 transform = glm::mat4(1.f);
	};
}

