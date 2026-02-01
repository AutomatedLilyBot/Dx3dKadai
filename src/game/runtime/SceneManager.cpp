#include "SceneManager.hpp"
#include "../scene/TransitionScene.hpp"

SceneManager::SceneManager(Renderer *renderer) : renderer_(renderer) {}

void SceneManager::setScene(std::unique_ptr<Scene> scene) {
    currentScene_ = std::move(scene);
    if (currentScene_) {
        currentScene_->setSceneManager(this);
        currentScene_->init(renderer_);
    }
}

void SceneManager::transitionTo(std::unique_ptr<Scene> targetScene) {
    // 延迟切换：先保存到 pending，在 tick 结束后再执行
    auto transition = std::make_unique<TransitionScene>(std::move(targetScene));
    pendingScene_ = std::move(transition);
}

void SceneManager::finishTransition(std::unique_ptr<Scene> nextScene) {
    // 延迟切换：先保存到 pending，在 tick 结束后再执行
    pendingScene_ = std::move(nextScene);
}

void SceneManager::tick(float dt) {
    if (currentScene_) {
        currentScene_->tick(dt);
    }

    // 在 tick 结束后，如果有待切换的场景，则执行切换
    if (pendingScene_) {
        setScene(std::move(pendingScene_));
        pendingScene_.reset();
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
