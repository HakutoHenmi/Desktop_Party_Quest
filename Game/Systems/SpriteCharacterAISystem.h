#pragma once
#include "ISystem.h"
#include <random>
#include <string>

namespace Game {

struct SpriteCharacterAIComponent : public Component {
    float changeDirTimer = 0.0f;
    float currentWaitTime = 2.0f;
    DirectX::XMFLOAT2 moveDir = {0, 0};
    float speed = 100.0f; // px/sec
    SpriteCharacterAIComponent() { type = static_cast<ComponentType>(1001); }
};

class SpriteCharacterAISystem : public ISystem {
public:
    void Update(entt::registry& registry, GameContext& ctx) override {
        // フル展開モードのときだけ動かす（必要に応じて）
        static float orbitTime = 0.0f;
        if (ctx.isStowed) orbitTime += ctx.dt;

        static bool wasStowed = false;
        bool justUnstowed = (wasStowed && !ctx.isStowed);

        auto view = registry.view<SpriteCharacterAIComponent, RectTransformComponent>();
        for (auto entity : view) {
            auto& ai = view.get<SpriteCharacterAIComponent>(entity);
            auto& rect = view.get<RectTransformComponent>(entity);
            if (!rect.enabled) continue;
            
            // TD用のヒーローはDesktopCombatSystemで制御するため、このシステムでは無視する
            if (registry.all_of<NameComponent>(entity)) {
                if (registry.get<NameComponent>(entity).name.find("Char_") == 0) {
                    continue;
                }
            }
            
            if (justUnstowed) {
                // 格納解除時に右端のUI裏に隠れないよう、画面左側にテレポートさせる
                rect.pos.x = 200.0f + (rand() % 600);
                rect.pos.y = 200.0f + (rand() % 600);
            }

            if (ctx.isStowed) {
                // オービタル（衛星）モード：マウスカーソルの周りを公転する
                POINT pt; GetCursorPos(&pt);
                float mouseX = static_cast<float>(pt.x);
                float mouseY = static_cast<float>(pt.y);
                
                float maxRadius = 60.0f; // マウスカーソルからの最大許容距離
                
                float cx = rect.pos.x + rect.size.x / 2.0f;
                float cy = rect.pos.y + rect.size.y / 2.0f;
                float dx = cx - mouseX;
                float dy = cy - mouseY;
                float dist = std::sqrt(dx*dx + dy*dy);
                
                // 枠からはみ出たら引っ張る（リードのような挙動）
                if (dist > maxRadius) {
                    cx = mouseX + (dx / dist) * maxRadius;
                    cy = mouseY + (dy / dist) * maxRadius;
                    rect.pos.x = cx - rect.size.x / 2.0f;
                    rect.pos.y = cy - rect.size.y / 2.0f;
                }
            } else {
                // 通常の徘徊AI
                ai.changeDirTimer -= ctx.dt;
                if (ai.changeDirTimer <= 0.0f) {
                    int r = rand() % 100;
                    if (r < 30) {
                        ai.moveDir = {0, 0};
                        ai.currentWaitTime = 1.0f + (rand() % 20) / 10.0f;
                    } else {
                        float angle = (rand() % 360) * 3.14159f / 180.0f;
                        ai.moveDir.x = std::cos(angle);
                        ai.moveDir.y = std::sin(angle);
                        ai.currentWaitTime = 2.0f + (rand() % 30) / 10.0f;
                    }
                    ai.changeDirTimer = ai.currentWaitTime;
                }

                rect.pos.x += ai.moveDir.x * ai.speed * ctx.dt;
                rect.pos.y += ai.moveDir.y * ai.speed * ctx.dt;

                // 画面外に出ないようにクランプ
                float screenW = ctx.viewportSize.x > 0 ? ctx.viewportSize.x : 1920.0f;
                float screenH = ctx.viewportSize.y > 0 ? ctx.viewportSize.y : 1080.0f;
                
                // 下部のUIに被らないように適当に制限 (高さ1080-タスクバーなど)
                float maxY = screenH - 100.0f; 
                
                if (rect.pos.x < 0) rect.pos.x = 0;
                if (rect.pos.x > screenW - rect.size.x) rect.pos.x = screenW - rect.size.x;
                if (rect.pos.y < 0) rect.pos.y = 0;
                if (rect.pos.y > maxY - rect.size.y) rect.pos.y = maxY - rect.size.y;
            }
        }
        
        wasStowed = ctx.isStowed;
    }

    void Reset(entt::registry& registry) override {
        (void)registry;
    }
};

} // namespace Game
