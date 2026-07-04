#pragma once
#include "ISystem.h"
#include <random>
#include <string>
#include <cmath>
#include "SpriteCharacterAISystem.h"
#pragma warning(push)
#pragma warning(disable: 26495)
#include "../../Engine/ThirdParty/nlohmann/json.hpp"
#pragma warning(pop)
#include "../../Engine/PathUtils.h"
#include <fstream>
#include <chrono>

using json = nlohmann::json;

namespace Game {

struct DesktopEnemyComponent : public Component {
    float hp = 100.0f;
    float maxHp = 100.0f;
    entt::entity hpBarBg = entt::null;
    entt::entity hpBarFg = entt::null;
    float attackTimer = 1.0f;
    float attackDmg = 5.0f;
    float freezeTimer = 0.0f;
    DesktopEnemyComponent() { type = static_cast<ComponentType>(1002); }
};

struct DesktopCastleComponent : public Component {
    float hp = 10000.0f;
    float maxHp = 10000.0f;
    entt::entity hpBarBg = entt::null;
    entt::entity hpBarFg = entt::null;
    DesktopCastleComponent() { type = static_cast<ComponentType>(1008); }
};

enum class HeroRole {
    Tank,
    Sniper,
    Scout,
    Engineer,
    Healer
};

struct DesktopProjectileComponent : public Component {
    float targetX;
    float targetY;
    float startX;
    float startY;
    float progress;
    float arcHeight;
    entt::entity targetEntity;
    float speed;
    float damage;
    bool isHeal;
    DirectX::XMFLOAT4 color;
    int specialType; // 0:爆発, 1:氷, 2:分裂
    DesktopProjectileComponent() 
        : targetX(0.0f), targetY(0.0f), startX(0.0f), startY(0.0f), progress(0.0f), arcHeight(0.0f),
          targetEntity(entt::null), speed(500.0f), damage(0.0f), isHeal(false), color({1.0f, 1.0f, 1.0f, 1.0f}), specialType(-1) 
    { 
        type = static_cast<ComponentType>(1009); 
    }
};

struct DesktopHeroComponent : public Component {
    HeroRole role = HeroRole::Tank;
    float hp = 100.0f;
    float maxHp = 100.0f;
    entt::entity hpBarBg = entt::null;
    entt::entity hpBarFg = entt::null;
    entt::entity weaponEntity = entt::null;
    float attackTimer = 0.0f;
    float investigationTimer = 0.0f; // スカウトの調査タイマー
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
    int currentStage = 1;
    int enemiesSpawnedInStage = 0;
    int enemiesKilledInStage = 0;
    int enemiesPerStage = 10;
    
    double currentGold = 300.0;
    int totalKills = 0;
    double totalDamage = 0.0;
    double totalGoldEarned = 300.0;
    int totalItems = 0;
    int prestigeCount = 0;
    int prestigePoints = 0;
    int skillLevelAtk = 0;
    int skillLevelHp = 0;
    int skillLevelSpd = 0;
    int skillLevelGold = 0;
    
    static std::string FormatNum(double num) {
        if (num < 1000.0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.1f", num);
            return std::string(buf);
        }
        const char* suffixes[] = {"", "K", "M", "B", "T", "Qa", "Qi", "Sx", "Sp"};
        int suffixIndex = 0;
        while (num >= 1000.0 && suffixIndex < 8) {
            num /= 1000.0;
            suffixIndex++;
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "%.2f%s", num, suffixes[suffixIndex]);
        return std::string(buf);
    }
    
    // 0:Tank, 1:Sniper, 2:Scout, 3:Engineer
    int heroLevels[4] = {1, 1, 1, 1};
    
    // 0:Normal, 1:Cyber, 2:Antique, 3:Gold
    int currentTheme = 0;
    bool themeOwned[4] = {true, false, false, false};
    
    bool hasLoadedProgress = false;

    void SaveProgress(entt::registry& registry) {
        auto view = registry.view<DesktopPartyProgressComponent>();
        if (view.empty()) return;
        auto& prog = registry.get<DesktopPartyProgressComponent>(view.front());
        
        json j;
        j["dataFragments"] = prog.dataFragments;
        j["equippedIds"] = prog.equippedIds;
        j["ugcTexturePaths"] = {prog.ugcTexturePaths[0], prog.ugcTexturePaths[1], prog.ugcTexturePaths[2], prog.ugcTexturePaths[3], prog.ugcTexturePaths[4]};
        j["hiredRoles"] = prog.hiredRoles;
        j["maxDeploymentLimit"] = prog.maxDeploymentLimit;
        j["lastSavedTime"] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        
        j["chestEnergyTarget"] = prog.chestEnergyTarget;
        j["availableChests"] = prog.availableChests;
        j["specialArrows"] = {prog.specialArrows[0], prog.specialArrows[1], prog.specialArrows[2]};
        
        j["currentGold"] = currentGold;
        j["totalKills"] = totalKills;
        j["totalDamage"] = totalDamage;
        j["totalGoldEarned"] = totalGoldEarned;
        j["totalItems"] = totalItems;
        j["prestigeCount"] = prestigeCount;
        j["prestigePoints"] = prestigePoints;
        j["skillLevelAtk"] = skillLevelAtk;
        j["skillLevelHp"] = skillLevelHp;
        j["skillLevelSpd"] = skillLevelSpd;
        j["skillLevelGold"] = skillLevelGold;
        j["currentStage"] = currentStage;
        j["heroLevels"] = {heroLevels[0], heroLevels[1], heroLevels[2], heroLevels[3]};
        j["currentTheme"] = currentTheme;
        j["themeOwned"] = {themeOwned[0], themeOwned[1], themeOwned[2], themeOwned[3]};
        
        json inv = json::array();
        for (const auto& eq : prog.inventory) {
            json item;
            item["id"] = eq.id;
            item["name"] = eq.name; 
            item["rarity"] = (int)eq.rarity;
            item["type"] = (int)eq.type;
            item["atkMul"] = eq.atkMul;
            item["spdMul"] = eq.spdMul;
            item["hpMul"] = eq.hpMul;
            item["flavorText"] = eq.flavorText;
            inv.push_back(item);
        }
        j["inventory"] = inv;
        
        json relics = json::array();
        for (const auto& r : prog.relicEncyclopedia) {
            json item;
            item["id"] = r.id;
            item["name"] = r.name;
            item["description"] = r.description;
            item["isUnlocked"] = r.isUnlocked;
            item["globalAtkBuff"] = r.globalAtkBuff;
            item["globalHpBuff"] = r.globalHpBuff;
            item["globalSpdBuff"] = r.globalSpdBuff;
            relics.push_back(item);
        }
        j["relicEncyclopedia"] = relics;
        
        std::string savePath = Engine::PathUtils::GetUnifiedPath("party_progress.json");
        std::ofstream ofs(savePath);
        if (ofs.is_open()) {
            ofs << j.dump(4);
        }
    }

    void LoadProgress(entt::registry& registry) {
        auto view = registry.view<DesktopPartyProgressComponent>();
        entt::entity e = entt::null;
        if (view.empty()) {
            e = registry.create();
            registry.emplace<DesktopPartyProgressComponent>(e);
        } else {
            e = view.front();
        }
        auto& prog = registry.get<DesktopPartyProgressComponent>(e);
        
        std::string loadPath = Engine::PathUtils::GetUnifiedPath("party_progress.json");
        std::ifstream ifs(loadPath);
        if (ifs.is_open()) {
            json j;
            try {
                ifs >> j;
                prog.dataFragments = j.value("dataFragments", 0);
                prog.equippedIds = j.value("equippedIds", std::vector<int>(25, -1));
                if (prog.equippedIds.size() < 25) {
                    std::vector<int> newIds(25, -1);
                    for (size_t i = 0; i < prog.equippedIds.size() && i < 5; ++i) {
                        int oldId = prog.equippedIds[i];
                        if (oldId != -1) {
                            int type = 0;
                            for (const auto& eq : prog.inventory) {
                                if (eq.id == static_cast<uint32_t>(oldId)) { type = (int)eq.type; break; }
                            }
                            newIds[i * 5 + type] = oldId;
                        }
                    }
                    prog.equippedIds = newIds;
                }
                if (j.contains("ugcTexturePaths") && j["ugcTexturePaths"].is_array() && j["ugcTexturePaths"].size() >= 5) {
                    for (int i = 0; i < 5; ++i) {
                        prog.ugcTexturePaths[i] = j["ugcTexturePaths"][i].get<std::string>();
                    }
                }
                if (j.contains("hiredRoles") && j["hiredRoles"].is_array()) {
                    prog.hiredRoles.clear();
                    for (auto& r : j["hiredRoles"]) prog.hiredRoles.push_back(r.get<int>());
                } else {
                    prog.hiredRoles = {0, 1, 2, 4}; // Default if missing
                }
                prog.maxDeploymentLimit = j.value("maxDeploymentLimit", 4);
                prog.lastSavedTime = j.value("lastSavedTime", 0LL);
                
                prog.chestEnergyTarget = j.value("chestEnergyTarget", 50);
                prog.availableChests = j.value("availableChests", 0);
                if (j.contains("specialArrows") && j["specialArrows"].is_array() && j["specialArrows"].size() >= 3) {
                    for (int i=0; i<3; ++i) prog.specialArrows[i] = j["specialArrows"][i].get<int>();
                } else {
                    prog.specialArrows = {0, 0, 0};
                }
                
                currentGold = j.value("currentGold", 0.0) + 1.0e15; // DEBUG
                totalKills = j.value("totalKills", 0);
                totalDamage = j.value("totalDamage", 0.0);
                totalGoldEarned = j.value("totalGoldEarned", 0.0) + 1.0e15; // DEBUG
                totalItems = j.value("totalItems", 0);
                prestigeCount = j.value("prestigeCount", 0);
                prestigePoints = j.value("prestigePoints", 0);
                skillLevelAtk = j.value("skillLevelAtk", 0);
                skillLevelHp = j.value("skillLevelHp", 0);
                skillLevelSpd = j.value("skillLevelSpd", 0);
                skillLevelGold = j.value("skillLevelGold", 0);
                currentStage = j.value("currentStage", 1);
                
                if (j.contains("heroLevels") && j["heroLevels"].is_array() && j["heroLevels"].size() >= 4) {
                    for (int i = 0; i < 4; ++i) {
                        heroLevels[i] = j["heroLevels"][i].get<int>();
                    }
                }
                
                currentTheme = j.value("currentTheme", 0);
                if (j.contains("themeOwned") && j["themeOwned"].is_array() && j["themeOwned"].size() >= 4) {
                    for (int i = 0; i < 4; ++i) {
                        themeOwned[i] = j["themeOwned"][i].get<bool>();
                    }
                }
                
                if (j.contains("inventory")) {
                    for (const auto& item : j["inventory"]) {
                        Equipment eq;
                        eq.id = item.value("id", 0);
                        eq.name = item.value("name", "Unknown Part");
                        eq.rarity = (EquipRarity)item.value("rarity", 0);
                        eq.type = (EquipType)item.value("type", 0);
                        eq.atkMul = item.value("atkMul", 0.0f);
                        eq.spdMul = item.value("spdMul", 0.0f);
                        eq.hpMul = item.value("hpMul", 0.0f);
                        eq.flavorText = item.value("flavorText", "");
                        prog.inventory.push_back(eq);
                    }
                }
                
                if (j.contains("relicEncyclopedia")) {
                    for (const auto& item : j["relicEncyclopedia"]) {
                        InternetRelic r;
                        r.id = item.value("id", 0);
                        r.name = item.value("name", "Unknown");
                        r.description = item.value("description", "");
                        r.isUnlocked = item.value("isUnlocked", false);
                        r.globalAtkBuff = item.value("globalAtkBuff", 0.0f);
                        r.globalHpBuff = item.value("globalHpBuff", 0.0f);
                        r.globalSpdBuff = item.value("globalSpdBuff", 0.0f);
                        prog.relicEncyclopedia.push_back(r);
                    }
                }
            } catch(...) {}
        }
    }
    
    void TriggerCoreFever() {
        isCoreFever = true;
        coreFeverTimer = 5.0f;
    }

    entt::entity CreateDamageText(entt::registry& reg, float x, float y, double damage, bool isStowed = false) {
        auto ent = reg.create();
        auto& t = reg.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f};
        t.pivot = {0.0f, 0.0f};
        t.pos = {x, y};
        t.size = {100, 30};
        
        auto& txt = reg.emplace<UITextComponent>(ent);
        txt.text = (damage > 0.0) ? FormatNum(damage) : "";
        txt.fontSize = isStowed ? 10.0f : 24.0f;
        txt.color = isStowed ? DirectX::XMFLOAT4{1.0f, 0.2f, 0.2f, 0.6f} : DirectX::XMFLOAT4{1.0f, 0.2f, 0.2f, 1.0f}; // 赤
        txt.fontPath = "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf";
        
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

    entt::entity CreateHealText(entt::registry& reg, float x, float y, double amount, bool isStowed = false) {
        auto ent = reg.create();
        auto& t = reg.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f};
        t.pivot = {0.0f, 0.0f};
        t.pos = {x, y};
        t.size = {100, 30};
        
        auto& txt = reg.emplace<UITextComponent>(ent);
        txt.text = "+" + FormatNum(amount);
        txt.fontSize = isStowed ? 10.0f : 24.0f;
        txt.color = isStowed ? DirectX::XMFLOAT4{0.2f, 1.0f, 0.2f, 0.6f} : DirectX::XMFLOAT4{0.2f, 1.0f, 0.2f, 1.0f}; // 緑
        
        reg.emplace<DesktopDamageTextComponent>(ent);
        return ent;
    }

public:
    double GetLevelUpCost(int roleIndex) {
        if (roleIndex < 0 || roleIndex >= 4) return 0.0;
        int lv = heroLevels[roleIndex];
        return 100.0 * std::pow(1.5, lv - 1);
    }

    void GetHeroStats(entt::registry& registry, int roleIndex, float& outHp, float& outMaxHp, float& outAtk, float& outSpd, float* outBaseMaxHp = nullptr, float* outBaseAtk = nullptr, float* outBaseSpd = nullptr) {
        int lv = 1;
        if (roleIndex >= 0 && roleIndex < 4) lv = heroLevels[roleIndex];
        
        float baseMaxHp = 100.0f, baseAtk = 10.0f, baseSpd = 1.0f;
        if (roleIndex == 0) { baseMaxHp = 100.0f * static_cast<float>(std::pow(1.2, lv-1)); baseAtk = 10.0f * static_cast<float>(std::pow(1.25, lv-1)); baseSpd = 2.0f; }
        else if (roleIndex == 1) { baseMaxHp = 60.0f * static_cast<float>(std::pow(1.15, lv-1)); baseAtk = 25.0f * static_cast<float>(std::pow(1.3, lv-1)); baseSpd = 2.2f; }
        else if (roleIndex == 2) { baseMaxHp = 80.0f * static_cast<float>(std::pow(1.2, lv-1)); baseAtk = 5.0f * static_cast<float>(std::pow(1.2, lv-1)); baseSpd = 4.0f; }
        else if (roleIndex == 3) { baseMaxHp = 80.0f * static_cast<float>(std::pow(1.2, lv-1)); baseAtk = 8.0f * static_cast<float>(std::pow(1.2, lv-1)); baseSpd = 1.5f; }
        else if (roleIndex == 4) { baseMaxHp = 90.0f; baseAtk = 5.0f; baseSpd = 2.5f; }

        // 1. マイルストーンボーナス
        float milestoneMul = 1.0f;
        if (lv >= 10) milestoneMul *= 2.0f;
        if (lv >= 25) milestoneMul *= 3.0f;
        if (lv >= 50) milestoneMul *= 5.0f;
        if (lv >= 100) milestoneMul *= 10.0f;
        
        baseMaxHp *= milestoneMul;
        baseAtk *= milestoneMul;

        // 2. プレステージボーナス (スキルツリー)
        float skillHpBonus = skillLevelHp * 0.5f; // 1Lvにつき +50%
        float skillAtkBonus = skillLevelAtk * 0.5f;
        float skillSpdBonus = skillLevelSpd * 0.1f; // 1Lvにつき +10%
        
        baseMaxHp *= (1.0f + skillHpBonus);
        baseAtk *= (1.0f + skillAtkBonus);
        baseSpd *= (1.0f + skillSpdBonus);

        // 3. シナジーボーナス
        if (roleIndex == 1) { // スナイパー (タンクHPシナジー)
            int tankLv = heroLevels[0];
            float tankHp = 100.0f * static_cast<float>(std::pow(1.2, tankLv-1));
            if (tankLv >= 10) tankHp *= 2.0f;
            if (tankLv >= 25) tankHp *= 3.0f;
            if (tankLv >= 50) tankHp *= 5.0f;
            if (tankLv >= 100) tankHp *= 10.0f;
            tankHp *= (1.0f + skillHpBonus);
            baseAtk += tankHp * 0.01f;
        }
        if (roleIndex == 2) { // スカウト (累積ゴールドシナジー)
            float goldBonus = (float)std::log10(totalGoldEarned + 1.0) * 0.2f;
            baseSpd += goldBonus;
        }
        if (roleIndex == 3) { // エンジニア (全体レベルシナジー)
            int totalLv = heroLevels[0] + heroLevels[1] + heroLevels[2] + heroLevels[3];
            baseAtk *= (1.0f + (totalLv * 0.05f));
        }

        float globalAtkBuff = 0.0f, globalHpBuff = 0.0f, globalSpdBuff = 0.0f;
        float eqAtkBuff = 0.0f, eqHpBuff = 0.0f, eqSpdBuff = 0.0f;
        
        auto viewProg = registry.view<DesktopPartyProgressComponent>();
        if (!viewProg.empty()) {
            auto& prog = registry.get<DesktopPartyProgressComponent>(viewProg.front());
            for (const auto& relic : prog.relicEncyclopedia) {
                if (relic.isUnlocked) {
                    globalAtkBuff += relic.globalAtkBuff;
                    globalHpBuff += relic.globalHpBuff;
                    globalSpdBuff += relic.globalSpdBuff;
                }
            }
            if (roleIndex >= 0 && roleIndex < 5) {
                for (int slot = 0; slot < 5; ++slot) {
                    int eqId = prog.equippedIds[roleIndex * 5 + slot];
                    if (eqId != -1) {
                        for (const auto& eq : prog.inventory) {
                            if (eq.id == static_cast<uint32_t>(eqId)) {
                                eqAtkBuff += eq.atkMul;
                                eqHpBuff += eq.hpMul;
                                eqSpdBuff += eq.spdMul;
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        if (outBaseMaxHp) *outBaseMaxHp = baseMaxHp;
        if (outBaseAtk) *outBaseAtk = baseAtk;
        if (outBaseSpd) *outBaseSpd = baseSpd;

        outMaxHp = baseMaxHp * (1.0f + globalHpBuff + eqHpBuff);
        outAtk = baseAtk * (1.0f + globalAtkBuff + eqAtkBuff);
        outSpd = baseSpd * (1.0f + globalSpdBuff + eqSpdBuff);
        
        outHp = outMaxHp;
        auto heroView = registry.view<DesktopHeroComponent>();
        for (auto h : heroView) {
            auto& hero = registry.get<DesktopHeroComponent>(h);
            if ((int)hero.role == roleIndex) {
                outHp = hero.hp;
                break;
            }
        }
    }

    void Update(entt::registry& registry, GameContext& ctx) override {
        if (!hasLoadedProgress) {
            LoadProgress(registry);
            hasLoadedProgress = true;
        }
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

        // --- 全体バフの集計 ---
        float globalAtkBuff = 0.0f;
        float globalHpBuff = 0.0f;
        float globalSpdBuff = 0.0f;
        auto viewProg = registry.view<DesktopPartyProgressComponent>();
        if (!viewProg.empty()) {
            auto& prog = registry.get<DesktopPartyProgressComponent>(viewProg.front());
            for (const auto& relic : prog.relicEncyclopedia) {
                if (relic.isUnlocked) {
                    globalAtkBuff += relic.globalAtkBuff;
                    globalHpBuff += relic.globalHpBuff;
                    globalSpdBuff += relic.globalSpdBuff;
                }
            }
        }

        // --- 拠点（城）の管理 ---
        float screenW = ctx.viewportSize.x > 0 ? ctx.viewportSize.x : 1920.0f;
        float screenH = ctx.viewportSize.y > 0 ? ctx.viewportSize.y : 1080.0f;
        
        auto castleView = registry.view<DesktopCastleComponent>();
        entt::entity castleEnt = entt::null;
        float baseCastleX = 50.0f;
        float baseCastleY = screenH / 2.0f;
        
        if (castleView.empty()) {
            castleEnt = registry.create();
            registry.emplace<NameComponent>(castleEnt, "Castle");
            auto& rect = registry.emplace<RectTransformComponent>(castleEnt);
            rect.anchor = {0.0f, 0.0f}; rect.pivot = {0.0f, 0.0f};
            rect.pos = {baseCastleX, baseCastleY - 100.0f};
            rect.size = {150.0f, 200.0f};
            auto& img = registry.emplace<UIImageComponent>(castleEnt);
            img.texturePath = "Resources/Textures/white1x1.png"; // TODO: 拠点画像
            if (ctx.renderer) img.textureHandle = ctx.renderer->LoadTexture2D(img.texturePath, false);
            img.color = {0.8f, 0.8f, 0.8f, 1.0f};
            img.layer = -5;
            
            auto& castle = registry.emplace<DesktopCastleComponent>(castleEnt);
            castle.hpBarBg = registry.create();
            auto& bgRect = registry.emplace<RectTransformComponent>(castle.hpBarBg);
            bgRect.anchor = {0.0f, 0.0f}; bgRect.pivot = {0.0f, 0.0f};
            bgRect.size = {150, 15};
            auto& bgImg = registry.emplace<UIImageComponent>(castle.hpBarBg);
            bgImg.texturePath = "Resources/Textures/white1x1.png";
            bgImg.color = {0.2f, 0.2f, 0.2f, 1.0f};
            bgImg.layer = -4;
            if (ctx.renderer) bgImg.textureHandle = ctx.renderer->LoadTexture2D(bgImg.texturePath);
            
            castle.hpBarFg = registry.create();
            auto& fgRect = registry.emplace<RectTransformComponent>(castle.hpBarFg);
            fgRect.anchor = {0.0f, 0.0f}; fgRect.pivot = {0.0f, 0.0f};
            fgRect.size = {150, 15};
            auto& fgImg = registry.emplace<UIImageComponent>(castle.hpBarFg);
            fgImg.texturePath = "Resources/Textures/white1x1.png";
            fgImg.color = {0.2f, 0.9f, 0.2f, 1.0f};
            fgImg.layer = -3;
            if (ctx.renderer) fgImg.textureHandle = ctx.renderer->LoadTexture2D(fgImg.texturePath);
        } else {
            castleEnt = castleView.front();
        }

        // --- ★オービタル（格納）モードにおける強制サイズ変更と敵の追従 ---
        float targetScale = ctx.isStowed ? 5.0f : 32.0f; // キャラクターサイズを大幅に縮小
        auto sizeView = registry.view<RectTransformComponent>();
        
        POINT pt; GetCursorPos(&pt);
        float mouseX = static_cast<float>(pt.x);
        float mouseY = static_cast<float>(pt.y);
        
        // 城の座標更新
        auto& castleRef = registry.get<DesktopCastleComponent>(castleEnt);
        auto& castleRect = registry.get<RectTransformComponent>(castleEnt);
        if (ctx.isStowed) {
            castleRect.pos = {mouseX - 10.0f, mouseY - 10.0f};
            castleRect.size = {20.0f, 20.0f};
            registry.get<UIImageComponent>(castleEnt).color = {0.8f, 0.8f, 0.8f, 0.5f};
        } else {
            castleRect.pos = {50.0f, 0.0f}; // 左から少し離した位置
            castleRect.size = {30.0f, screenH}; // 壁自体は薄く
            registry.get<UIImageComponent>(castleEnt).color = {0.6f, 0.8f, 1.0f, 1.0f};
        }
        
        float castleCenterX = castleRect.pos.x + castleRect.size.x / 2.0f;
        float castleCenterY = castleRect.pos.y + castleRect.size.y / 2.0f;
        
        // 城HPバー更新
        if (registry.valid(castleRef.hpBarBg)) {
            auto& bgRect = registry.get<RectTransformComponent>(castleRef.hpBarBg);
            bgRect.pos = {castleRect.pos.x, castleRect.pos.y - 20.0f};
            bgRect.enabled = !ctx.isStowed;
        }
        if (registry.valid(castleRef.hpBarFg)) {
            auto& fgRect = registry.get<RectTransformComponent>(castleRef.hpBarFg);
            fgRect.pos = {castleRect.pos.x, castleRect.pos.y - 20.0f};
            fgRect.size.x = (ctx.isStowed ? 20.0f : 150.0f) * (castleRef.hp / castleRef.maxHp);
            fgRect.enabled = !ctx.isStowed;
        }

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
                
                bool eIsEnemy = registry.all_of<DesktopEnemyComponent>(e);
                bool oIsEnemy = registry.all_of<DesktopEnemyComponent>(other);
                
                // 自分が敵で、相手が味方の場合は弾かれない（動かない）
                if (eIsEnemy && !oIsEnemy) continue;
                
                auto& oRect = sizeView.get<RectTransformComponent>(other);
                float ox = oRect.pos.x + oRect.size.x / 2.0f;
                float oy = oRect.pos.y + oRect.size.y / 2.0f;
                float dx = cx - ox;
                float dy = cy - oy;
                float dist = std::sqrt(dx*dx + dy*dy);
                
                // 敵と味方の間は少し広めの距離（密着しない）を保ち、同盟同士は少しの距離を保つ
                float separateDist = 15.0f;
                if (eIsEnemy && oIsEnemy) {
                    separateDist = ctx.isStowed ? 20.0f : 40.0f; // 敵同士は密集しないように広め
                } else if (eIsEnemy != oIsEnemy) {
                    separateDist = ctx.isStowed ? 30.0f : 40.0f; // 敵と味方
                } else {
                    separateDist = ctx.isStowed ? 20.0f : 15.0f; // 味方同士は隊列の重なりを許容
                }
                
                if (dist < separateDist) {
                    if (dist < 0.001f) {
                        dx = static_cast<float>((rand() % 100) - 50);
                        dy = static_cast<float>((rand() % 100) - 50);
                        dist = std::sqrt(dx*dx + dy*dy) + 0.001f;
                    }
                    float pushForce = (separateDist - dist) * 10.0f; // 弾き力
                    rect.pos.x += (dx / dist) * pushForce * ctx.dt;
                    rect.pos.y += (dy / dist) * pushForce * ctx.dt;
                }
            }
        }

        // --- 0.5 タンクの位置を取得（敵の引き寄せ用） ---
        float tankX = 0.0f, tankY = 0.0f;
        entt::entity tankEntity = entt::null;
        auto charViewForTank = registry.view<SpriteCharacterAIComponent, RectTransformComponent, DesktopHeroComponent>();
        for (auto entity : charViewForTank) {
            auto& hero = charViewForTank.get<DesktopHeroComponent>(entity);
            if (hero.role == HeroRole::Tank && hero.hp > 0.0f) {
                auto& rect = charViewForTank.get<RectTransformComponent>(entity);
                tankX = rect.pos.x + rect.size.x / 2.0f;
                tankY = rect.pos.y + rect.size.y / 2.0f;
                tankEntity = entity;
                break; // 最初に見つけた生きているタンクをターゲットにする
            }
        }

        // --- 1. 敵の出現管理 ---
        auto enemyView = registry.view<DesktopEnemyComponent, RectTransformComponent>();
        int activeEnemyCount = 0;
        for (auto it = enemyView.begin(); it != enemyView.end(); ++it) activeEnemyCount++;

        int stageMultiplier = currentStage;
        int maxEnemies = ctx.isStowed ? (50 + stageMultiplier * 5) : (3 + stageMultiplier);
        float spawnDelay = ctx.isStowed ? 3.0f : (std::max)(0.2f, 1.5f - (stageMultiplier * 0.05f));

        spawnTimer -= ctx.dt;
        if (activeEnemyCount < maxEnemies && spawnTimer <= 0.0f && enemiesSpawnedInStage < enemiesPerStage) {
            spawnTimer = spawnDelay;
            
            int spawnCount = ctx.isStowed ? 15 : (1 + (stageMultiplier / 5)); // 5ステージごとに一度に湧く数が増える
            
            float baseSpawnX, baseSpawnY;
            if (ctx.isStowed) {
                baseSpawnX = screenW + 300.0f; // 常に画面の右枠外から湧くようにする
                baseSpawnY = mouseY + static_cast<float>(rand() % 800 - 400);
            } else {
                float maxSpawnY = (std::max)(100.0f, screenH - 200.0f);
                baseSpawnX = 1300.0f; // 司令室(X=1320)のすぐ左隣から出現
                baseSpawnY = 100.0f + (rand() % (int)maxSpawnY);
            }

            for (int i = 0; i < spawnCount; i++) {
                if (enemiesSpawnedInStage >= enemiesPerStage) break;
                if (activeEnemyCount + i >= maxEnemies) break;
                
                enemiesSpawnedInStage++;
                // 敵を生成
                entt::entity newEnemy = registry.create();
                registry.emplace<NameComponent>(newEnemy, "Enemy_Virus");
                
                auto& rect = registry.emplace<RectTransformComponent>(newEnemy);
                rect.enabled = true; // 常に表示
                rect.anchor = {0.0f, 0.0f};
                rect.pivot = {0.0f, 0.0f};
                
                float spawnX = baseSpawnX;
                float spawnY = baseSpawnY;
                if (ctx.isStowed && spawnCount > 1) {
                    spawnX += static_cast<float>(rand() % 400 - 200); // 集団で散らばる
                    spawnY += static_cast<float>(rand() % 400 - 200);
                }
                rect.pos = {spawnX, spawnY};
                rect.size = {targetScale, targetScale};
            
            auto& img = registry.emplace<UIImageComponent>(newEnemy);
            img.texturePath = "Resources/Textures/white1x1.png"; // 敵を四角テクスチャに
            if (ctx.renderer) {
                img.textureHandle = ctx.renderer->LoadTexture2D(img.texturePath, false);
            }
            img.color = {0.8f, 0.2f, 0.2f, 1.0f}; // 赤色
            img.layer = -4; // キャラより少し手前に変更 (見えやすくするため)
            
            float enemyScale = static_cast<float>(std::pow(1.3, currentStage - 1));
            enemyScale *= (1.0f + prestigeCount * 0.2f); // 敵はプレステージ回数に応じて少しだけ強くなる

            auto& enemy = registry.emplace<DesktopEnemyComponent>(newEnemy);
            enemy.maxHp = 80.0f * enemyScale;
            enemy.hp = enemy.maxHp;
            enemy.attackDmg = (5.0f + (rand() % 5)) * enemyScale;
            
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
            
            } // for end
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
                float enemyScale = static_cast<float>(std::pow(1.3, currentStage - 1));
                enemyScale *= (1.0f + prestigeCount * 0.2f);
                
                double goldGained = 50.0 * enemyScale;
                goldGained *= (1.0 + skillLevelGold * 0.5); // スキルボーナス
                auto auraHeroes = registry.view<FocusAuraTag>();
                if (!auraHeroes.empty()) goldGained *= 1.5; // オーラ時は獲得ゴールドもアップ
                
                currentGold += goldGained;
                totalGoldEarned += goldGained;
                totalKills += 1;
                enemiesKilledInStage += 1;
                
                auto eff = CreateDamageText(registry, enemyRect.pos.x, enemyRect.pos.y, 0);
                registry.get<UITextComponent>(eff).text = "GOLD +" + FormatNum(goldGained);
                registry.get<UITextComponent>(eff).color = {0.8f, 0.6f, 0.1f, 1.0f};
                if (ctx.isStowed) registry.get<UITextComponent>(eff).fontSize = 12.0f;

                if (registry.valid(enemy.hpBarBg)) registry.destroy(enemy.hpBarBg);
                if (registry.valid(enemy.hpBarFg)) registry.destroy(enemy.hpBarFg);
                enemiesToDestroy.push_back(currentEnemy);

                // --- ハクスラドロップ判定 ---
                if (rand() % 100 < 30) { // 30%の確率で装備品ドロップ
                    auto viewProgDrop = registry.view<DesktopPartyProgressComponent>();
                    if (!viewProgDrop.empty()) {
                        auto& prog = registry.get<DesktopPartyProgressComponent>(viewProgDrop.front());
                        Equipment eq;
                        eq.id = (uint32_t)prog.inventory.size() + 1;
                        
                        int r = rand() % 100;
                        if (r < 5) eq.rarity = EquipRarity::Artifact;
                        else if (r < 20) eq.rarity = EquipRarity::Epic;
                        else if (r < 50) eq.rarity = EquipRarity::Rare;
                        else eq.rarity = EquipRarity::Common;
                        
                        int itemLevel = rand() % 10 + 1 + (currentStage / 5);
                        eq.type = (EquipType)(rand() % 5);
                        std::wstring names[] = {L"USBメモリ", L"拡張基板", L"コンデンサ", L"冷却ファン", L"LANケーブル"};
                        eq.name = Engine::PathUtils::ToUTF8(names[(int)eq.type] + L" (Lv" + std::to_wstring(itemLevel) + L")");
                        
                        float mulBase = (0.05f + (itemLevel * 0.02f)) * static_cast<float>(std::pow(1.1, currentStage / 5));
                        if (eq.rarity == EquipRarity::Rare) mulBase *= 1.5f;
                        if (eq.rarity == EquipRarity::Epic) mulBase *= 2.0f;
                        if (eq.rarity == EquipRarity::Artifact) mulBase *= 3.0f;
                        
                        // ランダム幅は80%〜120%
                        auto getRandFactor = []() { return 0.8f + (rand() % 41) / 100.0f; };
                        eq.atkMul = mulBase * getRandFactor();
                        eq.spdMul = mulBase * getRandFactor();
                        eq.hpMul = mulBase * getRandFactor();
                        
                        // ウィルス混入（デメリット付き強力版）の可能性
                        if (eq.rarity == EquipRarity::Artifact || eq.rarity == EquipRarity::Epic) {
                            if (rand() % 100 < 30) {
                                eq.hpMul = -0.1f;
                                eq.atkMul += 0.2f;
                                eq.name = Engine::PathUtils::ToUTF8(L"[感染] ") + eq.name;
                                eq.flavorText = Engine::PathUtils::ToUTF8(L"強力だがウイルスが混入している。");
                            }
                        }
                        
                        prog.inventory.push_back(eq);
                        SaveProgress(registry); // 取得時にオートセーブ
                        
                        // UI通知
                        auto dropEff = CreateDamageText(registry, enemyRect.pos.x, enemyRect.pos.y - 30.0f, 0, ctx.isStowed);
                        registry.get<UITextComponent>(dropEff).text = "DROP: " + eq.name;
                        registry.get<UITextComponent>(dropEff).color = eq.GetRarityColor();
                    }
                } else if (rand() % 100 < 2) { // 装備が落ちなかった場合の2%でデータ断片ドロップ
                    auto viewProgDrop = registry.view<DesktopPartyProgressComponent>();
                    if (!viewProgDrop.empty()) {
                        auto& prog = registry.get<DesktopPartyProgressComponent>(viewProgDrop.front());
                        prog.dataFragments++;
                        SaveProgress(registry);
                        auto dropEff = CreateDamageText(registry, enemyRect.pos.x, enemyRect.pos.y - 30.0f, 0, ctx.isStowed);
                        registry.get<UITextComponent>(dropEff).text = "DROP: 謎のデータ断片";
                        registry.get<UITextComponent>(dropEff).color = {0.2f, 0.8f, 0.2f, 1.0f};
                    }
                }
            } else {
                // 敵の移動ロジック
                float ex = enemyRect.pos.x + enemyRect.size.x / 2.0f;
                float ey = enemyRect.pos.y + enemyRect.size.y / 2.0f;
                float dx = castleCenterX - ex;
                // 展開時は城（壁）が画面全体を覆うため、上下に集まらずにまっすぐ左へ進む
                float dy = ctx.isStowed ? (castleCenterY - ey) : 0.0f;
                
                // タンクが存在すれば、タンクのY座標に引き寄せる
                if (!ctx.isStowed && registry.valid(tankEntity)) {
                    dy = tankY - ey;
                }
                
                // ヒーロー（味方）との衝突・攻撃チェック
                bool isBlockedByHero = false;
                entt::entity blockedHeroEnt = entt::null;
                
                if (!ctx.isStowed) {
                    auto heroView = registry.view<DesktopHeroComponent, RectTransformComponent>();
                    for (auto he : heroView) {
                        auto& heroRect = heroView.get<RectTransformComponent>(he);
                        auto& heroData = heroView.get<DesktopHeroComponent>(he);
                        
                        if (heroData.hp > 0.0f) {
                            float heroRightEdge = heroRect.pos.x + heroRect.size.x;
                            // 横方向の重なりチェック
                            if (enemyRect.pos.x <= heroRightEdge && enemyRect.pos.x + enemyRect.size.x >= heroRect.pos.x) {
                                // 縦方向の重なりチェック（隙間をすり抜けないよう上下に少し判定を広げる）
                                float expandY = 15.0f;
                                if (enemyRect.pos.y <= heroRect.pos.y + heroRect.size.y + expandY && 
                                    enemyRect.pos.y + enemyRect.size.y >= heroRect.pos.y - expandY) {
                                    isBlockedByHero = true;
                                    blockedHeroEnt = he;
                                    enemyRect.pos.x = heroRightEdge; // キャラクターの右側に食い止められる
                                    break;
                                }
                            }
                        }
                    }
                }
                
                float dist = std::sqrt(dx*dx + dy*dy);
                if (!isBlockedByHero && dist > 50.0f) {
                    float speed = ctx.isStowed ? 50.0f : 80.0f; // 移動速度
                    
                    if (enemy.freezeTimer > 0.0f) {
                        speed *= 0.3f; // 速度ダウン
                        enemy.freezeTimer -= ctx.dt;
                        if (registry.all_of<UIImageComponent>(currentEnemy)) registry.get<UIImageComponent>(currentEnemy).color = {0.4f, 0.8f, 1.0f, 1.0f}; // 凍結色
                    } else {
                        if (registry.all_of<UIImageComponent>(currentEnemy)) registry.get<UIImageComponent>(currentEnemy).color = {0.8f, 0.2f, 0.2f, 1.0f}; // 通常色
                    }
                    
                    enemyRect.pos.x += (dx / dist) * speed * ctx.dt;
                    enemyRect.pos.y += (dy / dist) * speed * ctx.dt;
                } else {
                    // 城 または キャラクターへの攻撃
                    enemy.attackTimer -= ctx.dt;
                    if (enemy.attackTimer <= 0.0f) {
                        enemy.attackTimer = 1.0f; // 1秒に1回攻撃
                        
                        if (isBlockedByHero) {
                            auto& targetHero = registry.get<DesktopHeroComponent>(blockedHeroEnt);
                            targetHero.hp -= enemy.attackDmg;
                        } else {
                            castleRef.hp -= enemy.attackDmg;
                        }
                        
                        // ダメージ表記は攻撃を行っている敵の位置（少し左）に出す
                        auto eff = CreateDamageText(registry, enemyRect.pos.x - 30.0f, enemyRect.pos.y, enemy.attackDmg, ctx.isStowed);
                        registry.get<UITextComponent>(eff).color = ctx.isStowed ? DirectX::XMFLOAT4{1.0f, 0.2f, 0.2f, 0.6f} : DirectX::XMFLOAT4{1.0f, 0.2f, 0.2f, 1.0f};
                    }
                }
            }
        }
        for (auto e : enemiesToDestroy) {
            registry.destroy(e);
        }
        
        // ステージクリア判定
        if (enemiesKilledInStage >= enemiesPerStage) {
            currentStage++;
            enemiesKilledInStage = 0;
            enemiesSpawnedInStage = 0;
            spawnTimer = 5.0f;
            
            // screenWとscreenHは既に定義されているため削除
            auto eff = CreateDamageText(registry, screenW/2.0f - 100.0f, screenH/2.0f, 0);
            registry.get<UITextComponent>(eff).text = "STAGE " + std::to_string(currentStage) + " START!";
            registry.get<UITextComponent>(eff).color = {1.0f, 0.8f, 0.2f, 1.0f};
            registry.get<UITextComponent>(eff).fontSize = 32.0f;
        }

        // --- 1.5. 味方の雇用枠と同期・初期化 ---
        auto heroView = registry.view<DesktopHeroComponent>();
        int currentHeroCount = 0;
        for (auto it = heroView.begin(); it != heroView.end(); ++it) currentHeroCount++;
        
        auto viewProgHired = registry.view<DesktopPartyProgressComponent>();
        if (!viewProgHired.empty()) {
            auto& prog = registry.get<DesktopPartyProgressComponent>(viewProgHired.front());
            if (currentHeroCount != prog.hiredRoles.size()) {
                if (currentHeroCount > prog.hiredRoles.size()) {
                    std::vector<entt::entity> toDestroy;
                    for (auto e : heroView) toDestroy.push_back(e);
                    
                    for (auto e : toDestroy) {
                        if (registry.valid(e) && registry.all_of<DesktopHeroComponent>(e)) {
                            auto& hero = registry.get<DesktopHeroComponent>(e);
                            if (registry.valid(hero.hpBarBg)) registry.destroy(hero.hpBarBg);
                            if (registry.valid(hero.hpBarFg)) registry.destroy(hero.hpBarFg);
                            registry.destroy(e);
                        }
                    }
                    currentHeroCount = 0;
                }
                
                for (size_t i = currentHeroCount; i < prog.hiredRoles.size(); i++) {
                    int r = prog.hiredRoles[i];
                    entt::entity e = registry.create();
                    auto& hero = registry.emplace<DesktopHeroComponent>(e);
                    hero.role = static_cast<HeroRole>(r);
                    
                    registry.emplace<NameComponent>(e, "Char_" + std::to_string(i));
                    
                    auto& rect = registry.emplace<RectTransformComponent>(e);
                    rect.anchor = {0.0f, 0.0f}; rect.pivot = {0.0f, 0.0f};
                    rect.pos = {200.0f + (rand()%100), 400.0f + (rand()%200)};
                    rect.size = {targetScale, targetScale};
                    rect.enabled = true;
                    
                    auto& img = registry.emplace<UIImageComponent>(e);
                    
                    std::string customPath = "";
                    if ((int)hero.role < 5 && !prog.ugcTexturePaths[(int)hero.role].empty()) {
                        customPath = prog.ugcTexturePaths[(int)hero.role];
                    }
                    
                    if (!customPath.empty()) {
                        img.texturePath = customPath;
                    } else {
                        img.texturePath = "Resources/Textures/white1x1.png";
                    }
                    if (ctx.renderer) img.textureHandle = ctx.renderer->LoadTexture2D(img.texturePath, false);
                    img.layer = -5;
                    
                    DirectX::XMFLOAT4 heroColor = {1, 1, 1, 1};
                    int lv = 1;
                    if (r == 0) { lv = heroLevels[0]; hero.maxHp = 100.0f * static_cast<float>(std::pow(1.2, lv-1)); heroColor = {0.2f, 0.6f, 1.0f, 1.0f}; }
                    else if (r == 1) { lv = heroLevels[1]; hero.maxHp = 60.0f * static_cast<float>(std::pow(1.15, lv-1)); heroColor = {1.0f, 0.8f, 0.2f, 1.0f}; }
                    else if (r == 2) { lv = heroLevels[2]; hero.maxHp = 80.0f * static_cast<float>(std::pow(1.2, lv-1)); heroColor = {0.2f, 0.8f, 0.2f, 1.0f}; }
                    else if (r == 4) { lv = heroLevels[3]; hero.maxHp = 90.0f * static_cast<float>(std::pow(1.2, lv-1)); heroColor = {1.0f, 0.4f, 0.6f, 1.0f}; }
                    hero.hp = hero.maxHp;
                    
                    if (!customPath.empty()) {
                        img.color = {1.0f, 1.0f, 1.0f, 1.0f}; // UGCの場合は白（色なし）
                    } else {
                        img.color = heroColor;
                    }
                    
                    registry.emplace<SpriteCharacterAIComponent>(e);
                    
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
                }
            }
        }
        
        auto charView = registry.view<SpriteCharacterAIComponent, RectTransformComponent>();

        // --- 2. キャラクターのAI制御＆味方HP管理 ---
        auto getEquipBuffs = [&](int roleIndex, float& outAtk, float& outHp, float& outSpd) {
            outAtk = 0.0f; outHp = 0.0f; outSpd = 0.0f;
            if (!viewProg.empty()) {
                auto& prog = registry.get<DesktopPartyProgressComponent>(viewProg.front());
                if (roleIndex >= 0 && roleIndex < 5) {
                    for (int slot = 0; slot < 5; ++slot) {
                        int eqId = prog.equippedIds[roleIndex * 5 + slot];
                        if (eqId != -1) {
                            for (const auto& eq : prog.inventory) {
                                if (eq.id == static_cast<uint32_t>(eqId)) {
                                    outAtk += eq.atkMul; outHp += eq.hpMul; outSpd += eq.spdMul;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        };

        // 役職ごとのインデックスと総数を計算 (スケール調整とグリッド配置用)
        std::unordered_map<entt::entity, int> heroIndices;
        int roleCounts[5] = {0};
        auto heroCompView = registry.view<DesktopHeroComponent>();
        for (auto e : heroCompView) {
            auto& h = heroCompView.get<DesktopHeroComponent>(e);
            heroIndices[e] = roleCounts[(int)h.role]++;
        }


        for (auto entity : charView) {
            auto& ai = charView.get<SpriteCharacterAIComponent>(entity);
            auto& rect = charView.get<RectTransformComponent>(entity);
            if (!rect.enabled) continue;

            auto& hero = registry.get<DesktopHeroComponent>(entity);
            
            int count = roleCounts[(int)hero.role];
            if (count == 0) count = 1;
            
            float roleScaleRatio = 1.0f;
            if (count > 25) {
                roleScaleRatio = 25.0f / static_cast<float>(count);
                if (roleScaleRatio < 0.4f) roleScaleRatio = 0.4f; // 最小0.4倍まで縮小
            }
            
            float eqAtk = 0.0f, eqHp = 0.0f, eqSpd = 0.0f;
            getEquipBuffs((int)hero.role, eqAtk, eqHp, eqSpd);

            // 最大HPの動的更新
            float dummyHp, newMaxHp, dummyAtk, dummySpd;
            GetHeroStats(registry, (int)hero.role, dummyHp, newMaxHp, dummyAtk, dummySpd);
            if (std::abs(hero.maxHp - newMaxHp) > 0.1f) {
                float ratio = hero.hp / hero.maxHp;
                hero.maxHp = newMaxHp;
                hero.hp = newMaxHp * ratio;
            }
            
            // 味方のHPバー位置更新
            float hpBarScale = (std::max)(0.4f, roleScaleRatio);
            if (registry.valid(hero.hpBarBg)) {
                auto& bgRect = registry.get<RectTransformComponent>(hero.hpBarBg);
                bgRect.size = {32.0f * hpBarScale, 4.0f * hpBarScale};
                bgRect.pos = {rect.pos.x + rect.size.x/2.0f - 16.0f * hpBarScale, rect.pos.y - 8.0f * hpBarScale};
            }
            if (registry.valid(hero.hpBarFg)) {
                auto& fgRect = registry.get<RectTransformComponent>(hero.hpBarFg);
                float hpRatio = (std::max)(0.0f, hero.hp) / hero.maxHp;
                fgRect.size = {32.0f * hpBarScale * hpRatio, 4.0f * hpBarScale};
                fgRect.pos = {rect.pos.x + rect.size.x/2.0f - 16.0f * hpBarScale, rect.pos.y - 8.0f * hpBarScale};
            }
            
            // 武器位置の更新
            if (registry.valid(hero.weaponEntity)) {
                auto& wRect = registry.get<RectTransformComponent>(hero.weaponEntity);
                float scaleRatio = ctx.isStowed ? (8.0f / 32.0f) : roleScaleRatio;
                float offsetX = 16.0f;
                float offsetY = -8.0f;
                
                float attackInterval = (hero.role == HeroRole::Sniper) ? 4.0f : ((hero.role == HeroRole::Scout) ? 0.4f : 1.5f);
                if (hero.role == HeroRole::Healer) attackInterval = 1.5f;
                
                if (hero.attackTimer > attackInterval - 0.2f && hero.attackTimer > 0.0f) {
                    float attackAnim = (hero.attackTimer - (attackInterval - 0.2f)) / 0.2f;
                    offsetX += 10.0f * attackAnim;
                }
                
                wRect.size = {32.0f * scaleRatio, 32.0f * scaleRatio};
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
            bool shouldAttackTarget = false;
            float attackRange = 0.0f;
            float attackDmg = dummyAtk;
            float attackInterval = 1.0f;

            // グリッド（スロット）配置システム
            int index = heroIndices[entity];
            
            float unitSize = 32.0f * roleScaleRatio; // キャラの実際のサイズ(32*scale)に合わせる
            int maxRows = static_cast<int>((std::max)(1.0f, (screenH * 0.8f) / (unitSize * 1.2f)));
            if (maxRows > 50) maxRows = 50; // 50体過ぎたら2列目を作るように制限
            
            // 実際の列数を計算 (最大行数を超えないようにする)
            int actualRows = (count < maxRows) ? count : maxRows;
            if (actualRows < 1) actualRows = 1;
            
            int col = index / actualRows;
            int row = index % actualRows;
            
            // 行間隔を計算 (キャラの高さより大きくして確実に重ならないようにする)
            float rowSpacing = unitSize * 1.2f; 
            
            // 中央揃えのための開始Y座標計算
            float totalHeight = (actualRows - 1) * rowSpacing;
            float startY = castleCenterY - (totalHeight / 2.0f);
            
            float gridY = startY + row * rowSpacing;
            // 横にも確実に隙間を空ける
            float gridXOffset = col * (unitSize * 1.5f); 
            
            // 兵の大きさを変更（数が増えると縮小）
            rect.size = {32.0f * roleScaleRatio, 32.0f * roleScaleRatio};

            if (hero.role == HeroRole::Tank) { // 重装クロスボウ (最前線)
                targetX = castleCenterX + 350.0f - gridXOffset; 
                targetY = gridY;
                attackRange = 250.0f;
                attackInterval = 1.5f;
                shouldAttackTarget = true;
            } else if (hero.role == HeroRole::Sniper) { // ロングボウ (後衛)
                targetX = castleCenterX + 25.0f - gridXOffset; 
                targetY = gridY;
                attackRange = 2000.0f; // 無限
                attackInterval = 4.0f; // 遅いが高火力
                attackDmg *= 5.0f; 
                shouldAttackTarget = true;
            } else if (hero.role == HeroRole::Scout) { // 連射クロスボウ (中衛)
                targetX = castleCenterX + 200.0f - gridXOffset; 
                targetY = gridY;
                attackRange = 600.0f;
                attackInterval = 0.4f; // 手数が多いが低火力
                attackDmg *= 0.5f;
                shouldAttackTarget = true;
            } else if (hero.role == HeroRole::Healer) { // 癒やしの矢 (最背後)
                targetX = castleCenterX - 45.0f - gridXOffset; 
                targetY = gridY;
                attackInterval = 2.0f;
                shouldAttackTarget = false; // 味方を回復する
            }

            if (hero.hp <= 0.0f) {
                // HPが0以下の場合は倒れているため攻撃等の行動をしないが、描画は少し暗くして残す
                if (registry.all_of<UIImageComponent>(entity)) {
                    registry.get<UIImageComponent>(entity).color = {0.3f, 0.3f, 0.3f, 1.0f};
                }
                shouldAttackTarget = false;
            } else {
                // HPがある場合は通常カラー
                if (registry.all_of<UIImageComponent>(entity)) {
                    // roleColor_ is stored via img.color initially, but we might overwrite it.
                    // For now let's just make it fully opaque.
                    registry.get<UIImageComponent>(entity).color.x = (std::max)(0.4f, registry.get<UIImageComponent>(entity).color.x);
                    registry.get<UIImageComponent>(entity).color.y = (std::max)(0.4f, registry.get<UIImageComponent>(entity).color.y);
                    registry.get<UIImageComponent>(entity).color.z = (std::max)(0.4f, registry.get<UIImageComponent>(entity).color.z);
                    registry.get<UIImageComponent>(entity).color.w = 1.0f;
                }
            }

            if (ctx.isStowed) {
                // カーソル格納モード: キャラを非表示にし、カーソル（城）と同化させる
                rect.enabled = false;
                if (registry.valid(hero.weaponEntity)) registry.get<RectTransformComponent>(hero.weaponEntity).enabled = false;
                if (registry.valid(hero.hpBarBg)) registry.get<RectTransformComponent>(hero.hpBarBg).enabled = false;
                if (registry.valid(hero.hpBarFg)) registry.get<RectTransformComponent>(hero.hpBarFg).enabled = false;
                
                targetX = castleCenterX;
                targetY = castleCenterY;
                attackRange = 250.0f; // 射程を短くして、画面内に入ってから撃つようにする
            } else {
                rect.enabled = true;
                if (registry.valid(hero.weaponEntity)) registry.get<RectTransformComponent>(hero.weaponEntity).enabled = true;
                if (registry.valid(hero.hpBarBg)) registry.get<RectTransformComponent>(hero.hpBarBg).enabled = true;
                if (registry.valid(hero.hpBarFg)) registry.get<RectTransformComponent>(hero.hpBarFg).enabled = true;
            }

            // 移動ロジック
            float moveSpeed = ai.speed;
            moveSpeed *= (1.0f + globalSpdBuff + eqSpd);
            
            float dx = targetX - cx;
            float dy = targetY - cy;
            float targetDist = std::sqrt(dx*dx + dy*dy);
            
            if (targetDist > 10.0f) {
                ai.moveDir.x = dx / targetDist; ai.moveDir.y = dy / targetDist;
                
                // 展開時・格納時に素早くパッと元の位置に戻るためのスピード補正
                float speedMultiplier = 1.0f;
                if (targetDist > 300.0f) speedMultiplier = 15.0f;
                else if (targetDist > 100.0f) speedMultiplier = 4.0f;
                
                rect.pos.x += ai.moveDir.x * moveSpeed * speedMultiplier * ctx.dt;
                rect.pos.y += ai.moveDir.y * moveSpeed * speedMultiplier * ctx.dt;
            }

            // 攻撃/回復ロジック
            if (hero.role == HeroRole::Healer) {
                hero.attackTimer -= ctx.dt;
                
                // 城の回復チェック
                float castleRatio = castleRef.hp / castleRef.maxHp;
                if (castleRatio < 0.9f) {
                    if (hero.attackTimer <= 0.0f) {
                        float healAmt = 50.0f * (1.0f + skillLevelAtk * 0.5f);
                        entt::entity proj = registry.create();
                        auto& pRect = registry.emplace<RectTransformComponent>(proj);
                        pRect.anchor = {0.0f, 0.0f}; pRect.pivot = {0.0f, 0.0f};
                        pRect.pos = {cx, cy}; pRect.size = {8.0f, 8.0f}; pRect.enabled = true;
                        auto& pImg = registry.emplace<UIImageComponent>(proj);
                        pImg.texturePath = "Resources/Textures/white1x1.png";
                        if (ctx.renderer) pImg.textureHandle = ctx.renderer->LoadTexture2D(pImg.texturePath, false);
                        pImg.color = {0.2f, 1.0f, 0.2f, 1.0f}; pImg.layer = -5;
                        auto& projComp = registry.emplace<DesktopProjectileComponent>(proj);
                        projComp.targetEntity = castleEnt;
                        projComp.speed = 500.0f; projComp.damage = healAmt; projComp.isHeal = true; projComp.color = pImg.color;
                        projComp.startX = cx; projComp.startY = cy; projComp.progress = 0.0f; projComp.arcHeight = 50.0f;
                        hero.attackTimer = attackInterval;
                    }
                } else {
                    // 味方の回復チェック
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
                        if (hero.attackTimer <= 0.0f) {
                            float healAmt = 25.0f * (1.0f + skillLevelAtk * 0.5f);
                            entt::entity proj = registry.create();
                            auto& pRect = registry.emplace<RectTransformComponent>(proj);
                            pRect.anchor = {0.0f, 0.0f}; pRect.pivot = {0.0f, 0.0f};
                            pRect.pos = {cx, cy}; pRect.size = {8.0f, 8.0f}; pRect.enabled = true;
                            auto& pImg = registry.emplace<UIImageComponent>(proj);
                            pImg.texturePath = "Resources/Textures/white1x1.png";
                            if (ctx.renderer) pImg.textureHandle = ctx.renderer->LoadTexture2D(pImg.texturePath, false);
                            pImg.color = {0.2f, 1.0f, 0.2f, 1.0f}; pImg.layer = -5;
                            auto& projComp = registry.emplace<DesktopProjectileComponent>(proj);
                            projComp.targetEntity = lowestHpTarget;
                            projComp.speed = 500.0f; projComp.damage = healAmt; projComp.isHeal = true; projComp.color = pImg.color;
                            projComp.startX = cx; projComp.startY = cy; projComp.progress = 0.0f; projComp.arcHeight = 50.0f;
                            hero.attackTimer = attackInterval;
                        }
                    }
                }
            } else if (shouldAttackTarget && registry.valid(currentEnemy)) {
                if (isCoreFever) attackDmg *= 2.0f;
                
                auto& eRect = registry.get<RectTransformComponent>(currentEnemy);
                float ex = eRect.pos.x + eRect.size.x / 2.0f;
                float ey = eRect.pos.y + eRect.size.y / 2.0f;
                float distToEnemy = std::sqrt((ex-cx)*(ex-cx) + (ey-cy)*(ey-cy));
                
                if (ctx.isStowed) attackRange = 100.0f;
                
                if (distToEnemy <= attackRange) {
                    hero.attackTimer -= ctx.dt;
                    if (hero.attackTimer <= 0.0f) {
                        entt::entity proj = registry.create();
                        
                        int specialArrowType = -1;
                        if (hero.role == HeroRole::Sniper || hero.role == HeroRole::Scout) {
                            auto arrowViewProg = registry.view<DesktopPartyProgressComponent>();
                            for (auto entityProg : arrowViewProg) {
                                auto& prog = registry.get<DesktopPartyProgressComponent>(entityProg);
                                std::vector<int> availableSpecials;
                                for (int i=0; i<3; ++i) if (prog.specialArrows[i] > 0) availableSpecials.push_back(i);
                                
                                if (!availableSpecials.empty() && rand() % 100 < 30) { // 30%の確率で特殊矢を発射
                                    specialArrowType = availableSpecials[rand() % availableSpecials.size()];
                                    // 矢は消費しない（永続）
                                }
                                break;
                            }
                        }
                        
                        auto& pRect = registry.emplace<RectTransformComponent>(proj);
                        pRect.anchor = {0.0f, 0.0f}; pRect.pivot = {0.0f, 0.0f};
                        pRect.pos = {cx, cy}; pRect.size = {12.0f, 4.0f}; pRect.enabled = true;
                        auto& pImg = registry.emplace<UIImageComponent>(proj);
                        pImg.texturePath = "Resources/Textures/white1x1.png";
                        if (ctx.renderer) pImg.textureHandle = ctx.renderer->LoadTexture2D(pImg.texturePath, false);
                        
                        if (specialArrowType == 0) pImg.color = {1.0f, 0.4f, 0.1f, 1.0f}; // 爆発(オレンジ)
                        else if (specialArrowType == 1) pImg.color = {0.4f, 0.8f, 1.0f, 1.0f}; // 氷(水色)
                        else if (specialArrowType == 2) pImg.color = {0.8f, 0.2f, 1.0f, 1.0f}; // 分裂(紫)
                        else pImg.color = registry.get<UIImageComponent>(entity).color;
                        
                        pImg.layer = -5;
                        auto& projComp = registry.emplace<DesktopProjectileComponent>(proj);
                        projComp.targetEntity = currentEnemy;
                        projComp.speed = 800.0f; projComp.damage = attackDmg; projComp.isHeal = false; projComp.color = pImg.color;
                        projComp.specialType = specialArrowType;
                        projComp.startX = cx; projComp.startY = cy; projComp.progress = 0.0f;
                        float d = std::sqrt((ex-cx)*(ex-cx) + (ey-cy)*(ey-cy));
                        projComp.arcHeight = d * 0.2f; // 距離に応じて弧の高さを変える
                        hero.attackTimer = attackInterval;
                    }
                }
            }
            
            // 画面外に出ないようにクランプ
            // screenW, screenHは既に定義済みなので削除
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

        // --- 4. プロジェクタイル（矢・回復弾）の更新 ---
        auto projView = registry.view<DesktopProjectileComponent, RectTransformComponent>();
        std::vector<entt::entity> projsToDestroy;
        for (auto pEnt : projView) {
            auto& proj = projView.get<DesktopProjectileComponent>(pEnt);
            auto& pRect = projView.get<RectTransformComponent>(pEnt);
            
            float tx = proj.targetX, ty = proj.targetY;
            if (registry.valid(proj.targetEntity) && registry.all_of<RectTransformComponent>(proj.targetEntity)) {
                auto& tRect = registry.get<RectTransformComponent>(proj.targetEntity);
                tx = tRect.pos.x + tRect.size.x / 2.0f;
                ty = tRect.pos.y + tRect.size.y / 2.0f;
                proj.targetX = tx; proj.targetY = ty;
            }
            
            float dx = tx - proj.startX; float dy = ty - proj.startY;
            float totalDist = std::sqrt(dx*dx + dy*dy);
            if (totalDist < 0.1f) totalDist = 0.1f;
            
            proj.progress += (proj.speed * ctx.dt) / totalDist;
            
            if (proj.progress >= 1.0f) {
                if (registry.valid(proj.targetEntity)) {
                    if (proj.isHeal) {
                        if (registry.all_of<DesktopHeroComponent>(proj.targetEntity)) {
                            auto& th = registry.get<DesktopHeroComponent>(proj.targetEntity);
                            th.hp += proj.damage; if (th.hp > th.maxHp) th.hp = th.maxHp;
                        } else if (registry.all_of<DesktopCastleComponent>(proj.targetEntity)) {
                            auto& tc = registry.get<DesktopCastleComponent>(proj.targetEntity);
                            tc.hp += proj.damage; if (tc.hp > tc.maxHp) tc.hp = tc.maxHp;
                        }
                        CreateHealText(registry, tx - 20.0f, ty - 40.0f, proj.damage, ctx.isStowed);
                    } else if (registry.all_of<DesktopEnemyComponent>(proj.targetEntity)) {
                        auto& enemy = registry.get<DesktopEnemyComponent>(proj.targetEntity);
                        enemy.hp -= proj.damage;
                        totalDamage += proj.damage;
                        auto eff = CreateDamageText(registry, tx - 20.0f + (rand() % 40 - 20.0f), ty - 40.0f + (rand() % 40 - 20.0f), proj.damage, ctx.isStowed);
                        registry.get<UITextComponent>(eff).color = ctx.isStowed ? DirectX::XMFLOAT4{0.2f, 0.4f, 1.0f, 0.6f} : DirectX::XMFLOAT4{0.2f, 0.4f, 1.0f, 1.0f}; // 青色
                        
                        // --- 特殊矢の効果 ---
                        if (proj.specialType == 0) { // 爆発
                            auto allEnemies = registry.view<DesktopEnemyComponent, RectTransformComponent>();
                            for (auto oe : allEnemies) {
                                if (oe == proj.targetEntity) continue;
                                auto& oeRect = allEnemies.get<RectTransformComponent>(oe);
                                float ox = oeRect.pos.x + oeRect.size.x/2; float oy = oeRect.pos.y + oeRect.size.y/2;
                                float dist = std::sqrt((tx-ox)*(tx-ox) + (ty-oy)*(ty-oy));
                                if (dist < 100.0f) {
                                    allEnemies.get<DesktopEnemyComponent>(oe).hp -= proj.damage * 0.5f;
                                    totalDamage += proj.damage * 0.5f;
                                    CreateDamageText(registry, ox, oy - 20.0f, proj.damage * 0.5f, ctx.isStowed);
                                }
                            }
                            CreateDamageParticle(registry, ctx, tx, ty, "Resources/Textures/soft_circle.png", {1.0f, 0.5f, 0.1f, 1.0f}, 150.0f);
                        } else if (proj.specialType == 1) { // 氷
                            enemy.freezeTimer = 5.0f;
                            CreateDamageParticle(registry, ctx, tx, ty, "Resources/Textures/soft_circle.png", {0.4f, 0.8f, 1.0f, 1.0f}, 60.0f);
                        } else if (proj.specialType == 2) { // 分裂
                            auto allEnemies = registry.view<DesktopEnemyComponent, RectTransformComponent>();
                            int splitCount = 0;
                            for (auto oe : allEnemies) {
                                if (oe == proj.targetEntity) continue;
                                if (splitCount >= 3) break;
                                
                                entt::entity subProj = registry.create();
                                auto& subRect = registry.emplace<RectTransformComponent>(subProj);
                                subRect.pos = {tx, ty}; subRect.size = {8.0f, 4.0f}; subRect.enabled = true;
                                auto& pImg = registry.emplace<UIImageComponent>(subProj);
                                pImg.texturePath = "Resources/Textures/white1x1.png";
                                if (ctx.renderer) pImg.textureHandle = ctx.renderer->LoadTexture2D(pImg.texturePath, false);
                                pImg.color = {0.8f, 0.2f, 1.0f, 1.0f}; pImg.layer = -5;
                                auto& subProjComp = registry.emplace<DesktopProjectileComponent>(subProj);
                                subProjComp.targetEntity = oe;
                                subProjComp.speed = 800.0f; subProjComp.damage = proj.damage * 0.5f; subProjComp.isHeal = false;
                                subProjComp.startX = tx; subProjComp.startY = ty; subProjComp.progress = 0.0f; subProjComp.arcHeight = 0.0f;
                                subProjComp.specialType = -1;
                                splitCount++;
                            }
                            CreateDamageParticle(registry, ctx, tx, ty, "Resources/Textures/soft_circle.png", {0.8f, 0.2f, 1.0f, 1.0f}, 60.0f);
                        }
                    }
                    DirectX::XMFLOAT4 pColor = proj.isHeal ? DirectX::XMFLOAT4{0.2f, 1.0f, 0.2f, 1.0f} : DirectX::XMFLOAT4{0.2f, 0.4f, 1.0f, 1.0f};
                    CreateDamageParticle(registry, ctx, tx, ty, "Resources/Textures/soft_circle.png", pColor, ctx.isStowed ? 5.0f : 32.0f);
                }
                projsToDestroy.push_back(pEnt);
            } else {
                float currentX = proj.startX + dx * proj.progress;
                float currentY = proj.startY + dy * proj.progress;
                // 弧を描くためのオフセット (二次関数: y = -4h * x * (x - 1))
                float arcY = -4.0f * proj.arcHeight * proj.progress * (proj.progress - 1.0f);
                pRect.pos.x = currentX;
                pRect.pos.y = currentY - arcY;
            }
        }
        for (auto e : projsToDestroy) {
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
