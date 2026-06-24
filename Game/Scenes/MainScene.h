#pragma once
#include "IScene.h"
#include "../../Engine/Camera.h"
#include "../../Engine/Renderer.h"
#include <memory>

#include "../../Engine/EventSystem.h"
#include "../../externals/entt/entt.hpp"

#include "../Systems/ISystem.h" // For GameContext
#include "../../Engine/ParticleEditor.h"
#include <vector>
#include <string>

// --- Systems ---
#include "../Systems/UISystem.h"
#include "../Systems/WorkEnergySystem.h"

namespace Game {

class MainScene : public Engine::IScene {
public:
    MainScene() = default;
    ~MainScene() override;

    void Initialize(Engine::WindowDX* dx, const Engine::SceneParameters& params) override;
    void Update() override;
    void Draw() override;
    void DrawUI() override;

    entt::registry& GetRegistry() { return registry_; }
    Engine::EventSystem& GetEventSystem() { return eventSystem_; }

    std::vector<entt::entity>& GetSelectedEntities() { return selectedEntities_; }
    entt::entity GetSelectedEntity() const { return selectedEntity_; }
    void SetSelectedEntity(entt::entity e) { selectedEntity_ = e; }
    
    entt::entity CreateEntity(const std::string& name = "Entity");
    void DestroyObject(uint32_t id);
    
    bool GetIsPlaying() const { return isPlaying_; }
    void SetIsPlaying(bool b) { isPlaying_ = b; }
    
    Engine::ParticleEditor& GetParticleEditor() { return particleEditor_; }
    Engine::Camera& GetCamera() { return camera_; }
    GameContext& GetContext() { return context_; }
    
    Engine::Matrix4x4 GetWorldMatrix(int /*index*/) const { Engine::Matrix4x4 m; return m; }
    void SyncTag(entt::entity /*e*/) {}
    
private:
    Engine::Camera camera_;
    Engine::WindowDX* dx_ = nullptr;
    Engine::Renderer* renderer_ = nullptr;
    
    entt::registry registry_;
    Engine::EventSystem eventSystem_;
    
    std::chrono::steady_clock::time_point lastTime_;
    float dt_ = 1.0f / 60.0f;
    float totalTime_ = 0.0f;

    std::vector<entt::entity> selectedEntities_;
    entt::entity selectedEntity_ = entt::null;
    bool isPlaying_ = false;
    Engine::ParticleEditor particleEditor_;
    GameContext context_;

    // Systems
    std::vector<std::unique_ptr<ISystem>> systems_;
};

} // namespace Game
