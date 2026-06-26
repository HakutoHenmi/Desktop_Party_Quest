#pragma once
#include "ISystem.h"
#include <Windows.h>
#include <chrono>

namespace Game {

class WorkEnergySystem : public ISystem {
public:
    void Update(entt::registry& registry, GameContext& ctx) override;
    void Reset(entt::registry& registry) override;
    void Draw(entt::registry& /*registry*/, GameContext& /*ctx*/) override {}
    void DrawUI(entt::registry& /*registry*/, GameContext& /*ctx*/) override {}
    
    WorkEnergySystem();
    ~WorkEnergySystem();

    // Energy configuration
    static constexpr float kMaxEnergy = 100.0f; // ★最大エネルギー値

    float GetEnergy() const { return currentEnergy_; }
    bool IsFocusAuraActive() const { return focusAuraActive_; }

private:
    // Energy tracking
    float currentEnergy_ = 0.0f;
    bool focusAuraActive_ = false;

    // Mouse tracking
    POINT lastMousePos_{};
    std::chrono::steady_clock::time_point lastActivityTime_;

    // Input tracking
    int prevKeyStrokes_ = 0;
    int prevMouseClicks_ = 0;
    bool prevActiveWindow_ = true;

    void AddEnergy(float amount);
};

} // namespace Game
