#pragma once
#include <memory>
#include "Scene.hpp"

class TransitionScene;

class SceneManager {
public:
    explicit SceneManager(Renderer *renderer);

    void setScene(std::unique_ptr<Scene> scene);
    void transitionTo(std::unique_ptr<Scene> targetScene);

    void tick(float dt);
    void render();
    void handleInput(float dt, const void *window);

    Scene *currentScene() const { return currentScene_.get(); }

private:
    friend class TransitionScene;

    void finishTransition(std::unique_ptr<Scene> nextScene);

    Renderer *renderer_ = nullptr;
    std::unique_ptr<Scene> currentScene_;
};
