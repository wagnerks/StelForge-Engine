#include "Renderer.h"

#include <deque>
#include <random>

#include "BlendStack.h"
#include "CapabilitiesStack.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/TreeComponent.h"
#include "core/Engine.h"
#include "core/FileSystem.h"
#include "core/ThreadPool.h"
#include "ecss/Registry.h"
#include "componentsModule/FrustumComponent.h"
#include "componentsModule/OcTreeComponent.h"
#include "debugModule/Benchmark.h"
#include "logsModule/logger.h"

#include "gtc/random.hpp"

#include "propertiesModule/PropertiesSystem.h"

constexpr int GLFW_CONTEXT_VER_MAJ = 4;
#ifdef __APPLE__
constexpr int GLFW_CONTEXT_VER_MIN = 1;
#else
constexpr int GLFW_CONTEXT_VER_MIN = 6;
#endif


using namespace SFE;
using namespace ::SFE::Render;
using namespace ::SFE::CoreModule;


Renderer::~Renderer() {
	delete mBatcher;
	glfwTerminate();
}

void Renderer::draw() {
	Render::Renderer::mDrawCallsCount = 0;
	Render::Renderer::mDrawVerticesCount = 0;
}

void Renderer::postDraw() {
	FUNCTION_BENCHMARK;
	glfwSwapBuffers(Engine::instance()->getMainWindow());
	glfwPollEvents();
}

void Renderer::init() {
	mBatcher = new Batcher();
}


void Renderer::drawArrays(RenderMode mode, GLsizei size, GLint first) {
	glDrawArrays(mode, first, size);
	mDrawCallsCount++;
	mDrawVerticesCount += size;
}

void Renderer::drawElements(RenderMode mode, GLsizei size, RenderDataType type, const void* place) {
	glDrawElements(mode, size, static_cast<GLenum>(type), place);
	mDrawCallsCount++;
	mDrawVerticesCount += size;
}

void Renderer::drawElementsInstanced(RenderMode mode, GLsizei size, RenderDataType type, GLsizei instancesCount, const void* place) {
	glDrawElementsInstanced(mode, size, static_cast<GLenum>(type), place, instancesCount);
	mDrawCallsCount++;
	mDrawVerticesCount += size * instancesCount;
}

void Renderer::drawArraysInstancing(RenderMode mode, GLsizei size, GLsizei instancesCount, GLint first) {
	glDrawArraysInstanced(mode, first, size, instancesCount);
	mDrawCallsCount++;
	mDrawVerticesCount += size * instancesCount;
}

GLFWwindow* Renderer::initGLFW() {
	if (mGLFWInited) {
		assert(false && "GLFW Already inited");
		return nullptr;
	}
	mGLFWInited = true;

    if (!glfwInit()){
        LogsModule::Logger::LOG_ERROR("Failed to init GLFW window");
        glfwTerminate();
        return nullptr;
    }
    
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GLFW_CONTEXT_VER_MAJ);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GLFW_CONTEXT_VER_MIN);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	glfwWindowHint(GLFW_SAMPLES, 2);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	auto window = glfwCreateWindow(Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, "GameEngine", nullptr, nullptr);
	if (window == nullptr) {
        const char* error;
        glfwGetError(&error);
        
		LogsModule::Logger::LOG_ERROR("Failed to create GLFW window %s", error );
		glfwTerminate();
		return nullptr;
	}

	glfwGetWindowContentScale(window, &Renderer::SCR_RENDER_SCALE_W, &Renderer::SCR_RENDER_SCALE_H);
	glfwGetFramebufferSize(window, &Renderer::SCR_RENDER_W, &Renderer::SCR_RENDER_H);
#ifdef __APPLE__
	Renderer::SCR_WIDTH = Renderer::SCR_RENDER_W / Renderer::SCR_RENDER_SCALE_W;
	Renderer::SCR_HEIGHT = Renderer::SCR_RENDER_H / Renderer::SCR_RENDER_SCALE_H;
#endif

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) {
		glViewport(0, 0, width, height);
		Renderer::SCR_RENDER_W = width;
		Renderer::SCR_RENDER_H = height;
#ifdef __APPLE__
		Renderer::SCR_WIDTH = Renderer::SCR_RENDER_W / Renderer::SCR_RENDER_SCALE_W;
		Renderer::SCR_HEIGHT = Renderer::SCR_RENDER_H / Renderer::SCR_RENDER_SCALE_H;
#else
		Renderer::SCR_WIDTH = Renderer::SCR_RENDER_W;
		Renderer::SCR_HEIGHT = Renderer::SCR_RENDER_H;
#endif


	});

	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
		LogsModule::Logger::LOG_ERROR("Failed to initialize GLAD");
		glfwTerminate();
		glfwDestroyWindow(window);
		return nullptr;
	}

	glfwSwapInterval(-1);

	CapabilitiesStack::push(CULL_FACE, true);
	CapabilitiesStack::push(DEPTH_TEST, true);
	glDepthFunc(GL_LEQUAL);
	//glDepthFunc(GL_LESS);
	glClearDepth(drawDistance);

	//Render::CapabilitiesStack::push(Render::BLEND, true);
	//BlendFuncStack::push({ SRC_ALPHA, ONE_MINUS_SRC_ALPHA });

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	LogsModule::Logger::LOG_INFO("GLFW initialized");
	return window;
}
