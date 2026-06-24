#include "WorkEnergySystem.h"
#include <cmath>

namespace Game {

WorkEnergySystem* WorkEnergySystem::s_Instance_ = nullptr;

WorkEnergySystem::WorkEnergySystem() {
    s_Instance_ = this;
    currentEnergy_ = 0.0f;
    focusAuraActive_ = false;
    lastActivityTime_ = std::chrono::steady_clock::now();
    GetCursorPos(&lastMousePos_);

    // Set up active window switching hook (replaces SetWindowsHookEx for key logging)
    hEventHook_ = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr,
        WorkEnergySystem::WinEventProc,
        0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
}

WorkEnergySystem::~WorkEnergySystem() {
    if (hEventHook_) {
        UnhookWinEvent(hEventHook_);
        hEventHook_ = nullptr;
    }
    s_Instance_ = nullptr;
}

void WorkEnergySystem::Reset(entt::registry& /*registry*/) {
    currentEnergy_ = 0.0f;
    focusAuraActive_ = false;
    lastActivityTime_ = std::chrono::steady_clock::now();
    GetCursorPos(&lastMousePos_);
}

void WorkEnergySystem::Update(entt::registry& /*registry*/, GameContext& ctx) {
    float dt = ctx.dt;
    // 1. Calculate mouse movement distance
    POINT currentMousePos;
    if (GetCursorPos(&currentMousePos)) {
        float dx = static_cast<float>(currentMousePos.x - lastMousePos_.x);
        float dy = static_cast<float>(currentMousePos.y - lastMousePos_.y);
        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance > 1.0f) {
            AddEnergy(distance * 0.1f); // Arbitrary energy scaling
            lastMousePos_ = currentMousePos;
        }
    }

    // 2. Check for "Focus Aura" (3+ seconds of no activity)
    auto now = std::chrono::steady_clock::now();
    float inactiveSeconds = std::chrono::duration<float>(now - lastActivityTime_).count();

    if (inactiveSeconds >= 3.0f) {
        focusAuraActive_ = true;
    } else {
        focusAuraActive_ = false;
    }

    // Decay energy over time
    if (currentEnergy_ > 0.0f) {
        currentEnergy_ -= 10.0f * dt;
        if (currentEnergy_ < 0.0f) currentEnergy_ = 0.0f;
    }
}

void WorkEnergySystem::AddEnergy(float amount) {
    currentEnergy_ += amount;
    lastActivityTime_ = std::chrono::steady_clock::now();
}

void CALLBACK WorkEnergySystem::WinEventProc(HWINEVENTHOOK /*hWinEventHook*/, DWORD event, HWND /*hwnd*/, LONG /*idObject*/, LONG /*idChild*/, DWORD /*dwEventThread*/, DWORD /*dwmsEventTime*/) {
    if (event == EVENT_SYSTEM_FOREGROUND && s_Instance_) {
        // Active window changed! Give a burst of energy.
        s_Instance_->AddEnergy(50.0f);
    }
}

} // namespace Game
