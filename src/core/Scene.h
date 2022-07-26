﻿#pragma once
#include "modelModule/Model.h"
#include "nodeModule/Node.h"
#include "renderModule/DirectionalLight.h"
#include "renderModule/SceneGridFloor.h"
#include "renderModule/Skybox.h"

namespace GameModule::CoreModule {
	class Scene {
	public:
		void init();
		void updateScene(float dt);
		void drawScene();
		~Scene();
	private:
		GameEngine::NodeModule::Node* rootNode = nullptr;
		GameEngine::RenderModule::Skybox* skybox = nullptr;
		GameEngine::RenderModule::SceneGridFloor* gridFloor = nullptr;
		GameEngine::LightsModule::DirectionalLight* sunLight = nullptr;

		std::vector<glm::vec3> objectPositions;

		std::vector<glm::vec3> randomLightSpeeds;

		GameEngine::ModelModule::Model* modelObj = nullptr;
	};
}

