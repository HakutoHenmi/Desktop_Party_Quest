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

    // Decay energy over time (何もしないと少しずつ減る)
    if (!hasActivityThisFrame && currentEnergy_ > 0.0f) {
        currentEnergy_ -= 1.0f * dt;
        if (currentEnergy_ < 0.0f) currentEnergy_ = 0.0f;
    }
    
    // アクティブ支援発動 (エネルギーが100に達したら即時発動)
    if (currentEnergy_ >= kMaxEnergy) {
        currentEnergy_ -= kMaxEnergy;
        
        if (auto mainScene = dynamic_cast<MainScene*>(ctx.scene)) {
            mainScene->AddLog(">>> エネルギー充填完了！エンジニアがシステムブーストを発動！");
        }
        
        float spawnX = 1000.0f;
        float spawnY = 500.0f;
        auto engView = registry.view<DesktopHeroComponent, RectTransformComponent>();
        bool engineerFound = false;
        
        for (auto e : engView) {
            if (engView.get<DesktopHeroComponent>(e).role == HeroRole::Engineer) {
                auto& r = engView.get<RectTransformComponent>(e);
                spawnX = r.pos.x + r.size.x / 2.0f;
                spawnY = r.pos.y - 30.0f;
                engineerFound = true;
                break;
            }
        }
        
        // 演出テキスト（エンジニアの頭上）
        if (engineerFound && !ctx.isStowed) {
            auto eff = registry.create();
            auto& t = registry.emplace<RectTransformComponent>(eff);
            t.anchor = {0.0f, 0.0f}; t.pivot = {0.5f, 0.5f};
            t.pos = {spawnX, spawnY};
            t.size = {200, 40};
            auto& txt = registry.emplace<UITextComponent>(eff);
            txt.text = "[ SYSTEM BOOST! ]";
            txt.fontSize = 24.0f;
            txt.color = {0.2f, 1.0f, 0.5f, 1.0f}; // 蛍光グリーン
            
            auto& dmgText = registry.emplace<DesktopDamageTextComponent>(eff);
            dmgText.lifetime = 2.0f;
            dmgText.maxLifetime = 2.0f;
            dmgText.velocity = {0, -30.0f};
            
            // ☕の代わりに回復オーラの画像エフェクトを重ねる
            auto imgEff = registry.create();
            auto& tImg = registry.emplace<RectTransformComponent>(imgEff);
            tImg.anchor = {0.0f, 0.0f}; tImg.pivot = {0.5f, 0.5f};
            tImg.pos = {spawnX, spawnY - 10.0f};
            tImg.size = {64, 64};
            auto& img = registry.emplace<UIImageComponent>(imgEff);
            img.texturePath = "Resources/Textures/soft_circle.png";
            img.color = {0.2f, 0.8f, 1.0f, 1.0f}; // 水色
            img.layer = -6;
            if (ctx.renderer) img.textureHandle = ctx.renderer->LoadTexture2D(img.texturePath);
            auto& dmgTextImg = registry.emplace<DesktopDamageTextComponent>(imgEff);
            dmgTextImg.lifetime = 2.0f;
            dmgTextImg.maxLifetime = 2.0f;
            dmgTextImg.velocity = {0, -40.0f};
        }
        
        // パーティ全体を回復
        for (auto e : engView) {
            auto& hero = engView.get<DesktopHeroComponent>(e);
            hero.hp += 50.0f; // 大回復
            if (hero.hp > hero.maxHp) hero.hp = hero.maxHp;
            
            if (!ctx.isStowed) {
                auto& r = engView.get<RectTransformComponent>(e);
                // 個別の回復エフェクト
                auto eff2 = registry.create();
                auto& t2 = registry.emplace<RectTransformComponent>(eff2);
                t2.anchor = {0.0f, 0.0f}; t2.pivot = {0.5f, 0.5f};
                t2.pos = {r.pos.x + r.size.x/2, r.pos.y + r.size.y/2 - 20.0f};
                t2.size = {100, 30};
                auto& txt2 = registry.emplace<UITextComponent>(eff2);
                txt2.text = "+50";
                txt2.fontSize = 20.0f;
                txt2.color = {0.2f, 1.0f, 0.2f, 1.0f};
                registry.emplace<DesktopDamageTextComponent>(eff2);
            }
        }
    }

    // ★ UIへの反映（エネルギー値の更新）
    auto view = registry.view<NameComponent, UITextComponent>();
    for (auto entity : view) {
        auto& name = view.get<NameComponent>(entity);
        if (name.name == "CommandHeaderWEV") {
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
