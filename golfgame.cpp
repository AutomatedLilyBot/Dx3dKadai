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
	sf::RenderWindow window(sf::VideoMode(sf::Vector2u(1280,720),32), "DX with SFML Window - RTS Mode");
	window.setMouseCursorVisible(true);  // RTS 模式：显示鼠标光标
	window.setMouseCursorGrabbed(false); // RTS 模式：不捕获鼠标
	HWND hwnd = window.getNativeHandle();

	Renderer renderer;
	renderer.initialize(hwnd, 1280, 720, true);

	// 场景管理：使用智能指针管理当前场景
	std::unique_ptr<BattleScene> currentScene = std::make_unique<BattleScene>();
	currentScene->init(&renderer);

	// 获取程序启动时间和帧时间
	auto startTime = std::chrono::high_resolution_clock::now();
	auto lastFrameTime = startTime;

	// RTS 模式不需要鼠标状态追踪

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
				if (currentScene) {
					currentScene->camera().processMouseScroll(scroll->delta);
				}
			}
		}

		// 输入处理
		if (currentScene) {
			// 更新右键状态（用于 InputManager 的长按/短按检测）
			currentScene->inputManager().setRightButtonDown(sf::Mouse::isButtonPressed(sf::Mouse::Button::Right));

			// 调用场景的输入处理（鼠标点击交互）
			currentScene->handleInput(deltaTime, &window);

			// RTS 键盘输入 - WASD 平移，QE 旋转
			bool forward = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W);
			bool backward = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S);
			bool left = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A);
			bool right = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D);
			bool rotateLeft = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q);
			bool rotateRight = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E);

			currentScene->camera().processKeyboard(forward, backward, left, right, rotateLeft, rotateRight, deltaTime);
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
