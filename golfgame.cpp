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
#include "src/game/scene/BattleScene.hpp"
#include "src/game/scene/MenuScene.hpp"
#include "src/game/runtime/SceneManager.hpp"

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

        SceneManager sceneManager(&renderer);
        sceneManager.setScene(std::make_unique<MenuScene>());

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
                        if (const auto* scroll = event->getIf<sf::Event::MouseWheelScrolled>()) {
                                if (auto* battle = dynamic_cast<BattleScene*>(sceneManager.currentScene())) {
                                        battle->camera().processMouseScroll(scroll->delta);
                                }
                        }
                }

                // 输入处理
                sceneManager.handleInput(deltaTime, &window);

                if (auto* battleScene = dynamic_cast<BattleScene*>(sceneManager.currentScene())) {
                        battleScene->inputManager().setRightButtonDown(sf::Mouse::isButtonPressed(sf::Mouse::Button::Right));

                        bool forward = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W);
                        bool backward = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S);
                        bool left = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A);
                        bool right = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D);
                        bool rotateLeft = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q);
                        bool rotateRight = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E);
                        bool boost = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

                        battleScene->camera().processKeyboard(forward, backward, left, right, rotateLeft, rotateRight, boost, deltaTime);

                        static bool fWasPressed = false;
                        bool fPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F);
                        if (fPressed && !fWasPressed) {
                                EntityId selectedId = battleScene->selectedNodeId();
                                if (selectedId != 0) {
                                        NodeEntity* node = battleScene->getNodeEntity(selectedId);
                                        if (node) {
                                                battleScene->camera().focusOnTarget(node->transform.position);
                                                printf("Focusing on Node %llu at (%.2f, %.2f, %.2f)\n",
                                                       selectedId,
                                                       node->transform.position.x,
                                                       node->transform.position.y,
                                                       node->transform.position.z);
                                        }
                                }
                        }
                        fWasPressed = fPressed;

                        // Camera switching: 1, 2, 3 keys
                        static bool key1WasPressed = false;
                        static bool key2WasPressed = false;
                        static bool key3WasPressed = false;

                        bool key1Pressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Num1);
                        bool key2Pressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Num2);
                        bool key3Pressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Num3);

                        if (key1Pressed && !key1WasPressed) {
	                        battleScene->camera().switchToCamera(CameraType::FreeCam);
	                        printf("Switching to Free Camera\n");
                        }
                        if (key2Pressed && !key2WasPressed) {
	                        battleScene->camera().switchToCamera(CameraType::FrontView);
	                        printf("Switching to Front View Camera\n");
                        }
                        if (key3Pressed && !key3WasPressed) {
	                        battleScene->camera().switchToCamera(CameraType::TopView);
	                        printf("Switching to Top View Camera\n");
                        }

                        key1WasPressed = key1Pressed;
                        key2WasPressed = key2Pressed;
                        key3WasPressed = key3Pressed;
                } else if (auto* menuScene = dynamic_cast<MenuScene*>(sceneManager.currentScene())) {
                        if (menuScene->exitRequested()) {
                                window.close();
                        }
                }
		// 驱动场景调度（物理→更新→提交）
		sceneManager.tick(deltaTime);
		// Frame render
        renderer.beginFrame(0.0f, 0.0f, 1.0f, 1);
		// 让当前场景渲染其内容
		sceneManager.render();
		renderer.endFrame();
	}

	return 0;
}
