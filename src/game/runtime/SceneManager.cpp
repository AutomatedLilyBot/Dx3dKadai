#include "SceneManager.hpp"
#include "TransitionScene.hpp"

SceneManager::SceneManager(Renderer *renderer) : renderer_(renderer) {}

void SceneManager::setScene(std::unique_ptr<Scene> scene) {
    currentScene_ = std::move(scene);
    if (currentScene_) {
        currentScene_->setSceneManager(this);
        currentScene_->init(renderer_);
    }
}

void SceneManager::transitionTo(std::unique_ptr<Scene> targetScene) {
    auto transition = std::make_unique<TransitionScene>(std::move(targetScene));
    setScene(std::move(transition));
}

void SceneManager::finishTransition(std::unique_ptr<Scene> nextScene) {
    setScene(std::move(nextScene));
}

void SceneManager::tick(float dt) {
    if (currentScene_) {
        currentScene_->tick(dt);
    }
}

void SceneManager::render() {
    if (currentScene_) {
        currentScene_->render();
    }
}

void SceneManager::handleInput(float dt, const void *window) {
    if (currentScene_) {
        currentScene_->handleInput(dt, window);
    }
}
