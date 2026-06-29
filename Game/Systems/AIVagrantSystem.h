#pragma once
#include "ISystem.h"
#include <random>

namespace Game {

struct AIVagrantComponent : public Component {
    float changeDirTimer = 0.0f;
    float currentWaitTime = 2.0f;
    AIVagrantComponent() { type = static_cast<ComponentType>(1000); }
};

class AIVagrantSystem : public ISystem {
public:
    void Update(entt::registry& registry, GameContext& ctx) override {
        if (!ctx.isPlaying) return;

        auto view = registry.view<AIVagrantComponent, PlayerInputComponent>();
        for (auto entity : view) {
            auto& ai = view.get<AIVagrantComponent>(entity);
            auto& input = view.get<PlayerInputComponent>(entity);
            
            ai.changeDirTimer -= ctx.dt;
            if (ai.changeDirTimer <= 0.0f) {
                // 方向をランダムに変更
                int r = rand() % 100;
                if (r < 30) {
                    // 止まる
                    input.moveDir = {0, 0};
                    ai.currentWaitTime = 1.0f + (rand() % 20) / 10.0f;
                } else {
                    // 動く
                    float angle = (rand() % 360) * 3.14159f / 180.0f;
                    input.moveDir.x = std::cos(angle);
                    input.moveDir.y = std::sin(angle);
                    ai.currentWaitTime = 2.0f + (rand() % 30) / 10.0f;
                }
                ai.changeDirTimer = ai.currentWaitTime;
            }
        }
    }

    void Reset(entt::registry& registry) override {}
};

} // namespace Game
