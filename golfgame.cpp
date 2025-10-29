// golfgame.cpp : Defines the entry point for the application.
//

#include "golfgame.h"
#include "core/gfx/Renderer.hpp"
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <windows.h>
#include <chrono>

using namespace std;

int main()
{
	sf::RenderWindow window(sf::VideoMode(sf::Vector2u(800,600),32), "DX with SFML Window");
	window.setMouseCursorVisible(false);
	window.setMouseCursorGrabbed(true);
	HWND hwnd = window.getNativeHandle();

	Renderer renderer;
	renderer.initialize(hwnd, 800, 600, true);

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

		// 创建旋转变换矩阵
		DirectX::XMMATRIX transform =
			DirectX::XMMatrixScaling(2.0f, 2.0f, 2.0f) *
			DirectX::XMMatrixRotationRollPitchYaw(time_in_seconds * 0.3f, time_in_seconds * 0.6f, 0) *
			DirectX::XMMatrixTranslation(0.0f, 0.0f, 2.0f);

		renderer.beginFrame(0.0f, 0.0f, 1.0f, 1);
		renderer.drawMesh(renderer.getCubeMesh(), transform);
		renderer.endFrame();
	}

	return 0;
}
