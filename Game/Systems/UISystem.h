#pragma once
#include "ISystem.h"
#include <vector>
#include "../../externals/entt/entt.hpp"

namespace Game {

class UISystem : public ISystem {
public:
    struct WorldRect { float x, y, w, h; };

    void Update(entt::registry& registry, GameContext& ctx) override;
    void Draw(entt::registry& registry, GameContext& ctx) override;
    void DrawUI(entt::registry& registry, GameContext& ctx) override;
    void Reset(entt::registry& registry) override;

    static WorldRect CalculateWorldRect(entt::entity entity, entt::registry& registry, float screenW, float screenH);

    // ★追加: 3Dワールド座標からスクリーン座標に変換 (Viewport考慮版)
    static bool WorldToScreen(const DirectX::XMFLOAT3& worldPos, const Engine::Camera& camera, float& screenX, float& screenY);
    static bool WorldToScreenWithView(const DirectX::XMFLOAT3& worldPos, const Engine::Camera& camera, const DirectX::XMFLOAT2& viewOffset, const DirectX::XMFLOAT2& viewSize, float& screenX, float& screenY);

private:
    void RenderNodeWithRect(entt::entity entity, entt::registry& registry, const WorldRect& wr, GameContext& ctx);
    void DrawTextW(entt::entity entity, entt::registry& registry, const UITextComponent& text, float worldX, float worldY, float worldW, float worldH, Engine::Renderer* renderer);
    void ProcessButton(entt::entity entity, entt::registry& registry, UIButtonComponent& btn, float worldX, float worldY, float worldW, float worldH, GameContext& ctx);
    
    float deathTimer_ = 0.0f;

    // Command Room UI State
    bool isCommandRoomMinimized_ = false;
    int commandRoomPlacement_ = 0; // 0 = Bottom, 1 = Right
    int currentTab_ = 1; // 0 = Command, 1 = Shop, 2 = Status
    
    // Mock Game Data for Prototype
    int playerGold_ = 1200;
    int totalEnemiesDefeated_ = 19;
    int totalDamageDealt_ = 1150;
    int totalItemsAcquired_ = 8;
};

} // namespace Game
