#pragma once
#include "ISystem.h"
#include <random>
#include <string>
#include "SpriteCharacterAISystem.h"

namespace Game {

struct DesktopEnemyComponent : public Component {
    float hp = 100.0f;
    float maxHp = 100.0f;
    entt::entity hpBarBg = entt::null;
    entt::entity hpBarFg = entt::null;
    float attackTimer = 1.0f;
    DesktopEnemyComponent() { type = static_cast<ComponentType>(1002); }
};

enum class HeroRole {
    Tank,
    Sniper,
    Scout,
    Engineer,
    Healer
};

struct DesktopHeroComponent : public Component {
    HeroRole role = HeroRole::Tank;
    float hp = 100.0f;
    float maxHp = 100.0f;
    entt::entity hpBarBg = entt::null;
    entt::entity hpBarFg = entt::null;
    entt::entity weaponEntity = entt::null;
    float attackTimer = 0.0f;
    DesktopHeroComponent() { type = static_cast<ComponentType>(1004); }
};

struct DesktopDamageTextComponent : public Component {
    float lifetime = 1.0f;
    float maxLifetime = 1.0f;
    DirectX::XMFLOAT2 velocity = {0, -50.0f};
    DesktopDamageTextComponent() { type = static_cast<ComponentType>(1003); }
};

enum class PartyAIMode {
    Attack,
    Collect,
    Formation
};

class DesktopCombatSystem : public ISystem {
public:
    float spawnTimer = 5.0f; // 5秒後に最初の敵出現
    
    PartyAIMode currentAiMode = PartyAIMode::Formation;
    bool isCoreFever = false;
    float coreFeverTimer = 0.0f;
    
    // --- Economy & Stats ---
    int currentGold = 300;
    int totalKills = 0;
    float totalDamage = 0.0f;
    int totalGoldEarned = 300;
    int totalItems = 0;
    
    // 0:Tank, 1:Sniper, 2:Scout, 3:Engineer
    int heroLevels[4] = {1, 1, 1, 1};
    
    // 0:Normal, 1:Cyber, 2:Antique, 3:Gold
    int currentTheme = 0;
    bool themeOwned[4] = {true, false, false, false};
    
    void TriggerCoreFever() {
        isCoreFever = true;
        coreFeverTimer = 5.0f;
    }

    entt::entity CreateDamageText(entt::registry& reg, float x, float y, float damage) {
        auto ent = reg.create();
        auto& t = reg.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f};
        t.pivot = {0.0f, 0.0f};
        t.pos = {x, y};
        t.size = {100, 30};
        
        auto& txt = reg.emplace<UITextComponent>(ent);
        txt.text = std::to_string((int)damage);
        txt.fontSize = 24.0f;
        txt.color = {1.0f, 0.2f, 0.2f, 1.0f}; // 赤
        
        reg.emplace<DesktopDamageTextComponent>(ent);
        return ent;
    }

    entt::entity CreateDamageParticle(entt::registry& reg, GameContext& ctx, float x, float y, const std::string& tex, DirectX::XMFLOAT4 col, float scale = 48.0f) {
        auto ent = reg.create();
        auto& t = reg.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f};
        t.pivot = {0.5f, 0.5f};
        t.pos = {x, y};
        t.size = {scale, scale};
        
        auto& img = reg.emplace<UIImageComponent>(ent);
        img.texturePath = tex;
        img.color = col;
        img.layer = -6; // 手前
        if (ctx.renderer) img.textureHandle = ctx.renderer->LoadTexture2D(img.texturePath);
        
        auto& dmgText = reg.emplace<DesktopDamageTextComponent>(ent);
        dmgText.lifetime = 1.0f;
        dmgText.maxLifetime = 1.0f;
        dmgText.velocity = {0, -50.0f};
        return ent;
    }

    entt::entity CreateHealText(entt::registry& reg, float x, float y, float amount) {
        auto ent = reg.create();
        auto& t = reg.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f};
        t.pivot = {0.0f, 0.0f};
        t.pos = {x, y};
        t.size = {100, 30};
        
        auto& txt = reg.emplace<UITextComponent>(ent);
        txt.text = "+" + std::to_string((int)amount);
        txt.fontSize = 24.0f;
        txt.color = {0.2f, 1.0f, 0.2f, 1.0f}; // 緑
        
        reg.emplace<DesktopDamageTextComponent>(ent);
        return ent;
    }

public:
    void Update(entt::registry& registry, GameContext& ctx) override {
        if (!ctx.isPlaying) return;
        
        // --- 0. コア・フィーバーのタイマー更新 ---
        if (isCoreFever) {
            coreFeverTimer -= ctx.dt;
            if (coreFeverTimer <= 0.0f) {
                isCoreFever = false;
            }
        }
        
        static bool wasStowed = false;
        bool justUnstowed = (wasStowed && !ctx.isStowed);

        // --- ★オービタル（格納）モードにおける強制サイズ変更と敵の追従 ---
        float targetScale = ctx.isStowed ? 16.0f : 128.0f;
        auto sizeView = registry.view<RectTransformComponent>();
        
        POINT pt; GetCursorPos(&pt);
        float mouseX = static_cast<float>(pt.x);
        float mouseY = static_cast<float>(pt.y);
        
        for (auto e : sizeView) {
            if (registry.all_of<DesktopHeroComponent>(e) || registry.all_of<DesktopEnemyComponent>(e)) {
                auto& rect = sizeView.get<RectTransformComponent>(e);
                rect.size = {targetScale, targetScale};
                rect.enabled = true; // 格納中でも非表示にしない
                
                // 敵をマウスカーソル半径内にクランプ（はみ出したら引っ張る）
                if (ctx.isStowed && registry.all_of<DesktopEnemyComponent>(e)) {
                    float cx = rect.pos.x + rect.size.x / 2.0f;
                    float cy = rect.pos.y + rect.size.y / 2.0f;
                    float dx = cx - mouseX;
                    float dy = cy - mouseY;
                    float dist = std::sqrt(dx*dx + dy*dy);
                    float maxRadius = 60.0f;
                    if (dist > maxRadius) {
                        cx = mouseX + (dx / dist) * maxRadius;
                        cy = mouseY + (dy / dist) * maxRadius;
                        rect.pos.x = cx - rect.size.x / 2.0f;
                        rect.pos.y = cy - rect.size.y / 2.0f;
                    }
                }
                
                if (justUnstowed && registry.all_of<DesktopEnemyComponent>(e)) {
                    // 展開時に右のUI裏に隠れないよう、敵も左側にテレポートさせる
                    rect.pos.x = 200.0f + (rand() % 600);
                    rect.pos.y = 200.0f + (rand() % 600);
                }
            }
        }
        
        // --- 散らばり（分離）処理 ---
        if (ctx.isStowed) {
            std::vector<entt::entity> orbitalEntities;
            for (auto e : sizeView) {
                if (registry.all_of<DesktopHeroComponent>(e) || registry.all_of<DesktopEnemyComponent>(e)) {
                    orbitalEntities.push_back(e);
                }
            }
            
            for (auto e : orbitalEntities) {
                auto& rect = sizeView.get<RectTransformComponent>(e);
                float cx = rect.pos.x + rect.size.x / 2.0f;
                float cy = rect.pos.y + rect.size.y / 2.0f;
                
                for (auto other : orbitalEntities) {
                    if (e == other) continue;
                    auto& oRect = sizeView.get<RectTransformComponent>(other);
                    float ox = oRect.pos.x + oRect.size.x / 2.0f;
                    float oy = oRect.pos.y + oRect.size.y / 2.0f;
                    float dx = cx - ox;
                    float dy = cy - oy;
                    float dist = std::sqrt(dx*dx + dy*dy);
                    
                    if (dist < 16.0f) { // キャラが重ならないように弾く距離
                        if (dist < 0.001f) {
                            dx = static_cast<float>((rand() % 100) - 50);
                            dy = static_cast<float>((rand() % 100) - 50);
                            dist = std::sqrt(dx*dx + dy*dy) + 0.001f;
                        }
                        float pushForce = (16.0f - dist) * 15.0f; // 強い弾き力
                        rect.pos.x += (dx / dist) * pushForce * ctx.dt;
                        rect.pos.y += (dy / dist) * pushForce * ctx.dt;
                    }
                }
            }
        }

        // --- 1. 敵の出現管理 ---
        auto enemyView = registry.view<DesktopEnemyComponent, RectTransformComponent>();
        int activeEnemyCount = 0;
        for (auto it = enemyView.begin(); it != enemyView.end(); ++it) activeEnemyCount++;

        spawnTimer -= ctx.dt;
        if (activeEnemyCount < 3 && spawnTimer <= 0.0f) {
            // 敵を生成
            entt::entity newEnemy = registry.create();
            registry.emplace<NameComponent>(newEnemy, "Enemy_Virus");
            
            auto& rect = registry.emplace<RectTransformComponent>(newEnemy);
            rect.enabled = true; // 常に表示
            rect.anchor = {0.0f, 0.0f};
            rect.pivot = {0.0f, 0.0f};
            
            float spawnX, spawnY;
            if (ctx.isStowed) {
                spawnX = mouseX + static_cast<float>(rand() % 100 - 50);
                spawnY = mouseY + static_cast<float>(rand() % 100 - 50);
            } else {
                float screenW = ctx.viewportSize.x > 0 ? ctx.viewportSize.x : 1920.0f;
                float screenH = ctx.viewportSize.y > 0 ? ctx.viewportSize.y : 1080.0f;
                float maxSpawnX = (std::max)(100.0f, screenW - 700.0f);
                float maxSpawnY = (std::max)(100.0f, screenH - 200.0f);
                spawnX = 100.0f + (rand() % (int)maxSpawnX);
                spawnY = 100.0f + (rand() % (int)maxSpawnY);
            }
            rect.pos = {spawnX, spawnY};
            rect.size = {targetScale, targetScale};
            
            auto& img = registry.emplace<UIImageComponent>(newEnemy);
            img.texturePath = "Resources/Textures/Characters/enemy_virus.png";
            if (ctx.renderer) {
                img.textureHandle = ctx.renderer->LoadTexture2D(img.texturePath, false);
            }
            img.layer = -4; // キャラより少し手前に変更 (見えやすくするため)
            
            auto& enemy = registry.emplace<DesktopEnemyComponent>(newEnemy);
            enemy.hp = 300.0f;
            enemy.maxHp = 300.0f;
            
            // HPバー（背景）
            enemy.hpBarBg = registry.create();
            auto& bgRect = registry.emplace<RectTransformComponent>(enemy.hpBarBg);
            bgRect.enabled = !ctx.isStowed;
            bgRect.anchor = {0.0f, 0.0f};
            bgRect.pivot = {0.0f, 0.0f};
            bgRect.size = {80, 10};
            auto& bgImg = registry.emplace<UIImageComponent>(enemy.hpBarBg);
            bgImg.texturePath = "Resources/Textures/white1x1.png";
            bgImg.color = {0.2f, 0.2f, 0.2f, 1.0f};
            bgImg.layer = -3;
            if (ctx.renderer) bgImg.textureHandle = ctx.renderer->LoadTexture2D(bgImg.texturePath);
            
            // HPバー（前景）
            enemy.hpBarFg = registry.create();
            auto& fgRect = registry.emplace<RectTransformComponent>(enemy.hpBarFg);
            fgRect.enabled = !ctx.isStowed;
            fgRect.anchor = {0.0f, 0.0f};
            fgRect.pivot = {0.0f, 0.0f};
            fgRect.size = {80, 10};
            auto& fgImg = registry.emplace<UIImageComponent>(enemy.hpBarFg);
            fgImg.texturePath = "Resources/Textures/white1x1.png";
            fgImg.color = {0.9f, 0.2f, 0.2f, 1.0f}; // 敵のHPは赤
            fgImg.layer = -2; // 背景(-3)より手前に描画
            if (ctx.renderer) fgImg.textureHandle = ctx.renderer->LoadTexture2D(fgImg.texturePath);
            
            spawnTimer = 3.0f; // 次の出現までの時間
        }

        std::vector<entt::entity> enemiesToDestroy;
        for (auto currentEnemy : enemyView) {
            // 敵が存在する場合、HPバーの位置を更新
            auto& enemy = registry.get<DesktopEnemyComponent>(currentEnemy);
            auto& enemyRect = registry.get<RectTransformComponent>(currentEnemy);
            
            if (registry.valid(enemy.hpBarBg)) {
                auto& bgRect = registry.get<RectTransformComponent>(enemy.hpBarBg);
                bgRect.pos = {enemyRect.pos.x + 24.0f, enemyRect.pos.y - 15.0f};
            }
            if (registry.valid(enemy.hpBarFg)) {
                auto& fgRect = registry.get<RectTransformComponent>(enemy.hpBarFg);
                fgRect.pos = {enemyRect.pos.x + 24.0f, enemyRect.pos.y - 15.0f};
                fgRect.size.x = 80.0f * (enemy.hp / enemy.maxHp);
            }
            
            // HP0で死亡
            if (enemy.hp <= 0.0f) {
                float goldGained = 50.0f;
                auto auraHeroes = registry.view<FocusAuraTag>();
                if (!auraHeroes.empty()) goldGained *= 1.5f; // オーラ時は獲得ゴールドもアップ
                
                currentGold += (int)goldGained;
                totalGoldEarned += (int)goldGained;
                totalKills += 1;
                
                auto eff = CreateDamageText(registry, enemyRect.pos.x, enemyRect.pos.y, 0);
                registry.get<UITextComponent>(eff).text = "GOLD +" + std::to_string((int)goldGained);
                registry.get<UITextComponent>(eff).color = {0.8f, 0.6f, 0.1f, 1.0f};
                if (ctx.isStowed) registry.get<UITextComponent>(eff).fontSize = 12.0f;

                if (registry.valid(enemy.hpBarBg)) registry.destroy(enemy.hpBarBg);
                if (registry.valid(enemy.hpBarFg)) registry.destroy(enemy.hpBarFg);
                enemiesToDestroy.push_back(currentEnemy);
            } else {
                // 敵の反撃
                enemy.attackTimer -= ctx.dt;
                if (enemy.attackTimer <= 0.0f) {
                    enemy.attackTimer = 2.0f; // 2秒に1回攻撃
                    // ランダムなキャラにダメージ
                    auto heroView = registry.view<SpriteCharacterAIComponent, RectTransformComponent, DesktopHeroComponent>();
                    std::vector<entt::entity> heroes;
                    for (auto h : heroView) heroes.push_back(h);
                    if (!heroes.empty()) {
                        entt::entity target = heroes[rand() % heroes.size()];
                        auto& hero = registry.get<DesktopHeroComponent>(target);
                        auto& hRect = registry.get<RectTransformComponent>(target);
                        float dmg = 5.0f + (rand() % 5);
                        
                        // 集中オーラ（静の評価）による防御ボーナス
                        if (registry.all_of<FocusAuraTag>(target)) {
                            dmg *= 0.5f; 
                        }
                        
                        hero.hp -= dmg;
                        float tx = hRect.pos.x + 32.0f + (rand() % 40 - 20.0f);
                        float ty = hRect.pos.y - 20.0f + (rand() % 40 - 20.0f);
                        auto eff = CreateDamageText(registry, tx, ty, dmg);
                        registry.get<UITextComponent>(eff).color = {1.0f, 0.2f, 0.2f, 1.0f}; // 被ダメージは赤色
                        if (ctx.isStowed) registry.get<UITextComponent>(eff).fontSize = 10.0f;
                    }
                }
            }
        }
        for (auto e : enemiesToDestroy) {
            registry.destroy(e);
        }

        // --- 1.5. 味方の初期化とタンク位置の取得 ---
        float tankX = 0.0f, tankY = 0.0f;
        entt::entity tankEntity = entt::null;
        auto charView = registry.view<SpriteCharacterAIComponent, RectTransformComponent>();
        
        for (auto entity : charView) {
            auto& rect = charView.get<RectTransformComponent>(entity);
            if (!rect.enabled) continue;

            // 味方のHP管理コンポーネントがなければ追加
            if (!registry.all_of<DesktopHeroComponent>(entity)) {
                auto& hero = registry.emplace<DesktopHeroComponent>(entity);
                auto name = registry.all_of<NameComponent>(entity) ? registry.get<NameComponent>(entity).name : "";
                
                std::string weaponTex = "";
                if (name == "Char_Tank") { hero.role = HeroRole::Tank; hero.maxHp = 150.0f; hero.hp = 150.0f; weaponTex = "Resources/Textures/Weapons/weapon_shield_t.png"; }
                else if (name == "Char_Sniper") { hero.role = HeroRole::Sniper; hero.maxHp = 80.0f; hero.hp = 80.0f; weaponTex = "Resources/Textures/Weapons/weapon_gun_t.png"; }
                else if (name == "Char_Scout") { hero.role = HeroRole::Scout; hero.maxHp = 100.0f; hero.hp = 100.0f; weaponTex = "Resources/Textures/Weapons/weapon_sword_t.png"; }
                else if (name == "Char_Engineer") { hero.role = HeroRole::Engineer; hero.maxHp = 100.0f; hero.hp = 100.0f; weaponTex = "Resources/Textures/Weapons/weapon_spanner_t.png"; }
                else if (name == "Char_Healer") { hero.role = HeroRole::Healer; hero.maxHp = 90.0f; hero.hp = 90.0f; weaponTex = "Resources/Textures/Weapons/weapon_staff_t.png"; }
                
                hero.hpBarBg = registry.create();
                auto& bgRect = registry.emplace<RectTransformComponent>(hero.hpBarBg);
                bgRect.enabled = !ctx.isStowed;
                bgRect.anchor = {0.0f, 0.0f}; bgRect.pivot = {0.0f, 0.0f}; bgRect.size = {60, 8};
                auto& bgImg = registry.emplace<UIImageComponent>(hero.hpBarBg);
                bgImg.texturePath = "Resources/Textures/white1x1.png";
                bgImg.color = {0.2f, 0.2f, 0.2f, 1.0f};
                bgImg.layer = -4;
                if (ctx.renderer) bgImg.textureHandle = ctx.renderer->LoadTexture2D(bgImg.texturePath);
                
                hero.hpBarFg = registry.create();
                auto& fgRect = registry.emplace<RectTransformComponent>(hero.hpBarFg);
                fgRect.enabled = !ctx.isStowed;
                fgRect.anchor = {0.0f, 0.0f}; fgRect.pivot = {0.0f, 0.0f}; fgRect.size = {60, 8};
                auto& fgImg = registry.emplace<UIImageComponent>(hero.hpBarFg);
                fgImg.texturePath = "Resources/Textures/white1x1.png";
                fgImg.color = {0.2f, 0.9f, 0.2f, 1.0f};
                fgImg.layer = -2;
                if (ctx.renderer) fgImg.textureHandle = ctx.renderer->LoadTexture2D(fgImg.texturePath);
                
                if (!weaponTex.empty()) {
                    hero.weaponEntity = registry.create();
                    auto& wRect = registry.emplace<RectTransformComponent>(hero.weaponEntity);
                    wRect.enabled = true;
                    wRect.anchor = {0.0f, 0.0f}; wRect.pivot = {0.5f, 0.5f}; wRect.size = {64, 64};
                    auto& wImg = registry.emplace<UIImageComponent>(hero.weaponEntity);
                    wImg.texturePath = weaponTex;
                    wImg.color = {1.0f, 1.0f, 1.0f, 1.0f};
                    wImg.layer = -6;
                    if (ctx.renderer) wImg.textureHandle = ctx.renderer->LoadTexture2D(wImg.texturePath, false);
                }
            }

            auto& hero = registry.get<DesktopHeroComponent>(entity);
            if (hero.role == HeroRole::Tank) {
                tankX = rect.pos.x + rect.size.x / 2.0f;
                tankY = rect.pos.y + rect.size.y / 2.0f;
                tankEntity = entity;
            }
        }

        // --- 2. キャラクターのAI制御＆味方HP管理 ---
        for (auto entity : charView) {
            auto& ai = charView.get<SpriteCharacterAIComponent>(entity);
            auto& rect = charView.get<RectTransformComponent>(entity);
            if (!rect.enabled) continue;

            auto& hero = registry.get<DesktopHeroComponent>(entity);
            
            // 味方のHPバー位置更新
            if (registry.valid(hero.hpBarBg)) {
                auto& bgRect = registry.get<RectTransformComponent>(hero.hpBarBg);
                bgRect.pos = {rect.pos.x + 34.0f, rect.pos.y - 10.0f};
            }
            if (registry.valid(hero.hpBarFg)) {
                auto& fgRect = registry.get<RectTransformComponent>(hero.hpBarFg);
                fgRect.pos = {rect.pos.x + 34.0f, rect.pos.y - 10.0f};
                fgRect.size.x = 60.0f * (hero.hp / hero.maxHp);
            }
            
            // 武器位置の更新
            if (registry.valid(hero.weaponEntity)) {
                auto& wRect = registry.get<RectTransformComponent>(hero.weaponEntity);
                float scaleRatio = ctx.isStowed ? (16.0f / 128.0f) : 1.0f;
                float offsetX = 48.0f;
                float offsetY = 16.0f;
                
                float attackInterval = (hero.role == HeroRole::Sniper) ? 2.5f : ((hero.role == HeroRole::Scout) ? 0.8f : 1.5f);
                if (hero.role == HeroRole::Healer) attackInterval = 1.5f;
                
                if (hero.attackTimer > attackInterval - 0.2f && hero.attackTimer > 0.0f) {
                    float attackAnim = (hero.attackTimer - (attackInterval - 0.2f)) / 0.2f;
                    offsetX += 30.0f * attackAnim;
                }
                
                wRect.size = {64.0f * scaleRatio, 64.0f * scaleRatio};
                wRect.pos = {rect.pos.x + rect.size.x/2.0f + offsetX * scaleRatio, rect.pos.y + rect.size.y/2.0f + offsetY * scaleRatio};
                wRect.enabled = rect.enabled;
            }

            entt::entity currentEnemy = entt::null;
            float closestDist = 999999.0f;
            for (auto e : enemyView) {
                auto& eRect = registry.get<RectTransformComponent>(e);
                float dx = (eRect.pos.x + eRect.size.x/2.0f) - (rect.pos.x + rect.size.x/2.0f);
                float dy = (eRect.pos.y + eRect.size.y/2.0f) - (rect.pos.y + rect.size.y/2.0f);
                float dist = dx*dx + dy*dy;
                if (dist < closestDist) {
                    closestDist = dist;
                    currentEnemy = e;
                }
            }

            float cx = rect.pos.x + rect.size.x / 2.0f;
            float cy = rect.pos.y + rect.size.y / 2.0f;
            
            float targetX = cx;
            float targetY = cy;
            bool shouldMoveToTarget = true;
            bool shouldAttackTarget = false;
            float attackRange = 80.0f;
            float attackDmg = 10.0f;

            if (hero.role == HeroRole::Healer) {
                hero.attackTimer -= ctx.dt;
                entt::entity lowestHpTarget = entt::null;
                float lowestRatio = 1.0f;
                
                auto hView = registry.view<DesktopHeroComponent, RectTransformComponent>();
                for (auto he : hView) {
                    auto& th = hView.get<DesktopHeroComponent>(he);
                    float ratio = th.hp / th.maxHp;
                    if (ratio < lowestRatio && th.hp > 0.0f && th.hp < th.maxHp) {
                        lowestRatio = ratio;
                        lowestHpTarget = he;
                    }
                }
                
                if (lowestRatio < 0.9f && registry.valid(lowestHpTarget)) {
                    auto& tRect = registry.get<RectTransformComponent>(lowestHpTarget);
                    targetX = tRect.pos.x + tRect.size.x / 2.0f;
                    targetY = tRect.pos.y + tRect.size.y / 2.0f;
                    float dx = targetX - cx; float dy = targetY - cy;
                    float dist = std::sqrt(dx*dx + dy*dy);
                    
                    if (dist <= 80.0f || ctx.isStowed) {
                        if (hero.attackTimer <= 0.0f) {
                            auto& th = registry.get<DesktopHeroComponent>(lowestHpTarget);
                            float healAmt = 25.0f;
                            th.hp += healAmt;
                            if (th.hp > th.maxHp) th.hp = th.maxHp;
                            auto eff = CreateHealText(registry, targetX - 20.0f + (rand() % 40 - 20.0f), targetY - 40.0f + (rand() % 40 - 20.0f), healAmt);
                            CreateDamageParticle(registry, ctx, cx, cy - 20.0f, "Resources/Textures/soft_circle.png", {0.2f, 1.0f, 0.2f, 1.0f}, ctx.isStowed ? 16.0f : 48.0f);
                            if (ctx.isStowed) registry.get<UITextComponent>(eff).fontSize = 10.0f;
                            hero.attackTimer = 1.5f;
                        }
                    }
                } else {
                    // 回復不要時はタンクの後方で陣形維持
                    if (registry.valid(tankEntity) && tankEntity != entity) {
                        targetX = tankX - 80.0f;
                        targetY = tankY - 80.0f;
                    } else {
                        shouldMoveToTarget = false; // タンクがいない場合はランダム移動
                    }
                }
            } else if (hero.role == HeroRole::Scout) {
                attackRange = 100.0f;
                attackDmg = 5.0f + (heroLevels[2] - 1) * 1.0f;
                
                if (currentAiMode == PartyAIMode::Attack && registry.valid(currentEnemy)) {
                    auto& eRect = registry.get<RectTransformComponent>(currentEnemy);
                    targetX = eRect.pos.x + eRect.size.x / 2.0f;
                    targetY = eRect.pos.y + eRect.size.y / 2.0f;
                    shouldAttackTarget = true;
                } else {
                    shouldMoveToTarget = false; // 自由に巡回調査
                }
            } else {
                // Tank, Sniper, Engineer
                if (registry.valid(currentEnemy)) {
                    auto& eRect = registry.get<RectTransformComponent>(currentEnemy);
                    float ex = eRect.pos.x + eRect.size.x / 2.0f;
                    float ey = eRect.pos.y + eRect.size.y / 2.0f;
                    shouldAttackTarget = (currentAiMode != PartyAIMode::Collect);
                    
                    if (hero.role == HeroRole::Tank) {
                        targetX = ex; targetY = ey;
                        attackRange = 80.0f; attackDmg = 10.0f + (heroLevels[0] - 1) * 2.0f;
                    } else if (hero.role == HeroRole::Sniper) {
                        float dx = tankX - ex; float dy = tankY - ey;
                        float ndist = std::sqrt(dx*dx + dy*dy);
                        if (ndist > 0.1f) { 
                            targetX = tankX + (dx/ndist)*150.0f; 
                            targetY = tankY + (dy/ndist)*150.0f; 
                        } else { 
                            targetX = tankX + 150.0f; targetY = tankY; 
                        }
                        attackRange = 400.0f; attackDmg = 25.0f + (heroLevels[1] - 1) * 5.0f;
                    } else if (hero.role == HeroRole::Engineer) {
                        targetX = ex + 50.0f; targetY = ey - 50.0f;
                        attackRange = 100.0f; attackDmg = 8.0f + (heroLevels[3] - 1) * 1.0f;
                    }
                } else {
                    // 敵がいない場合の陣形維持
                    if (hero.role == HeroRole::Tank) {
                        shouldMoveToTarget = false; // リーダーとしてランダム巡回
                    } else if (hero.role == HeroRole::Sniper) {
                        targetX = tankX - 100.0f; targetY = tankY + 50.0f;
                    } else if (hero.role == HeroRole::Engineer) {
                        targetX = tankX + 80.0f; targetY = tankY + 80.0f;
                    }
                }
            }

            float moveSpeed = ai.speed;
            if (isCoreFever) moveSpeed *= 2.0f;
            if (currentAiMode == PartyAIMode::Attack) moveSpeed *= 1.5f;
            if (currentAiMode == PartyAIMode::Collect) moveSpeed *= 2.0f;
            
            if (shouldMoveToTarget) {
                float dx = targetX - cx;
                float dy = targetY - cy;
                float targetDist = std::sqrt(dx*dx + dy*dy);
                
                // 陣形時の停止距離
                float stopDist = 10.0f;
                if (!registry.valid(currentEnemy)) stopDist = 40.0f; // 索敵時は少しルーズに
                
                if (targetDist > stopDist || ctx.isStowed) {
                    ai.moveDir.x = dx / targetDist; ai.moveDir.y = dy / targetDist;
                    rect.pos.x += ai.moveDir.x * moveSpeed * ctx.dt;
                    rect.pos.y += ai.moveDir.y * moveSpeed * ctx.dt;
                }
            } else {
                // ランダム巡回（Scout、非戦闘時のTankなど）
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
                rect.pos.x += ai.moveDir.x * moveSpeed * ctx.dt;
                rect.pos.y += ai.moveDir.y * moveSpeed * ctx.dt;
            }

            // --- 攻撃処理 ---
            if (shouldAttackTarget && registry.valid(currentEnemy)) {
                if (currentAiMode == PartyAIMode::Attack) attackDmg *= 1.5f;
                if (isCoreFever) attackDmg *= 2.0f;
                
                auto& eRect = registry.get<RectTransformComponent>(currentEnemy);
                float ex = eRect.pos.x + eRect.size.x / 2.0f;
                float ey = eRect.pos.y + eRect.size.y / 2.0f;
                float distToEnemy = std::sqrt((ex-cx)*(ex-cx) + (ey-cy)*(ey-cy));
                
                if (ctx.isStowed) attackRange = 40.0f;
                
                if (distToEnemy <= attackRange) {
                    hero.attackTimer -= ctx.dt;
                    if (hero.attackTimer <= 0.0f) {
                        auto& enemy = registry.get<DesktopEnemyComponent>(currentEnemy);
                        enemy.hp -= attackDmg;
                        totalDamage += attackDmg;
                        auto eff = CreateDamageText(registry, ex - 20.0f + (rand() % 40 - 20.0f), ey - 40.0f + (rand() % 40 - 20.0f), attackDmg);
                        registry.get<UITextComponent>(eff).color = {0.4f, 0.8f, 1.0f, 1.0f};
                        CreateDamageParticle(registry, ctx, cx, cy - 20.0f, "Resources/Textures/soft_circle.png", {0.4f, 0.8f, 1.0f, 0.6f}, ctx.isStowed ? 16.0f : 32.0f);
                        if (ctx.isStowed) registry.get<UITextComponent>(eff).fontSize = 10.0f;
                        
                        hero.attackTimer = (hero.role == HeroRole::Sniper) ? 2.5f : ((hero.role == HeroRole::Scout) ? 0.8f : 1.5f);
                    }
                }
            }
            
            // 画面外に出ないようにクランプ
            float screenW = ctx.viewportSize.x > 0 ? ctx.viewportSize.x : 1920.0f;
            float screenH = ctx.viewportSize.y > 0 ? ctx.viewportSize.y : 1080.0f;
            float maxY = screenH - 100.0f; 
            
            if (rect.pos.x < 0) rect.pos.x = 0;
            if (rect.pos.x > screenW - rect.size.x) rect.pos.x = screenW - rect.size.x;
            if (rect.pos.y < 0) rect.pos.y = 0;
            if (rect.pos.y > maxY - rect.size.y) rect.pos.y = maxY - rect.size.y;
        }
        
        // --- 3. ダメージテキスト・パーティクルの更新 ---
        auto dmgView = registry.view<DesktopDamageTextComponent, RectTransformComponent>();
        std::vector<entt::entity> deadTexts;
        for (auto entity : dmgView) {
            auto& dmg = dmgView.get<DesktopDamageTextComponent>(entity);
            auto& rect = dmgView.get<RectTransformComponent>(entity);
            
            dmg.lifetime -= ctx.dt;
            if (dmg.lifetime <= 0.0f) {
                deadTexts.push_back(entity);
            } else {
                rect.pos.x += dmg.velocity.x * ctx.dt;
                rect.pos.y += dmg.velocity.y * ctx.dt;
                // フェードアウト
                float alpha = dmg.lifetime / dmg.maxLifetime;
                if (registry.all_of<UITextComponent>(entity)) {
                    registry.get<UITextComponent>(entity).color.w = alpha;
                }
                if (registry.all_of<UIImageComponent>(entity)) {
                    registry.get<UIImageComponent>(entity).color.w = alpha;
                }
            }
        }
        for (auto e : deadTexts) {
            registry.destroy(e);
        }
        
        // --- 5. 集中オーラのUI更新 ---
        bool hasAura = !registry.view<FocusAuraTag>().empty();
        auto nameView = registry.view<NameComponent, UITextComponent>();
        for (auto e : nameView) {
            if (nameView.get<NameComponent>(e).name == "FocusMode") {
                auto& txt = nameView.get<UITextComponent>(e);
                if (hasAura) {
                    txt.text = "集中オーラ発動中：防御＆EXPボーナス！";
                    txt.color = {0.2f, 0.8f, 1.0f, 1.0f};
                } else {
                    txt.text = "集中オーラ未発動";
                    txt.color = {0.5f, 0.5f, 0.5f, 1.0f};
                }
            }
        }
        
        wasStowed = ctx.isStowed;
    }

    void Reset(entt::registry& registry) override {
        (void)registry;
    }
};

} // namespace Game
