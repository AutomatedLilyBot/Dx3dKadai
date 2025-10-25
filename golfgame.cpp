// golfgame.cpp : Defines the entry point for the application.
//

#include "golfgame.h"
#include "core/gfx/Renderer.hpp"
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <windows.h>

using namespace std;

int main()
{

	sf::RenderWindow window(sf::VideoMode(sf::Vector2u(800,600),32), "DX with SFML Window");
	HWND hwnd = window.getNativeHandle();   // 就是 Win32 HWND

	Renderer renderer;
	renderer.initialize(hwnd, 800, 600, true);

	Renderer::SpriteDesc3D d;
	d.position = {0,0,0};
	d.size = {200,200};
	d.rotationY = 0.35f;
	d.albedo = {0.9f,0.6f,0.6f};

	while (window.isOpen()) {
		renderer.beginFrame(1.0f,1.0f,1.0f,1);
		renderer.drawSprite(d);   // ← 现在是“3D 带光照的矩形”
		renderer.endFrame();
	}


}
