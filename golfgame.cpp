// golfgame.cpp : Defines the entry point for the application.
//

#include "golfgame.h"
#include "src/core/gfx/Renderer.hpp"
#include "src/core/gfx/ModelLoader.hpp"
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <windows.h>
#include <chrono>
#include <random>
#include <DirectXMath.h>
#include "src/core/gfx/Model.hpp"
#include "src/game/world/Field.hpp"
#include "src/game/entity/StaticEntity.hpp"
#include "src/core/physics/Transform.hpp"
#include "src/game/runtime/BattleScene.hpp"

using namespace std;
using namespace DirectX;

static std::wstring ExeDirMain() {
	wchar_t buf[MAX_PATH];
	GetModuleFileNameW(nullptr, buf, MAX_PATH);
	std::wstring s(buf);
	auto p = s.find_last_of(L"\\/");
	return (p == std::wstring::npos) ? L"." : s.substr(0, p);
}

int main()
{
	sf::RenderWindow window(sf::VideoMode(sf::Vector2u(800,600),32), "DX with SFML Window");
	window.setMouseCursorVisible(false);
	window.setMouseCursorGrabbed(true);
	HWND hwnd = window.getNativeHandle();

	Renderer renderer;
	renderer.initialize(hwnd, 800, 600, true);

	// 场景管理：使用智能指针管理当前场景
        std::unique_ptr<Scene> currentScene = std::make_unique<BattleScene>();
	currentScene->init(&renderer);

	// Create a Field and load models for platform and decoration
	Field field;
	Model cubeModel;
	Model treeModel;

	std::wstring cubePath = ExeDirMain() + L"\\asset\\cube.fbx";
	std::wstring treePath = ExeDirMain() + L"\\asset\\tree.fbx";

	bool cubeLoaded = ModelLoader::LoadFBX(renderer.device(), cubePath, cubeModel);
	bool treeLoaded = ModelLoader::LoadFBX(renderer.device(), treePath, treeModel);
	printf("Cube FBX loaded: %d, meshes=%zu, drawItems=%zu\n", cubeLoaded, cubeModel.meshes.size(),
	       cubeModel.drawItems.size());
	printf("Tree FBX loaded: %d, meshes=%zu, drawItems=%zu\n", treeLoaded, treeModel.meshes.size(),
	       treeModel.drawItems.size());

	// Build a 10x10 platform of cubes (grid in XZ plane)
	if (cubeLoaded) {
		const int W = 10, H = 10;
		for (int z = 0; z < H; ++z) {
			for (int x = 0; x < W; ++x) {
				auto e = std::make_unique<StaticEntity>();
				e->modelRef = &cubeModel; // lifetime note: cubeModel lives until main ends
				e->transform.position = {(float) x * 0.1f, 0.0f, (float) z * 0.1f};
				e->transform.scale = {10.0f, 10.0f, 10.0f};
				e->transform.setRotationEuler(XM_PIDIV2, 0, 0);
				field.add(std::move(e));
			}
		}
	}

	// Randomly place several trees on top of the platform
	if (treeLoaded) {
		std::mt19937 rng(1337);
		std::uniform_int_distribution<int> distXZ(-0, 0.1f);
		std::uniform_real_distribution<float> jitter(-0.0f, 0.1f);
		const int treeCount = 10; // a few decorative trees
		for (int i = 0; i < treeCount; ++i) {
			float x = (float) distXZ(rng) + jitter(rng);
			float z = (float) distXZ(rng) + jitter(rng);
			auto t = std::make_unique<StaticEntity>();
			t->modelRef = &treeModel;
			t->transform.scale = {10.0f, 10.0f, 10.0f};
			t->transform.setRotationEuler(XM_PIDIV2, 0, 0);
			t->transform.position = {x, 0, z}; // slightly above the cubes
			field.add(std::move(t));
		}
	}

	// Create 9 Cube entities arranged in a 3x3 grid around the origin
	// std::array<Cube, 9> cubes;
	// const float spacing = 3.0f; // equal spacing
	// {
	// 	// Initialize cubes and set their positions in a 3x3 grid (i: x, j: y)
	// 	int idx = 0;
	// 	for (int j = -1; j <= 1; ++j) {
	// 		for (int i = -1; i <= 1; ++i) {
	// 			Cube& c = cubes[idx++];
	// 			c.initialize(renderer.device());
	// 			c.setPosition(i * spacing, 0, j*spacing);
	// 		}
	// 	}
	// }

	// 获取程序启动时间和帧时间
	auto startTime = std::chrono::high_resolution_clock::now();
	auto lastFrameTime = startTime;

	// Mouse state
	sf::Vector2i lastMousePos = sf::Mouse::getPosition(window);
	sf::Vector2i centerPos(400, 300);
	bool firstMouse = true;

	while (window.isOpen()) {
		// 计算deltaTime
		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
		lastFrameTime = currentTime;
		float time_in_seconds = std::chrono::duration<float>(currentTime - startTime).count();

		// 处理事件
		while (auto event = window.pollEvent()) {
			if (event->is<sf::Event::Closed>()) {
				window.close();
			}
			if (const auto* keyPress = event->getIf<sf::Event::KeyPressed>()) {
				if (keyPress->code == sf::Keyboard::Key::Escape) {
					window.close();
				}
                            // 场景切换：按数字键1重新加载BattleScene
				if (keyPress->code == sf::Keyboard::Key::Num1) {
					currentScene.reset();
                                    currentScene = std::make_unique<BattleScene>();
                                    currentScene->init(&renderer);
                                    printf("Switched to BattleScene\n");
				}
			}
			if (const auto* scroll = event->getIf<sf::Event::MouseWheelScrolled>()) {
				renderer.getCamera().processMouseScroll(scroll->delta);
			}
		}

		// 键盘输入 - WASD移动，Space/Shift上下移动
		bool forward = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W);
		bool backward = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S);
		bool left = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A);
		bool right = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D);
		bool up = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
		bool down = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift);

		renderer.getCamera().processKeyboard(forward, backward, left, right, up, down, deltaTime);

		// 鼠标输入 - 控制视角
		if (window.hasFocus()) {
			sf::Vector2i currentMousePos = sf::Mouse::getPosition(window);

			if (firstMouse) {
				lastMousePos = currentMousePos;
				firstMouse = false;
			}

			sf::Vector2i mouseDelta = currentMousePos - centerPos;

			if (mouseDelta.x != 0 || mouseDelta.y != 0) {
				renderer.getCamera().processMouseMove(
					static_cast<float>(mouseDelta.x),
					static_cast<float>(-mouseDelta.y)  // Invert Y
				);

				// Reset mouse to center
				sf::Mouse::setPosition(centerPos, window);
			}
		}

		// 驱动场景调度（物理→更新→提交）
		if (currentScene) {
			currentScene->tick(deltaTime);
		}

		// Frame render
        renderer.beginFrame(0.0f, 0.0f, 1.0f, 1);

		// 让当前场景渲染其内容
		if (currentScene) {
			currentScene->render();
		}

		//renderer.drawColliderWire(*obb);
		// Draw loaded model if available
		// if (modelLoaded) {
		// 	XMMATRIX S = XMMatrixScaling(10.0f, 10.0f, 10.0f);
		//	XMMATRIX R = XMMatrixRotationY(time_in_seconds * 0.5f);
		//	XMMATRIX T = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
		//	XMMATRIX M = S * R * T;
		//	renderer.drawMesh(modelMesh, M, modelTex);
		//}


		renderer.endFrame();
	}

	return 0;
}
