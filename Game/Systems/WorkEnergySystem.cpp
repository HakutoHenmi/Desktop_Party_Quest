#include "WorkEnergySystem.h"
#include <cmath>
#include "../../Engine/Input.h"
#include "../../Engine/WindowDX.h"
#include "../Scenes/MainScene.h"

namespace Game {

WorkEnergySystem::WorkEnergySystem() {
    currentEnergy_ = 0.0f;
    focusAuraActive_ = false;
    lastActivityTime_ = std::chrono::steady_clock::now();
    GetCursorPos(&lastMousePos_);
    
    prevActiveWindow_ = Engine::WindowDX::IsActiveWindow();
    if (Engine::Input::GetInstance()) {
        prevKeyStrokes_ = Engine::Input::GetInstance()->GetTotalGlobalKeyStrokes();
        prevMouseClicks_ = Engine::Input::GetInstance()->GetTotalGlobalMouseClicks();
    }
}

WorkEnergySystem::~WorkEnergySystem() {
}

void WorkEnergySystem::Reset(entt::registry& /*registry*/) {
    currentEnergy_ = 0.0f;
    focusAuraActive_ = false;
    lastActivityTime_ = std::chrono::steady_clock::now();
    GetCursorPos(&lastMousePos_);
}

void WorkEnergySystem::Update(entt::registry& registry, GameContext& ctx) {
    float dt = ctx.dt;
    bool hasActivityThisFrame = false;

    // 1. Calculate mouse movement distance
    POINT currentMousePos;
    if (GetCursorPos(&currentMousePos)) {
        float dx = static_cast<float>(currentMousePos.x - lastMousePos_.x);
        float dy = static_cast<float>(currentMousePos.y - lastMousePos_.y);
        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance > 1.0f) {
            AddEnergy(distance * 0.01f); // マウス移動は少なめに設定
            lastMousePos_ = currentMousePos;
            hasActivityThisFrame = true;
        }
    }

    // 2. グローバルタイピング・マウスクリックからのエネルギー変換
    if (ctx.input) {
        int currentKeys = ctx.input->GetTotalGlobalKeyStrokes();
        int currentClicks = ctx.input->GetTotalGlobalMouseClicks();
        
        int keyDiff = currentKeys - prevKeyStrokes_;
        int clickDiff = currentClicks - prevMouseClicks_;
        
        if (keyDiff > 0) {
            AddEnergy(keyDiff * 2.0f); // 1タイプでエネルギー2
            hasActivityThisFrame = true;
        }
        if (clickDiff > 0) {
            AddEnergy(clickDiff * 1.5f); // 1クリックでエネルギー1.5
            hasActivityThisFrame = true;
        }
        
        prevKeyStrokes_ = currentKeys;
        prevMouseClicks_ = currentClicks;
    }

    // 3. アクティブウィンドウ切り替えでのエネルギー（集中ボーナス）
    bool currentActive = Engine::WindowDX::IsActiveWindow();
    if (currentActive != prevActiveWindow_) {
        // ウィンドウを切り替えたらボーナス（またはペナルティ等の拡張も可能）
        AddEnergy(10.0f); 
        hasActivityThisFrame = true;
        prevActiveWindow_ = currentActive;
    }

    // 4. Check for "Focus Aura" (3+ seconds of continuous activity without switching apps)
    // 今回は「活動状態」と「放置」の判定として使う
    auto now = std::chrono::steady_clock::now();
    float inactiveSeconds = std::chrono::duration<float>(now - lastActivityTime_).count();

    if (inactiveSeconds < 3.0f) {
        focusAuraActive_ = true;
    } else {
        focusAuraActive_ = false;
    }

    // Decay energy over time (何もしないと少しずつ減る)
    if (!hasActivityThisFrame && currentEnergy_ > 0.0f) {
        currentEnergy_ -= 1.0f * dt;
        if (currentEnergy_ < 0.0f) currentEnergy_ = 0.0f;
    }
    
    // 最大エネルギー制限
    if (currentEnergy_ > kMaxEnergy) {
        currentEnergy_ = kMaxEnergy;
    }

    // ★ UIへの反映（エネルギー値の更新）
    auto view = registry.view<NameComponent, UITextComponent>();
    for (auto entity : view) {
        auto& name = view.get<NameComponent>(entity);
        if (name.name == "CommandHeaderEV") {
            auto& text = view.get<UITextComponent>(entity);
            char buf[64];
            snprintf(buf, sizeof(buf), "%d / %d", static_cast<int>(currentEnergy_), static_cast<int>(kMaxEnergy));
            text.text = buf;
        } else if (name.name == "BottomText") {
            auto& text = view.get<UITextComponent>(entity);
            char buf[128];
            snprintf(buf, sizeof(buf), "Desktop Party Quest  |  作業エネルギー収集モード... [%d / %d]", static_cast<int>(currentEnergy_), static_cast<int>(kMaxEnergy));
            text.text = buf;
        } else if (name.name == "RightText") {
            auto& text = view.get<UITextComponent>(entity);
            char buf[128];
            snprintf(buf, sizeof(buf), "D\nP\nQ\n\n[%d\n/\n%d]", static_cast<int>(currentEnergy_), static_cast<int>(kMaxEnergy));
            text.text = buf;
        }
    }
}

void WorkEnergySystem::AddEnergy(float amount) {
    currentEnergy_ += amount;
    lastActivityTime_ = std::chrono::steady_clock::now();
}

} // namespace Game
