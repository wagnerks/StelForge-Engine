﻿#include "CameraSystem.h"

#include "OcTreeSystem.h"
#include "PhysicsSystem.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "ecss/Registry.h"

namespace SFE::SystemsModule {
	CameraSystem::CameraSystem() {
		auto aspect = static_cast<float>(Engine::instance()->getWindow()->getScreenData().width) / static_cast<float>(Engine::instance()->getWindow()->getScreenData().height);
		mDefaultCamera = ECSHandler::registry().takeEntity();

		auto transform = ECSHandler::registry().addComponent<TransformComponent>(mDefaultCamera, mDefaultCamera);
		transform->setPos({ 0.f, 200.f, 400.f });
		transform->setRotate({ -20.f, 0.f, 0.0f });
		transform->reloadTransform();
		ECSHandler::registry().addComponent<CameraComponent>(mDefaultCamera, 45.f, aspect, Engine::instance()->getWindow()->getScreenData().near, Engine::instance()->getWindow()->getScreenData().far)->updateFrustum(transform->getViewMatrix());
		initKeyEvents();
	}

	void CameraSystem::update(float_t dt) {
		auto cam = getCurrentCamera();
		if (isPressed[CoreModule::InputKey::KEY_W]) {
			processKeyboard(cam, FORWARD, dt);
		}
		if (isPressed[CoreModule::InputKey::KEY_S]) {
			processKeyboard(cam, BACKWARD, dt);
		}
		if (isPressed[CoreModule::InputKey::KEY_A]) {
			processKeyboard(cam, LEFT, dt);
		}
		if (isPressed[CoreModule::InputKey::KEY_D]) {
			processKeyboard(cam, RIGHT, dt);
		}
		if (isPressed[CoreModule::InputKey::KEY_SPACE]) {
			processKeyboard(cam, TOP, dt);
		}
		if (isPressed[CoreModule::InputKey::KEY_LEFT_SHIFT]) {
			processKeyboard(cam, BOTTOM, dt);
		}
	}

	void CameraSystem::setCurrentCamera(ecss::EntityId camera) {
		mCurrentCamera = camera;
	}

	ecss::EntityId CameraSystem::getCurrentCamera() const {
		if (mCurrentCamera != ecss::INVALID_ID) {
			return mCurrentCamera;
		}

		return mDefaultCamera;
	}
	
	void CameraSystem::initKeyEvents() {
		onKeyEvent = [this](CoreModule::InputKey key, CoreModule::InputEventType type) {
			if (type == SFE::CoreModule::InputEventType::PRESS) {
				isPressed[key] = true;
			}
			else if (type == SFE::CoreModule::InputEventType::RELEASE) {
				isPressed[key] = false;
			}
		};

		onMouseBtnEvent = [this](Math::DVec2 mPos, CoreModule::MouseButton btn, CoreModule::InputEventType action) {
			if (btn == CoreModule::MouseButton::MOUSE_BUTTON_MIDDLE && action == CoreModule::InputEventType::PRESS) {
				processMouse = true;
				glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			}
			if (btn == CoreModule::MouseButton::MOUSE_BUTTON_MIDDLE && action == CoreModule::InputEventType::RELEASE) {
				processMouse = false;
				glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			}
		};

		onMouseEvent = [this](Math::DVec2 mPos, Math::DVec2 mouseOffset) {
			processMouseMovement(static_cast<float>(mouseOffset.x), static_cast<float>(mouseOffset.y));
		};

		onScrollEvent = [this](Math::DVec2 scrollOffset) {
			processMouseScroll(static_cast<float>(scrollOffset.y));
		};
	}

	void CameraSystem::processKeyboard(ecss::EntityId camera, CameraMovement direction, float deltaTime) {
		if (camera == ecss::INVALID_ID) {
			return;
		}

		auto tc = ECSHandler::registry().getComponent<TransformComponent>(camera);

		const float velocity = movementSpeed * deltaTime;

		Math::Vec3 dif = {};
		if (direction == FORWARD) {
			dif += tc->getForward() * velocity;
		}

		if (direction == BACKWARD) {
			dif -= tc->getForward() * velocity;
		}

		if (direction == LEFT) {
			dif -= tc->getRight() * velocity;
		}

		if (direction == RIGHT) {
			dif += tc->getRight() * velocity;
		}

		if (direction == TOP) {
			dif += Math::Vec3{0.f, 1.f, 0.f} *velocity;
		}

		if (direction == BOTTOM) {
			dif -= Math::Vec3{0.f, 1.f, 0.f} *velocity;
		}

		if (dif != Math::Vec3{}) {
			tc->setPos(tc->getPos() + dif);
			/*tc->reloadTransform();
			ECSHandler::registry().getComponent<CameraComponent>(camera)->updateFrustum(tc->getViewMatrix());
			TasksManager::instance()->notify({ camera, CAMERA_UPDATED });*/
		}
	}


	void CameraSystem::processMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
		if (!processMouse) {
			return;
		}
		xoffset *= mouseSensitivity;
		yoffset *= mouseSensitivity;
		auto tc = ECSHandler::registry().getComponent<TransformComponent>(getCurrentCamera());
		if (!tc) {
			return;
		}
		auto Pitch = tc->getRotate().x;
		auto Yaw = tc->getRotate().y;
		Yaw -= xoffset;

		Pitch += yoffset;

		if (constrainPitch) {
			Pitch = std::min(std::max(Pitch, -90.f), 90.f);
		}

		tc->setRotate({ Pitch, Yaw, 0.f });
		/*tc->reloadTransform();
		ECSHandler::registry().getComponent<CameraComponent>(getCurrentCamera())->updateFrustum(tc->getViewMatrix());*/
	}

	void CameraSystem::processMouseScroll(float yoffset) {
		auto fov = ECSHandler::registry().getComponent<CameraComponent>(getCurrentCamera())->getProjection().getFOV();
		fov -= yoffset;

		ECSHandler::registry().getComponent<CameraComponent>(getCurrentCamera())->getProjection().setFOV(std::min(std::max(1.f, fov), 90.f));

		ECSHandler::registry().getComponent<CameraComponent>(getCurrentCamera())->updateFrustum(ECSHandler::registry().getComponent<TransformComponent>(getCurrentCamera())->getViewMatrix());
	}
}