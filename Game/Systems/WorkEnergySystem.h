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

    float GetEnergy() const { return currentEnergy_; }
    bool IsFocusAuraActive() const { return focusAuraActive_; }

    // Callbacks for WinEventHook
    static void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);

private:
    // Energy tracking
    float currentEnergy_ = 0.0f;
    bool focusAuraActive_ = false;

    // Mouse tracking
    POINT lastMousePos_{};
    std::chrono::steady_clock::time_point lastActivityTime_;

    // Window event hook
    HWINEVENTHOOK hEventHook_ = nullptr;

    // Singleton instance for the callback
    static WorkEnergySystem* s_Instance_;

    void AddEnergy(float amount);
};

} // namespace Game
