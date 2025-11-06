// golfgame.cpp : Defines the entry point for the application.
//

#include "golfgame.h"
#include "core/gfx/Renderer.hpp"
#include "game/entity/Cube.hpp"
#include "core/gfx/ModelLoader.hpp"
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <windows.h>
#include <chrono>
#include <array>
#include <DirectXMath.h>

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

	// Try loading a model via ModelLoader
	Mesh modelMesh;
	Texture modelTex;
	std::wstring modelPath = ExeDirMain() + L"\\asset\\ball.fbx";
	bool modelLoaded = ModelLoader::LoadFBX(renderer.device(), modelPath, modelMesh, modelTex);
	printf("Model loaded: %d\n", modelLoaded);

	// Create 9 Cube entities arranged in a 3x3 grid around the origin
	std::array<Cube, 9> cubes;
	const float spacing = 3.0f; // equal spacing
	{
		// Initialize cubes and set their positions in a 3x3 grid (i: x, j: y)
		int idx = 0;
		for (int j = -1; j <= 1; ++j) {
			for (int i = -1; i <= 1; ++i) {
				Cube& c = cubes[idx++];
				c.initialize(renderer.device());
				c.setPosition(i * spacing, 0, j*spacing);
			}
		}
	}

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

		// 更新每个Cube的旋转并绘制 3x3 排布
		renderer.beginFrame(0.0f, 0.0f, 1.0f, 1);

		// Draw loaded model if available
		// if (modelLoaded) {
		// 	XMMATRIX S = XMMatrixScaling(10.0f, 10.0f, 10.0f);
		// 	XMMATRIX R = XMMatrixRotationY(time_in_seconds * 0.5f);
		// 	XMMATRIX T = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
		// 	XMMATRIX M = S * R * T;
		// 	renderer.drawMesh(modelMesh, M, modelTex);
		// }

		for (auto& c : cubes) {
			c.setRotation(time_in_seconds * 0.3f, time_in_seconds * 0.6f, 0.0f);
			renderer.drawMesh(c.getMesh(), c.getTransform(), c.getTexture());
		}
		renderer.endFrame();
	}

	return 0;
}
