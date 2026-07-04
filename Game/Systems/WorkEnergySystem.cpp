#include "WorkEnergySystem.h"
#include <cmath>
#include "../../Engine/Input.h"
#include "../../Engine/WindowDX.h"
#include "../Scenes/MainScene.h"
#include "DesktopCombatSystem.h"

namespace Game {

WorkEnergySystem::WorkEnergySystem() {
    currentEnergy_ = 0.0f;
    focusAuraActive_ = false;
    lastActivityTime_ = std::chrono::steady_clock::now();
    lastInputTime_ = std::chrono::steady_clock::now();
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
        
        if (keyDiff > 0 || clickDiff > 0) {
            auto now = std::chrono::steady_clock::now();
            float inputElapsed = std::chrono::duration<float>(now - lastInputTime_).count();
            
            // 冷却ペナルティ: 異常な連打（例：0.05秒以内の連続入力）は減衰させるか無視する
            if (inputElapsed < 0.05f && (keyDiff + clickDiff) > 2) {
                // スパムペナルティ
                currentEnergy_ -= 0.5f;
            } else {
                float add = (keyDiff * 2.0f) + (clickDiff * 1.5f);
                AddEnergy(add);
            }
            hasActivityThisFrame = true;
            lastInputTime_ = now;
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

    bool prevFocus = focusAuraActive_;
    if (inactiveSeconds >= 3.0f) {
        focusAuraActive_ = true;
    } else {
        focusAuraActive_ = false;
    }

    if (focusAuraActive_ && !prevFocus) {
        if (auto mainScene = dynamic_cast<MainScene*>(ctx.scene)) {
            mainScene->AddLog(">>> 作業への集中を検知。フォーカスオーラ発動！");
        }
    }

    // ヒーラー等のキャラにオーラ状態を同期
    auto heroView = registry.view<DesktopHeroComponent>();
    for (auto e : heroView) {
        if (focusAuraActive_) {
            if (!registry.all_of<FocusAuraTag>(e)) registry.emplace<FocusAuraTag>(e);
        } else {
            if (registry.all_of<FocusAuraTag>(e)) registry.remove<FocusAuraTag>(e);
        }
    }

    // Decay energy は溜めることができるように削除

    // 宝箱獲得判定
    auto chestViewProg = registry.view<DesktopPartyProgressComponent>();
    for (auto entity : chestViewProg) {
        auto& prog = registry.get<DesktopPartyProgressComponent>(entity);
        
        while (currentEnergy_ >= prog.chestEnergyTarget) {
            currentEnergy_ -= prog.chestEnergyTarget;
            prog.chestEnergyTarget += 50;
            prog.availableChests++;
            
            if (auto mainScene = dynamic_cast<MainScene*>(ctx.scene)) {
                mainScene->AddLog(">>> 作業エネルギーが目標に到達！ 宝箱を1つ獲得しました。");
            }
        }
        break;
    }

    // ★ UIへの反映（エネルギー値の更新）
    int targetEnergy = 50;
    auto targetViewProg = registry.view<DesktopPartyProgressComponent>();
    targetViewProg.each([&](auto /*entity*/, auto& prog) {
        targetEnergy = prog.chestEnergyTarget;
    });

    auto uiView = registry.view<NameComponent, UITextComponent>();
    uiView.each([&](auto /*entity*/, auto& name, auto& text) {
        if (name.name == "CommandHeaderWEV") {
            char buf[64];
            snprintf(buf, sizeof(buf), "%d / %d", static_cast<int>(currentEnergy_), targetEnergy);
            text.text = buf;
        } else if (name.name == "BottomText") {
            char buf[128];
            snprintf(buf, sizeof(buf), "Desktop Party Quest  |  作業エネルギー収集モード... [%d / %d]", static_cast<int>(currentEnergy_), targetEnergy);
            text.text = buf;
        } else if (name.name == "RightText") {
            char buf[128];
            snprintf(buf, sizeof(buf), "D\nP\nQ\n\n[%d\n/\n%d]", static_cast<int>(currentEnergy_), targetEnergy);
            text.text = buf;
        }
    });
}

void WorkEnergySystem::AddEnergy(float amount) {
    currentEnergy_ += amount;
    lastActivityTime_ = std::chrono::steady_clock::now();
}

} // namespace Game
