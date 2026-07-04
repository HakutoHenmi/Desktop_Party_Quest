#include "MainScene.h"
#include "../../Engine/Audio.h"
#include "../../Engine/Input.h"
#include "../../Engine/Renderer.h"
#include "../../Engine/SceneManager.h"
#include "../../Engine/WindowDX.h"
#include "../../Engine/PathUtils.h"
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

#include <cmath>

namespace Game {

MainScene::~MainScene() {
}

void MainScene::Initialize(Engine::WindowDX* dx, const Engine::SceneParameters& /*params*/) {
    dx_ = dx;
    renderer_ = Engine::Renderer::GetInstance();
    lastTime_ = std::chrono::steady_clock::now();

    // Initialize Camera
    camera_.Initialize();
    float aspect = (float)Engine::WindowDX::kW / (float)Engine::WindowDX::kH;
    camera_.SetProjection(DirectX::XMConvertToRadians(60.0f), aspect, 0.1f, 1000.0f);
    camera_.SetPosition({0.0f, 5.0f, -10.0f});
    camera_.LookAt({0.0f, 0.0f, 0.0f}, {0, 1, 0});

    if (renderer_) {
        renderer_->SetUseCubemapBackground(false);
        renderer_->SetPostProcessEnabled(false); // ★変更: ポストエフェクトを使わない
        // renderer_->SetPostEffect("OutlinePost"); // ★変更: コードは残すが使わない
        
        // Lighting setup
        renderer_->SetAmbientColor({0.3f, 0.3f, 0.3f});
        renderer_->SetDirectionalLight({0.5f, -1.0f, 0.5f}, {1.0f, 1.0f, 1.0f}, true);
    }

    // Context setup
    context_.camera = &camera_;
    context_.eventSystem = &eventSystem_;
    context_.renderer = renderer_;
    context_.input = Engine::Input::GetInstance(); // ★追加: これがないとUIがクリックに反応しない
    context_.scene = this;
    context_.isPlaying = true; // ★追加: これがないとAI等のゲーム内Systemが動かない

    // Initialize Systems
    auto uiSystem = std::make_unique<UISystem>();
    uiSystem->Reset(registry_);
    systems_.push_back(std::move(uiSystem));

    auto workEnergySystem = std::make_unique<WorkEnergySystem>();
    workSys_ = workEnergySystem.get();
    workEnergySystem->Reset(registry_);
    systems_.push_back(std::move(workEnergySystem));

    auto combatSystem = std::make_unique<DesktopCombatSystem>();
    combatSys_ = combatSystem.get();
    combatSystem->Reset(registry_);
    systems_.push_back(std::move(combatSystem));

    auto spriteAISystem = std::make_unique<SpriteCharacterAISystem>();
    systems_.push_back(std::move(spriteAISystem));

    // ヒーローはDesktopCombatSystemが自動生成するようになりました。

    {
        const float UI_OFFSET_X = 0.0f;
        const float UI_OFFSET_Y = 80.0f;

        auto createPanel = [&](const std::string& name, float x, float y, float w, float h, DirectX::XMFLOAT4 color, int z = 0) {
            auto ent = registry_.create();
            registry_.emplace<NameComponent>(ent, name);
            auto& t = registry_.emplace<RectTransformComponent>(ent);
            t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
            t.pos = {x + UI_OFFSET_X, y + UI_OFFSET_Y}; t.size = {w, h};
            auto& img = registry_.emplace<UIImageComponent>(ent);
            img.texturePath = "Resources/Textures/white1x1.png"; // ★変更: 丸角画像がフェイク透過だったため修正
            img.textureHandle = renderer_->LoadTexture2D(img.texturePath);
            img.color = color;
            img.layer = z;
            return ent;
        };

        auto createText = [&](const std::string& name, const std::string& textStr, float x, float y, float fontSize, DirectX::XMFLOAT4 color) {
            auto ent = registry_.create();
            registry_.emplace<NameComponent>(ent, name);
            auto& t = registry_.emplace<RectTransformComponent>(ent);
            t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
            t.pos = {x + UI_OFFSET_X, y + UI_OFFSET_Y}; t.size = {0, 0};
            auto& text = registry_.emplace<UITextComponent>(ent);
            text.text = textStr;
            text.fontSize = fontSize;
            text.color = color;
            text.fontPath = "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf";
            return ent;
        };

        auto createButton = [&](const std::string& name, const std::string& textStr, float x, float y, float w, float h, DirectX::XMFLOAT4 baseColor, float fontSize = 20.0f, DirectX::XMFLOAT4 textColor = {0.0f, 0.0f, 0.0f, 1.0f}) {
            auto ent = registry_.create();
            registry_.emplace<NameComponent>(ent, name);
            auto& t = registry_.emplace<RectTransformComponent>(ent);
            t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
            t.pos = {x + UI_OFFSET_X, y + UI_OFFSET_Y}; t.size = {w, h};
            auto& img = registry_.emplace<UIImageComponent>(ent);
            img.texturePath = "Resources/Textures/white1x1.png"; // ★変更
            img.textureHandle = renderer_->LoadTexture2D(img.texturePath);
            img.color = baseColor;
            img.layer = 1;
            auto& btn = registry_.emplace<UIButtonComponent>(ent);
            btn.normalColor = baseColor;
            btn.hoverColor = {baseColor.x * 0.85f, baseColor.y * 0.85f, baseColor.z * 0.85f, 1.0f};
            btn.pressedColor = {baseColor.x * 0.7f, baseColor.y * 0.7f, baseColor.z * 0.7f, 1.0f};
            
            auto& text = registry_.emplace<UITextComponent>(ent);
            text.text = textStr;
            text.fontSize = fontSize;
            text.color = textColor;
            text.fontPath = "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf";
            return ent;
        };

        // --- パネル構築 ---
        // ドロップシャドウ
        createPanel("Shadow", 1290, 40, 590, 710, {0.0f, 0.0f, 0.0f, 0.2f}, 0);
        // 背景パネル (白)
        createPanel("MainPanel", 1300, 40, 580, 700, {1.0f, 1.0f, 1.0f, 1.0f}, 0);

        // --- ヘッダー領域 ---
        createText("Title", "司令室", 1320, 60, 32.0f, {0.1f, 0.1f, 0.1f, 1.0f});
        createText("Gold", "所持ゴールド: 1250 G", 1430, 60, 16.0f, {0.7f, 0.5f, 0.0f, 1.0f});
        createText("EnergyHeader", "エネルギー: 0 / 100", 1430, 80, 16.0f, {0.8f, 0.2f, 0.5f, 1.0f});
        
        // 格納ボタン (オービタル格納)
        stowButton_ = createButton("StowBtn", "⬇ オービタル格納", 1720, 65, 150, 30, {0.9f, 0.9f, 0.9f, 1.0f}, 14.0f);

        // オービタル格納時の復帰ボタン（画面右下に配置）
        RECT wa = Engine::WindowDX::GetWorkArea();
        float bottomY = wa.bottom - 48.0f;
        float rightX = wa.right - 120.0f;
        unstowBtn_ = createButton("BottomUnstow", "司令室 復帰 ⬆", rightX, bottomY, 110, 32, {0.3f, 0.5f, 0.9f, 1.0f}, 14.0f, {1.0f, 1.0f, 1.0f, 1.0f});

        // 初期モード設定
        SwitchAppMode(AppMode::Fullscreen);

        // --- タブメニュー ---
        tabCommandBtn_ = createButton("TabCommand", "作戦", 1320, 110, 105, 45, {0.95f, 0.95f, 0.95f, 1.0f}, 16.0f, {0.4f, 0.4f, 0.4f, 1.0f});
        tabRoomBtn_    = createButton("TabRoom", "聖域", 1430, 110, 105, 45, {0.9f, 0.9f, 1.0f, 1.0f}, 16.0f, {0.0f, 0.0f, 0.0f, 1.0f});
        tabUpgradeBtn_ = createButton("TabUpgrade", "育成", 1540, 110, 105, 45, {0.95f, 0.95f, 0.95f, 1.0f}, 16.0f, {0.4f, 0.4f, 0.4f, 1.0f});
        tabSystemBtn_  = createButton("TabSystem", "システム", 1650, 110, 105, 45, {0.95f, 0.95f, 0.95f, 1.0f}, 16.0f, {0.4f, 0.4f, 0.4f, 1.0f});
        tabStatusBtn_  = createButton("TabStatus", "データ", 1760, 110, 105, 45, {0.95f, 0.95f, 0.95f, 1.0f}, 16.0f, {0.4f, 0.4f, 0.4f, 1.0f});
        tabActiveLine_ = createPanel("ActiveTabLine", 1320, 151, 105, 4, {0.2f, 0.4f, 1.0f, 1.0f}, 1);

        // --- 1. 作戦タブ (Command) ---
        commandTabEntities_.push_back(createPanel("CommandBG", 1320, 180, 530, 320, {0.95f, 0.96f, 0.98f, 1.0f}, 0));
        
        // 城壁耐久度
        commandTabEntities_.push_back(createText("CastleHPLabel", "城壁耐久度:", 1330, 190, 16.0f, {0.4f, 0.4f, 0.4f, 1.0f}));
        commandTabEntities_.push_back(createText("CastleHPValue", "1000 / 1000", 1430, 190, 16.0f, {0.8f, 0.2f, 0.2f, 1.0f}));
        commandTabEntities_.push_back(createPanel("CastleHPBarBG", 1560, 195, 280, 10, {0.3f, 0.1f, 0.1f, 1.0f}, 1));
        commandTabEntities_.push_back(createPanel("CastleHPBarFG", 1560, 195, 280, 10, {0.9f, 0.2f, 0.2f, 1.0f}, 2));
        
        // 雇用枠
        commandTabEntities_.push_back(createText("CommandHeaderE", "雇用枠:", 1330, 220, 16.0f, {0.4f, 0.4f, 0.4f, 1.0f}));
        commandTabEntities_.push_back(createText("CommandHeaderEV", "4 / 4", 1430, 220, 16.0f, {0.2f, 0.5f, 0.8f, 1.0f}));
        commandTabEntities_.push_back(createPanel("HireLimitBarBG", 1560, 225, 280, 10, {0.1f, 0.1f, 0.3f, 1.0f}, 1));
        commandTabEntities_.push_back(createPanel("HireLimitBarFG", 1560, 225, 280, 10, {0.2f, 0.5f, 0.9f, 1.0f}, 2));
        
        // 雇用ボタン類
        commandTabEntities_.push_back(createButton("HireTank", "重装クロスボウ雇用(100G)", 1340, 260, 230, 35, {1.0f, 0.7f, 0.7f, 1.0f}, 15.0f));
        commandTabEntities_.push_back(createButton("HireSniper", "ロングボウ雇用(150G)", 1590, 260, 230, 35, {1.0f, 0.9f, 0.6f, 1.0f}, 15.0f));
        commandTabEntities_.push_back(createButton("HireScout", "連射クロスボウ雇用(50G)", 1340, 305, 230, 35, {0.6f, 0.9f, 0.6f, 1.0f}, 15.0f));
        commandTabEntities_.push_back(createButton("HireHealer", "癒やしの矢雇用(200G)", 1590, 305, 230, 35, {1.0f, 0.7f, 0.9f, 1.0f}, 15.0f));
        
        commandTabEntities_.push_back(createButton("ExpandLimit", "雇用枠を拡張する (500G)", 1340, 350, 480, 35, {0.2f, 0.4f, 0.9f, 1.0f}, 16.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        
        commandTabEntities_.push_back(createText("ChestHeader", "宝箱: 0個", 1340, 400, 18.0f, {0.8f, 0.4f, 0.1f, 1.0f}));
        commandTabEntities_.push_back(createButton("OpenChestBtn", "開封", 1450, 395, 80, 35, {1.0f, 0.8f, 0.2f, 1.0f}, 16.0f));
        commandTabEntities_.push_back(createText("ArrowInfo", "爆:0 氷:0 分:0", 1550, 400, 18.0f, {0.2f, 0.2f, 0.2f, 1.0f}));
        
        commandTabEntities_.push_back(createPanel("GachaBG", 1320, 440, 530, 45, {0.8f, 0.9f, 0.8f, 1.0f}, 0));
        commandTabEntities_.push_back(createText("GachaHeader", "謎のデータ断片: 0個", 1340, 450, 18.0f, {0.1f, 0.3f, 0.1f, 1.0f}));
        commandTabEntities_.push_back(createButton("GachaBtn", "データ解析 (ガチャ)", 1600, 445, 240, 35, {0.2f, 0.8f, 0.2f, 1.0f}, 16.0f, {1.0f, 1.0f, 1.0f, 1.0f}));

        // --- 2. 聖域タブ (Room) ---
        DirectX::XMFLOAT4 borderColor = {0.2f, 0.7f, 1.0f, 1.0f};
        roomTabEntities_.push_back(createPanel("ChamberBorderTop",    1316, 176, 538, 4, borderColor, 2));
        roomTabEntities_.push_back(createPanel("ChamberBorderBottom", 1316, 380, 538, 4, borderColor, 2));
        roomTabEntities_.push_back(createPanel("ChamberBorderLeft",   1316, 180, 4, 200, borderColor, 2));
        roomTabEntities_.push_back(createPanel("ChamberBorderRight",  1850, 180, 4, 200, borderColor, 2));
        roomTabEntities_.push_back(createPanel("ChamberBG", 1320, 180, 530, 200, {0.05f, 0.05f, 0.1f, 1.0f}, 0));
        
        // EnergyCoreの幅64、高さ48の半分を引いて枠の中央に配置
        coreEntity_ = createPanel("EnergyCore", 1320 + 265 - 32, 180 + 100 - 24, 64, 48, {1.0f, 1.0f, 1.0f, 1.0f}, 1);
        auto& coreImg = registry_.get<UIImageComponent>(coreEntity_);
        coreImg.texturePath = "Resources/Textures/white1x1.png";
        coreImg.color = {1.0f, 0.3f, 0.5f, 1.0f};
        if (renderer_) coreImg.textureHandle = renderer_->LoadTexture2D(coreImg.texturePath);
        roomTabEntities_.push_back(coreEntity_);
        
        roomTabEntities_.push_back(createText("RoomHeader", "聖域チャンバーの模様替え", 1320, 400, 20.0f, {0.2f, 0.2f, 0.2f, 1.0f}));
        roomTabEntities_.push_back(createButton("RoomNormal", "標準チャンバー                  [適用中]", 1320, 440, 530, 45, {0.7f, 1.0f, 0.7f, 1.0f}, 16.0f));
        roomTabEntities_.push_back(createButton("RoomCyber", "サイバー空間                      500 G", 1320, 490, 530, 45, {0.98f, 0.98f, 0.98f, 1.0f}, 16.0f));
        roomTabEntities_.push_back(createButton("RoomAntique", "アンティーク調                  1000 G", 1320, 540, 530, 45, {1.0f, 1.0f, 1.0f, 1.0f}, 16.0f));
        roomTabEntities_.push_back(createButton("RoomGold", "黄金の聖域                        5000 G", 1320, 590, 530, 45, {0.98f, 0.98f, 0.98f, 1.0f}, 16.0f));
        
        // --- 3. システムタブ (System) ---
        systemTabEntities_.push_back(createText("StatusInfo", "キャラ画像変更 (UGC)", 1320, 180, 16.0f, {0.2f, 0.2f, 0.2f, 1.0f}));
        systemTabEntities_.push_back(createButton("UGCSelectChar", "タンク", 1320, 210, 150, 35, {1.0f, 1.0f, 1.0f, 1.0f}, 16.0f, {0.0f, 0.0f, 0.0f, 1.0f}));
        systemTabEntities_.push_back(createButton("UGCFile", "ファイルを選択", 1490, 210, 120, 35, {0.9f, 0.9f, 0.9f, 1.0f}, 14.0f, {0.2f, 0.2f, 0.2f, 1.0f}));
        systemTabEntities_.push_back(createText("UGCNothing", "選択されていません", 1630, 220, 14.0f, {0.5f, 0.5f, 0.5f, 1.0f}));
        
        systemTabEntities_.push_back(createButton("PrestigeBtn", "システム再起動 (プレステージ)", 1320, 260, 530, 40, {1.0f, 0.4f, 0.4f, 1.0f}, 16.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        
        // --- 4. 育成タブ (Upgrade) ---
        upgradeTabEntities_.push_back(createText("StatHeader2", "パーティーステータス詳細", 1320, 180, 18.0f, {0.2f, 0.2f, 0.2f, 1.0f}));
        
        auto createStatusRow = [&](int idx, const std::string& name, const std::string& lv, const std::string& hp, const std::string& atk, const std::string& spd) {
            float y = 220.0f + idx * 90.0f;
            upgradeTabEntities_.push_back(createPanel("RowBG", 1320, y, 530, 85, (idx % 2 == 0) ? DirectX::XMFLOAT4{1,1,1,1} : DirectX::XMFLOAT4{0.98f,0.98f,0.98f,1}, 0));
            
            // 1行目: キャラ名, Lv, ボタン
            upgradeTabEntities_.push_back(createText("R_Name" + std::to_string(idx), name, 1340, y + 10, 16.0f, {0.2f,0.2f,0.2f,1}));
            upgradeTabEntities_.push_back(createText("R_LvLbl" + std::to_string(idx), "Lv", 1480, y + 10, 14.0f, {0.5f,0.5f,0.5f,1}));
            upgradeTabEntities_.push_back(createText("R_Lv" + std::to_string(idx), lv, 1500, y + 10, 16.0f, {0.2f,0.2f,0.2f,1}));
            if (idx < 4) {
                upgradeTabEntities_.push_back(createButton("R_UpBtn" + std::to_string(idx), "LvUP", 1700, y + 5, 70, 28, {0.7f, 1.0f, 0.7f, 1.0f}, 14.0f));
                upgradeTabEntities_.push_back(createButton("R_Equip" + std::to_string(idx), "装備変更", 1775, y + 5, 70, 28, {0.9f, 0.9f, 0.9f, 1.0f}, 14.0f));
            } else {
                upgradeTabEntities_.push_back(createButton("R_Equip" + std::to_string(idx), "装備変更", 1765, y + 5, 80, 28, {0.9f, 0.9f, 0.9f, 1.0f}, 14.0f));
            }
            
            // 2行目: HP, ATK
            upgradeTabEntities_.push_back(createText("R_HPLbl" + std::to_string(idx), "HP:", 1350, y + 35, 14.0f, {0.5f,0.5f,0.5f,1}));
            upgradeTabEntities_.push_back(createText("R_HP" + std::to_string(idx), hp, 1380, y + 35, 14.0f, {0.2f,0.6f,0.2f,1}));
            
            upgradeTabEntities_.push_back(createText("R_ATKLbl" + std::to_string(idx), "ATK:", 1580, y + 35, 14.0f, {0.5f,0.5f,0.5f,1}));
            upgradeTabEntities_.push_back(createText("R_ATK" + std::to_string(idx), atk, 1620, y + 35, 14.0f, {0.8f,0.2f,0.2f,1}));
            
            // 3行目: SPD
            upgradeTabEntities_.push_back(createText("R_SPDLbl" + std::to_string(idx), "SPD:", 1350, y + 60, 14.0f, {0.5f,0.5f,0.5f,1}));
            upgradeTabEntities_.push_back(createText("R_SPD" + std::to_string(idx), spd, 1380, y + 60, 14.0f, {0.2f,0.2f,0.8f,1}));
        };
        createStatusRow(0, "重装クロスボウ", "1", "100/100", "10", "2.0");
        createStatusRow(1, "ロングボウ", "1", "60/60", "25", "2.2");
        createStatusRow(2, "連射クロスボウ", "1", "80/80", "5", "4.0");
        createStatusRow(3, "癒やしの矢", "1", "80/80", "8", "1.5");
        createStatusRow(4, "支援枠", "1", "90/90", "5", "2.5");

        // --- 5. データタブ (Status) ---
        statusTabEntities_.push_back(createPanel("StatPanelTop", 1320, 180, 530, 40, {0.15f, 0.25f, 0.35f, 1.0f}, 0));
        statusTabEntities_.push_back(createText("StatHeader1", "過去の戦績・統計", 1340, 190, 18.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        statusTabEntities_.push_back(createPanel("StatPanelBG", 1320, 220, 530, 160, {0.95f, 0.96f, 0.98f, 1.0f}, 0));
        statusTabEntities_.push_back(createText("Stat1", "倒した敵の数", 1340, 240, 16.0f, {0.3f, 0.3f, 0.3f, 1.0f}));
        statusTabEntities_.push_back(createText("Stat1V", "0 体", 1750, 240, 16.0f, {0.1f, 0.1f, 0.1f, 1.0f}));
        statusTabEntities_.push_back(createText("Stat2", "総与ダメージ", 1340, 275, 16.0f, {0.3f, 0.3f, 0.3f, 1.0f}));
        statusTabEntities_.push_back(createText("Stat2V", "0 pts", 1750, 275, 16.0f, {0.1f, 0.1f, 0.1f, 1.0f}));
        statusTabEntities_.push_back(createText("Stat3", "総獲得ゴールド", 1340, 310, 16.0f, {0.3f, 0.3f, 0.3f, 1.0f}));
        statusTabEntities_.push_back(createText("Stat3V", "0 G", 1750, 310, 16.0f, {0.1f, 0.1f, 0.1f, 1.0f}));
        statusTabEntities_.push_back(createText("Stat4", "取得アイテム数", 1340, 345, 16.0f, {0.3f, 0.3f, 0.3f, 1.0f}));
        statusTabEntities_.push_back(createText("Stat4V", "0 個", 1750, 345, 16.0f, {0.1f, 0.1f, 0.1f, 1.0f}));

        statusTabEntities_.push_back(createButton("RelicEncyclopediaBtn", "遺物図鑑", 1320, 400, 250, 45, {0.9f, 0.8f, 1.0f, 1.0f}, 18.0f, {0.2f, 0.0f, 0.4f, 1.0f}));
        statusTabEntities_.push_back(createButton("EquipEncyclopediaBtn", "装備図鑑", 1600, 400, 250, 45, {0.8f, 0.9f, 1.0f, 1.0f}, 18.0f, {0.0f, 0.2f, 0.4f, 1.0f}));

        // 初期化ボタンはシステムタブの一番下に移動
        systemTabEntities_.push_back(createButton("HardResetBtn", "システムデータ初期化", 1320, 600, 530, 45, {1.0f, 0.3f, 0.3f, 1.0f}, 18.0f, {1.0f, 1.0f, 1.0f, 1.0f}));

        // --- 全タブ共通: 戦闘ログ領域 ---
        createPanel("CommonLogBG", 1300, 750, 580, 140, {0.05f, 0.05f, 0.08f, 0.9f}, 0);
        createText("CommonLogTitle", "戦闘ログ / システムメッセージ", 1320, 760, 14.0f, {0.5f, 0.6f, 0.8f, 1.0f});
        for (int i = 0; i < 5; i++) {
            // 全タブ共通のログテキストはどのベクターにも入れず、常に表示されるようにする
            auto logTxt = createText("LogText" + std::to_string(i), "", 1320.0f, 785.0f + static_cast<float>(i) * 18.0f, 14.0f, {0.8f, 0.8f, 0.8f, 1.0f});
            logTextEntities_.push_back(logTxt);
        }

        AddLog("システム起動...");
        AddLog("司令室への接続完了。");
        
        // 初期タブ設定
        SwitchTab(TabType::Command);

    }
}

void MainScene::Update() {
    auto now = std::chrono::steady_clock::now();
    dt_ = std::chrono::duration<float>(now - lastTime_).count();
    lastTime_ = now;
    if (dt_ > 0.1f)
        dt_ = 1.0f / 60.0f;
    totalTime_ += dt_;
    context_.dt = dt_;

    if (dx_ && dx_->GetHwnd()) {
        HWND hwnd = dx_->GetHwnd();
        POINT pt;
        if (GetCursorPos(&pt)) {
            ScreenToClient(hwnd, &pt);
            bool overUI = false;
            
            auto view = registry_.view<RectTransformComponent>();
            for (auto entity : view) {
                auto& rect = view.get<RectTransformComponent>(entity);
                if (!rect.enabled) continue;
                
                // 背景を暗くするだけのPopupOverlayはクリック判定から除外（デスクトップへのパススルーを許可）
                if (registry_.all_of<NameComponent>(entity)) {
                    if (registry_.get<NameComponent>(entity).name == "PopupOverlay") continue;
                }
                
                float minX = rect.pos.x - rect.size.x * rect.pivot.x;
                float minY = rect.pos.y - rect.size.y * rect.pivot.y;
                float maxX = minX + rect.size.x;
                float maxY = minY + rect.size.y;
                
                if (pt.x >= minX && pt.x <= maxX && pt.y >= minY && pt.y <= maxY) {
                    overUI = true;
                    break;
                }
            }

            LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            if (overUI) {
                // UIの上なら透過を解除して入力を受け取る
                if (exStyle & WS_EX_TRANSPARENT) {
                    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
                }
                
                // ログ領域のスクロール判定
                if (currentTab_ == TabType::Command && pt.x >= 1320 && pt.x <= 1850 && pt.y >= 750 && pt.y <= 900) {
                    if (context_.input) {
                        float wheel = context_.input->GetMouseWheelDelta();
                        if (wheel > 0.0f) {
                            logScrollOffset_++;
                            UpdateLogUI();
                        } else if (wheel < 0.0f) {
                            logScrollOffset_--;
                            if (logScrollOffset_ < 0) logScrollOffset_ = 0;
                            UpdateLogUI();
                        }
                    }
                }
                
                // ツールチップの更新
                UpdateTooltip(static_cast<float>(pt.x), static_cast<float>(pt.y));
                
            } else {
                // UI以外なら透過にして背面デスクトップへ入力を流す
                if (!(exStyle & WS_EX_TRANSPARENT)) {
                    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
                }
            }
        }
    }

    camera_.Tick(dt_);
    if (renderer_) {
        renderer_->SetCamera(camera_);
    }

    // Update Systems
    context_.isStowed = IsStowed();
    for (auto& sys : systems_) {
        sys->Update(registry_, context_);
    }

    // タブクリック判定
    auto checkBtnClick = [&](entt::entity btnEnt, std::function<void()> onClick) {
        if (!registry_.valid(btnEnt)) return;
        
        if (!popupEntities_.empty()) {
            bool isPopupBtn = (std::find(popupEntities_.begin(), popupEntities_.end(), btnEnt) != popupEntities_.end());
            if (!isPopupBtn) return;
        }
        
        auto& btn = registry_.get<UIButtonComponent>(btnEnt);
        auto& rect = registry_.get<RectTransformComponent>(btnEnt);
        if (rect.enabled && btn.isHovered && context_.input->IsMouseTrigger(0)) {
            onClick();
        }
    };
    checkBtnClick(tabCommandBtn_, [&](){ SwitchTab(TabType::Command); });
    checkBtnClick(tabRoomBtn_,    [&](){ SwitchTab(TabType::Room); });
    checkBtnClick(tabUpgradeBtn_, [&](){ SwitchTab(TabType::Upgrade); });
    checkBtnClick(tabSystemBtn_,  [&](){ SwitchTab(TabType::System); });
    checkBtnClick(tabStatusBtn_,  [&](){ SwitchTab(TabType::Status); });
    
    bool requestClosePopup = false;
    bool requestShowRelicPopup = false;
    bool requestShowPrestigePopup = false;
    bool requestShowGameOverPopup = false;
    int requestShowEquipPopupIdx = -1;
    int requestEquipId = -1;
    int requestEquipRoleIdx = -1;

    // 装備インベントリのスクロール判定
    if (!popupEntities_.empty() && currentEquipRoleIdx_ != -1 && context_.input) {
        float wheel = context_.input->GetMouseWheelDelta();
        if (std::abs(wheel) > 0.01f) {
            equipPopupScrollY_ -= wheel * 150.0f; // ホイール量に応じてスクロール
            if (equipPopupScrollY_ < 0.0f) equipPopupScrollY_ = 0.0f;
            requestShowEquipPopupIdx = currentEquipRoleIdx_;
        }
    }

    // AIモード変更ボタン
    auto nameView = registry_.view<NameComponent>();
    for (auto e : nameView) {
        auto& n = nameView.get<NameComponent>(e);
        if (n.name == "HireTank") {
            checkBtnClick(e, [&](){ 
                auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                if (combatSys_ && !viewProg.empty()) {
                    auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                    if (prog.hiredRoles.size() >= prog.maxDeploymentLimit) { AddLog("雇用枠が上限に達しています！"); return; }
                    if (combatSys_->currentGold >= 100) {
                        combatSys_->currentGold -= 100;
                        prog.hiredRoles.push_back(0); // Tank
                        combatSys_->SaveProgress(registry_);
                        AddLog("> 重装クロスボウを新しく雇用しました！");
                    } else { AddLog("ゴールドが足りません！"); }
                }
            });
        }
        else if (n.name == "HireSniper") {
            checkBtnClick(e, [&](){ 
                auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                if (combatSys_ && !viewProg.empty()) {
                    auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                    if (prog.hiredRoles.size() >= prog.maxDeploymentLimit) { AddLog("雇用枠が上限に達しています！"); return; }
                    if (combatSys_->currentGold >= 150) {
                        combatSys_->currentGold -= 150;
                        prog.hiredRoles.push_back(1); // Sniper
                        combatSys_->SaveProgress(registry_);
                        AddLog("> ロングボウを新しく雇用しました！");
                    } else { AddLog("ゴールドが足りません！"); }
                }
            });
        }
        else if (n.name == "HireScout") {
            checkBtnClick(e, [&](){ 
                auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                if (combatSys_ && !viewProg.empty()) {
                    auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                    if (prog.hiredRoles.size() >= prog.maxDeploymentLimit) { AddLog("雇用枠が上限に達しています！"); return; }
                    if (combatSys_->currentGold >= 50) {
                        combatSys_->currentGold -= 50;
                        prog.hiredRoles.push_back(2); // Scout
                        combatSys_->SaveProgress(registry_);
                        AddLog("> 連射クロスボウを新しく雇用しました！");
                    } else { AddLog("ゴールドが足りません！"); }
                }
            });
        }
        else if (n.name == "HireHealer") {
            checkBtnClick(e, [&](){ 
                auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                if (combatSys_ && !viewProg.empty()) {
                    auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                    if (prog.hiredRoles.size() >= prog.maxDeploymentLimit) { AddLog("雇用枠が上限に達しています！"); return; }
                    if (combatSys_->currentGold >= 200) {
                        combatSys_->currentGold -= 200;
                        prog.hiredRoles.push_back(4); // Healer
                        combatSys_->SaveProgress(registry_);
                        AddLog("> 癒やしの矢を新しく雇用しました！");
                    } else { AddLog("ゴールドが足りません！"); }
                }
            });
        }
        else if (n.name == "ExpandLimit") {
            checkBtnClick(e, [&](){ 
                auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                if (combatSys_ && !viewProg.empty()) {
                    auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                    if (combatSys_->currentGold >= 500) {
                        combatSys_->currentGold -= 500;
                        prog.maxDeploymentLimit++;
                        combatSys_->SaveProgress(registry_);
                        AddLog("> 雇用枠を 1 拡張しました！ (現在: " + std::to_string(prog.maxDeploymentLimit) + ")");
                    } else { AddLog("ゴールドが足りません！"); }
                }
            });
        }
        else if (n.name == "Btn1" || n.name == "Btn2" || n.name == "Btn3" || n.name == "Btn4") {
            int heroIdx = (n.name == "Btn1") ? 0 : (n.name == "Btn2") ? 1 : (n.name == "Btn3") ? 2 : 3;
            std::string heroNames[] = {"タンク", "スナイパー", "スカウト", "エンジニア"};
            checkBtnClick(e, [&, heroIdx, heroNames](){ 
                if (combatSys_) {
                    double cost = combatSys_->GetLevelUpCost(heroIdx);
                    if (combatSys_->currentGold >= cost) {
                        combatSys_->currentGold -= cost;
                        combatSys_->heroLevels[heroIdx]++;
                        combatSys_->SaveProgress(registry_);
                        AddLog("> " + heroNames[heroIdx] + "を強化しました！ (Lv." + std::to_string(combatSys_->heroLevels[heroIdx]) + ")");
                    } else {
                        AddLog("ゴールドが足りません！");
                    }
                }
            });
        }
        else if (n.name == "RoomNormal") {
            checkBtnClick(e, [&](){ 
                if (combatSys_) {
                    combatSys_->currentTheme = 0;
                    combatSys_->SaveProgress(registry_);
                    AddLog("> 司令室を標準チャンバーに変更しました。"); 
                }
            });
        }
        else if (n.name == "RoomCyber") {
            checkBtnClick(e, [&](){ 
                if (combatSys_) {
                    if (combatSys_->themeOwned[1]) {
                        combatSys_->currentTheme = 1;
                        combatSys_->SaveProgress(registry_);
                        AddLog("> 司令室をサイバー空間に変更しました！");
                    } else if (combatSys_->currentGold >= 500) {
                        combatSys_->currentGold -= 500;
                        combatSys_->themeOwned[1] = true;
                        combatSys_->currentTheme = 1;
                        combatSys_->SaveProgress(registry_);
                        AddLog("> 司令室をサイバー空間に変更しました！");
                    } else { AddLog("ゴールドが足りません！"); }
                }
            });
        }
        else if (n.name == "RoomAntique") {
            checkBtnClick(e, [&](){ 
                if (combatSys_) {
                    if (combatSys_->themeOwned[2]) {
                        combatSys_->currentTheme = 2;
                        combatSys_->SaveProgress(registry_);
                        AddLog("> 司令室をアンティーク調に変更しました！");
                    } else if (combatSys_->currentGold >= 1000) {
                        combatSys_->currentGold -= 1000;
                        combatSys_->themeOwned[2] = true;
                        combatSys_->currentTheme = 2;
                        combatSys_->SaveProgress(registry_);
                        AddLog("> 司令室をアンティーク調に変更しました！");
                    } else { AddLog("ゴールドが足りません！"); }
                }
            });
        }
        else if (n.name == "RoomGold") {
            checkBtnClick(e, [&](){ 
                if (combatSys_) {
                    if (combatSys_->themeOwned[3]) {
                        combatSys_->currentTheme = 3;
                        combatSys_->SaveProgress(registry_);
                        AddLog("> 司令室を黄金の聖域に変更しました！");
                    } else if (combatSys_->currentGold >= 5000) {
                        combatSys_->currentGold -= 5000;
                        combatSys_->themeOwned[3] = true;
                        combatSys_->currentTheme = 3;
                        combatSys_->SaveProgress(registry_);
                        AddLog("> 司令室を黄金の聖域に変更しました！");
                    } else { AddLog("ゴールドが足りません！"); }
                }
            });
        }
        else if (n.name == "UGCSelectChar") {
            checkBtnClick(e, [&](){ 
                ugcTargetCharIndex_ = (ugcTargetCharIndex_ + 1) % 5;
                std::string names[] = {"重装クロスボウ", "ロングボウ", "連射クロスボウ", "癒やしの矢", "支援枠"};
                registry_.get<UITextComponent>(e).text = names[ugcTargetCharIndex_];
            });
        }
        else if (n.name == "UGCFile") {
            checkBtnClick(e, [&](){ 
                wchar_t filename[MAX_PATH] = L"";
                OPENFILENAMEW ofn;
                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = dx_->GetHwnd();
                ofn.lpstrFilter = L"Image Files\0*.png;*.jpg;*.jpeg\0All Files\0*.*\0";
                ofn.lpstrFile = filename;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
                ofn.lpstrDefExt = L"png";

                if (GetOpenFileNameW(&ofn)) {
                    std::string utf8Path = Engine::PathUtils::ToUTF8(filename);
                    
                    std::wstring shortNameW = filename;
                    size_t slash = shortNameW.find_last_of(L"\\/");
                    if (slash != std::wstring::npos) shortNameW = shortNameW.substr(slash + 1);
                    std::string shortName = Engine::PathUtils::ToUTF8(shortNameW);

                    // 新しいパスをセーブデータに保存
                    auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                    if (!viewProg.empty()) {
                        auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                        prog.ugcTexturePaths[ugcTargetCharIndex_] = utf8Path;
                        if (combatSys_) combatSys_->SaveProgress(registry_);
                    }
                    
                    // 現在スポーンしているヒーローの画像を更新
                    auto heroView = registry_.view<DesktopHeroComponent, UIImageComponent>();
                    for (auto ent : heroView) {
                        auto& hero = heroView.get<DesktopHeroComponent>(ent);
                        if ((int)hero.role == ugcTargetCharIndex_) {
                            auto& img = heroView.get<UIImageComponent>(ent);
                            img.texturePath = utf8Path;
                            img.color = {1.0f, 1.0f, 1.0f, 1.0f}; // カスタム画像にはデフォルトの色付けをしない
                            if (renderer_) {
                                img.textureHandle = renderer_->LoadTexture2D(img.texturePath, false);
                            }
                        }
                    }
                    
                    auto tView = registry_.view<NameComponent, UITextComponent>();
                    for (auto te : tView) {
                        if (tView.get<NameComponent>(te).name == "UGCNothing") {
                            tView.get<UITextComponent>(te).text = shortName + " を適用";
                        }
                    }
                    
                    AddLog("> 画像を " + shortName + " に差し替えました");
                }
            });
        }
        
        else if (n.name == "GachaBtn") {
            checkBtnClick(e, [&](){
                auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                if (!viewProg.empty()) {
                    auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                    
                    if (prog.relicEncyclopedia.empty()) {
                        prog.relicEncyclopedia = {
                            {0, "フロッピーディスク", "保存容量1.44MB。 全体ATK+5%", false, 0.05f, 0.0f, 0.0f},
                            {1, "MS-DOS画面", "黒画面と緑の文字。 全体HP+5%", false, 0.0f, 0.05f, 0.0f},
                            {2, "ダイヤルアップ音", "接続時のあの音。 全体SPD+5%", false, 0.0f, 0.0f, 0.05f},
                            {3, "ブラウン管モニタ", "重くてデカい。 全体HP+10%", false, 0.0f, 0.10f, 0.0f},
                            {4, "IEショートカット", "もう開くことはない。 全体ATK+10%", false, 0.10f, 0.0f, 0.0f}
                        };
                    }

                    if (prog.dataFragments >= 10) {
                        std::vector<int> lockedIdx;
                        for (size_t i = 0; i < prog.relicEncyclopedia.size(); ++i) {
                            if (!prog.relicEncyclopedia[i].isUnlocked) lockedIdx.push_back((int)i);
                        }
                        
                        if (!lockedIdx.empty()) {
                            prog.dataFragments -= 10;
                            int r = lockedIdx[rand() % lockedIdx.size()];
                            prog.relicEncyclopedia[r].isUnlocked = true;
                            AddLog("> ガチャ: [" + prog.relicEncyclopedia[r].name + "] を獲得！");
                            if (combatSys_) combatSys_->SaveProgress(registry_);
                        } else {
                            AddLog("> ガチャ: 全ての遺物を獲得済みです！");
                        }
                    } else {
                        AddLog("データ断片が足りません。");
                    }
                }
            });
        }
        else if (n.name == "OpenChestBtn") {
            checkBtnClick(e, [&](){
                auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                if (!viewProg.empty()) {
                    auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                    if (prog.availableChests > 0) {
                        prog.availableChests--;
                        int type = rand() % 3;
                        prog.specialArrows[type]++;
                        std::string arrowNames[] = {"爆発の矢", "氷結の矢", "分裂の矢"};
                        AddLog("> 宝箱を開けた！ [" + arrowNames[type] + "] を獲得！");
                        if (combatSys_) combatSys_->SaveProgress(registry_);
                    } else {
                        AddLog("開けられる宝箱がありません。");
                    }
                }
            });
        }
        else if (n.name == "RelicEncyclopediaBtn") {
            checkBtnClick(e, [&](){ requestShowRelicPopup = true; });
        }
        else if (n.name == "EquipEncyclopediaBtn") {
            checkBtnClick(e, [&](){ requestShowEquipPopupIdx = -1; currentEquipRoleIdx_ = -1; equipPopupScrollY_ = 0.0f; });
        }
        else if (n.name == "HardResetBtn") {
            checkBtnClick(e, [&](){
                if (combatSys_) {
                    std::remove("party_progress.json");
                    fadeState_ = FadeState::FadingOut;
                    fadeResetType_ = ResetType::HardReset;
                    fadeTimer_ = 0.0f;
                    context_.isPlaying = false;
                }
            });
        }
        else if (n.name.find("R_UpBtn") == 0) {
            int idx = n.name[7] - '0';
            std::string heroNames[] = {"重装クロスボウ", "ロングボウ", "連射クロスボウ", "癒やしの矢"};
            checkBtnClick(e, [&, idx, heroNames](){ 
                if (combatSys_ && idx >= 0 && idx < 4) {
                    double cost = combatSys_->GetLevelUpCost(idx);
                    if (combatSys_->currentGold >= cost) {
                        combatSys_->currentGold -= cost;
                        combatSys_->heroLevels[idx]++;
                        combatSys_->SaveProgress(registry_);
                        AddLog("> " + heroNames[idx] + "を強化しました！ (Lv." + std::to_string(combatSys_->heroLevels[idx]) + ")");
                    } else {
                        AddLog("ゴールドが足りません！ (必要: " + DesktopCombatSystem::FormatNum(cost) + "G)");
                    }
                }
            });
        }
        else if (n.name.find("R_Equip") == 0) {
            int idx = n.name[7] - '0';
            checkBtnClick(e, [&, idx](){ requestShowEquipPopupIdx = idx; currentEquipRoleIdx_ = idx; equipPopupScrollY_ = 0.0f; });
        }
        else if (n.name == "ClosePopupBtn") {
            checkBtnClick(e, [&](){ requestClosePopup = true; });
        }
        else if (n.name.find("EquipBtn_") == 0) {
            checkBtnClick(e, [&, n_name = n.name](){
                std::string s = n_name.substr(9);
                auto pos = s.find('_');
                if (pos != std::string::npos) {
                    requestEquipId = std::stoi(s.substr(0, pos));
                    requestEquipRoleIdx = std::stoi(s.substr(pos + 1));
                }
            });
        }
        else if (n.name.find("SelectSlotBtn_") == 0) {
            checkBtnClick(e, [&, n_name = n.name](){
                equipPopupActiveSlot_ = std::stoi(n_name.substr(14));
                equipPopupScrollY_ = 0.0f; // 部位を変えたらスクロールリセット
                requestShowEquipPopupIdx = currentEquipRoleIdx_;
            });
        }
        else if (n.name.find("FilterRar_") == 0) {
            checkBtnClick(e, [&, n_name = n.name](){
                equipPopupFilterRarity_ = std::stoi(n_name.substr(10));
                equipPopupScrollY_ = 0.0f;
                requestShowEquipPopupIdx = currentEquipRoleIdx_;
            });
        }
        else if (n.name.find("UnequipBtn_") == 0) {
            checkBtnClick(e, [&, n_name = n.name](){
                int slotIdx = std::stoi(n_name.substr(11));
                auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                if (!viewProg.empty()) {
                    auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                    prog.equippedIds[currentEquipRoleIdx_ * 5 + slotIdx] = -1; // 外す
                    if (combatSys_) combatSys_->SaveProgress(registry_);
                    requestShowEquipPopupIdx = currentEquipRoleIdx_;
                }
            });
        }
        else if (n.name == "PrestigeBtn") {
            checkBtnClick(e, [&](){ requestShowPrestigePopup = true; });
        }
        else if (n.name == "PrestigeSkillAtkBtn") {
            checkBtnClick(e, [&](){
                if (combatSys_ && combatSys_->prestigePoints > 0) {
                    combatSys_->prestigePoints--;
                    combatSys_->skillLevelAtk++;
                    combatSys_->SaveProgress(registry_);
                    requestShowPrestigePopup = true; 
                }
            });
        }
        else if (n.name == "PrestigeSkillHpBtn") {
            checkBtnClick(e, [&](){
                if (combatSys_ && combatSys_->prestigePoints > 0) {
                    combatSys_->prestigePoints--;
                    combatSys_->skillLevelHp++;
                    combatSys_->SaveProgress(registry_);
                    requestShowPrestigePopup = true; 
                }
            });
        }
        else if (n.name == "PrestigeSkillSpdBtn") {
            checkBtnClick(e, [&](){
                if (combatSys_ && combatSys_->prestigePoints > 0) {
                    combatSys_->prestigePoints--;
                    combatSys_->skillLevelSpd++;
                    combatSys_->SaveProgress(registry_);
                    requestShowPrestigePopup = true; 
                }
            });
        }
        else if (n.name == "PrestigeSkillGoldBtn") {
            checkBtnClick(e, [&](){
                if (combatSys_ && combatSys_->prestigePoints > 0) {
                    combatSys_->prestigePoints--;
                    combatSys_->skillLevelGold++;
                    combatSys_->SaveProgress(registry_);
                    requestShowPrestigePopup = true; 
                }
            });
        }
        else if (n.name == "PrestigeResetRunBtn") {
            checkBtnClick(e, [&](){
                if (combatSys_) {
                    int maxHeroLv = 1;
                    for (int i=0; i<4; i++) { if (combatSys_->heroLevels[i] > maxHeroLv) maxHeroLv = combatSys_->heroLevels[i]; }
                    if (maxHeroLv >= 20) {
                        requestClosePopup = true;
                        fadeState_ = FadeState::FadingOut;
                        fadeResetType_ = ResetType::NormalPrestige;
                        fadeTimer_ = 0.0f;
                        context_.isPlaying = false; // ポーズ
                    } else {
                        AddLog("転生するには、いずれかのキャラがLv.20以上必要です。");
                    }
                }
            });
        }
        else if (n.name == "BtnGameOverPrestige") {
            checkBtnClick(e, [&](){
                if (combatSys_) {
                    requestClosePopup = true;
                    fadeState_ = FadeState::FadingOut;
                    fadeResetType_ = ResetType::GameOverPrestige;
                    fadeTimer_ = 0.0f;
                    context_.isPlaying = false;
                }
            });
        }
        else if (n.name == "BtnGameOverRetry") {
            checkBtnClick(e, [&](){
                if (combatSys_) {
                    requestClosePopup = true;
                    fadeState_ = FadeState::FadingOut;
                    fadeResetType_ = ResetType::GameOverRetry;
                    fadeTimer_ = 0.0f;
                    context_.isPlaying = false;
                }
            });
        }

        // UIテキストの動的更新 (ゴールドやステータスなど)
        if (combatSys_) {
            if (n.name == "GachaHeader") {
                auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                if (!viewProg.empty()) {
                    auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                    registry_.get<UITextComponent>(e).text = "謎のデータ断片: " + std::to_string(prog.dataFragments) + "個";
                }
            } else if (n.name == "CommandHeaderEV") {
                auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                if (!viewProg.empty()) {
                    auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                    registry_.get<UITextComponent>(e).text = std::to_string(prog.hiredRoles.size()) + " / " + std::to_string(prog.maxDeploymentLimit);
                }
            } else if (n.name == "ChestHeader") {
                auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                if (!viewProg.empty()) {
                    auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                    registry_.get<UITextComponent>(e).text = "宝箱: " + std::to_string(prog.availableChests) + "個";
                }
            } else if (n.name == "ArrowInfo") {
                auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                if (!viewProg.empty()) {
                    auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                    registry_.get<UITextComponent>(e).text = "爆:" + std::to_string(prog.specialArrows[0]) + 
                                                             " 氷:" + std::to_string(prog.specialArrows[1]) + 
                                                             " 分:" + std::to_string(prog.specialArrows[2]);
                }
            } else if (n.name == "EnergyHeader") {
                auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                int maxE = 50;
                if (!viewProg.empty()) {
                    auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                    maxE = prog.chestEnergyTarget;
                }
                if (workSys_) {
                    registry_.get<UITextComponent>(e).text = "エネルギー: " + std::to_string((int)workSys_->GetEnergy()) + " / " + std::to_string(maxE);
                }
            } else if (n.name == "Gold") {
                registry_.get<UITextComponent>(e).text = "STAGE " + std::to_string(combatSys_->currentStage) + " | 所持ゴールド: " + DesktopCombatSystem::FormatNum(combatSys_->currentGold) + " G";
            } else if (n.name == "CastleHPValue") {
                auto viewCastle = registry_.view<DesktopCastleComponent>();
                if (!viewCastle.empty()) {
                    auto& castle = registry_.get<DesktopCastleComponent>(viewCastle.front());
                    int displayHp = (std::max)(0, (int)castle.hp);
                    registry_.get<UITextComponent>(e).text = std::to_string(displayHp) + " / " + std::to_string((int)castle.maxHp);
                    
                    if (castle.hp <= 0.0f && context_.isPlaying) {
                        context_.isPlaying = false; // ポーズ
                        requestShowGameOverPopup = true;
                    }
                }
            } else if (n.name == "CastleHPBarFG") {
                auto viewCastle = registry_.view<DesktopCastleComponent>();
                if (!viewCastle.empty()) {
                    auto& castle = registry_.get<DesktopCastleComponent>(viewCastle.front());
                    auto& rect = registry_.get<RectTransformComponent>(e);
                    float ratio = (std::max)(0.0f, castle.hp) / castle.maxHp;
                    rect.size.x = 280.0f * ratio; // 枠の幅が280なので280に乗算
                }
            } else if (n.name == "HireLimitBarFG") {
                auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                if (!viewProg.empty()) {
                    auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                    auto& rect = registry_.get<RectTransformComponent>(e);
                    float ratio = 0.0f;
                    if (prog.maxDeploymentLimit > 0) {
                        ratio = (float)prog.hiredRoles.size() / (float)prog.maxDeploymentLimit;
                    }
                    rect.size.x = 280.0f * ratio; // 枠の幅が280
                }
            } else if (n.name == "Stat1V") {
                registry_.get<UITextComponent>(e).text = DesktopCombatSystem::FormatNum(combatSys_->totalKills) + " 体";
            } else if (n.name == "Stat2V") {
                registry_.get<UITextComponent>(e).text = DesktopCombatSystem::FormatNum(combatSys_->totalDamage) + " pts";
            } else if (n.name == "Stat3V") {
                registry_.get<UITextComponent>(e).text = DesktopCombatSystem::FormatNum(combatSys_->totalGoldEarned) + " G";
            } else if (n.name == "Stat4V") {
                registry_.get<UITextComponent>(e).text = DesktopCombatSystem::FormatNum(combatSys_->totalItems) + " 個";
            } else if (n.name == "BuyTank") {
                int lv = combatSys_->heroLevels[0];
                float hp, maxHp, atk, spd;
                combatSys_->GetHeroStats(registry_, 0, hp, maxHp, atk, spd);
                registry_.get<UITextComponent>(e).text = "重装クロスボウ Lv." + std::to_string(lv) + "    ATK:" + DesktopCombatSystem::FormatNum(atk) + "/HP:" + DesktopCombatSystem::FormatNum(maxHp);
            } else if (n.name == "BuySniper") {
                int lv = combatSys_->heroLevels[1];
                float hp, maxHp, atk, spd;
                combatSys_->GetHeroStats(registry_, 1, hp, maxHp, atk, spd);
                registry_.get<UITextComponent>(e).text = "ロングボウ Lv." + std::to_string(lv) + "      ATK:" + DesktopCombatSystem::FormatNum(atk) + "/HP:" + DesktopCombatSystem::FormatNum(maxHp);
            } else if (n.name == "BuyScout") {
                int lv = combatSys_->heroLevels[2];
                float hp, maxHp, atk, spd;
                combatSys_->GetHeroStats(registry_, 2, hp, maxHp, atk, spd);
                registry_.get<UITextComponent>(e).text = "連射クロスボウ Lv." + std::to_string(lv) + "    ATK:" + DesktopCombatSystem::FormatNum(atk) + "/HP:" + DesktopCombatSystem::FormatNum(maxHp);
            } else if (n.name == "BuyEngineer") {
                int lv = combatSys_->heroLevels[3];
                float hp, maxHp, atk, spd;
                combatSys_->GetHeroStats(registry_, 3, hp, maxHp, atk, spd);
                registry_.get<UITextComponent>(e).text = "癒やしの矢 Lv." + std::to_string(lv) + "      ATK:" + DesktopCombatSystem::FormatNum(atk) + "/HP:" + DesktopCombatSystem::FormatNum(maxHp);
            } else if (n.name == "Btn1") {
                registry_.get<UITextComponent>(e).text = DesktopCombatSystem::FormatNum(combatSys_->GetLevelUpCost(0)) + " G";
            } else if (n.name == "Btn2") {
                registry_.get<UITextComponent>(e).text = DesktopCombatSystem::FormatNum(combatSys_->GetLevelUpCost(1)) + " G";
            } else if (n.name == "Btn3") {
                registry_.get<UITextComponent>(e).text = DesktopCombatSystem::FormatNum(combatSys_->GetLevelUpCost(2)) + " G";
            } else if (n.name == "Btn4") {
                registry_.get<UITextComponent>(e).text = DesktopCombatSystem::FormatNum(combatSys_->GetLevelUpCost(3)) + " G";
            } else if (n.name.find("R_Lv") == 0) {
                int idx = n.name[4] - '0';
                if (idx >= 0 && idx < 4) registry_.get<UITextComponent>(e).text = std::to_string(combatSys_->heroLevels[idx]);
                else if (idx == 4) registry_.get<UITextComponent>(e).text = "1";
            } else if (n.name.find("R_HP") == 0) {
                int idx = n.name[4] - '0';
                if (idx >= 0 && idx < 5) {
                    float hp, maxHp, atk, spd;
                    float baseMaxHp, baseAtk, baseSpd;
                    combatSys_->GetHeroStats(registry_, idx, hp, maxHp, atk, spd, &baseMaxHp, &baseAtk, &baseSpd);
                    std::string diff = "";
                    if (maxHp > baseMaxHp) diff = " (+" + DesktopCombatSystem::FormatNum(maxHp - baseMaxHp) + ")";
                    registry_.get<UITextComponent>(e).text = DesktopCombatSystem::FormatNum(hp) + "/" + DesktopCombatSystem::FormatNum(baseMaxHp) + diff;
                }
            } else if (n.name.find("R_ATK") == 0) {
                int idx = n.name[5] - '0';
                if (idx >= 0 && idx < 5) {
                    float hp, maxHp, atk, spd;
                    float baseMaxHp, baseAtk, baseSpd;
                    combatSys_->GetHeroStats(registry_, idx, hp, maxHp, atk, spd, &baseMaxHp, &baseAtk, &baseSpd);
                    std::string diff = "";
                    if (atk > baseAtk) diff = " (+" + DesktopCombatSystem::FormatNum(atk - baseAtk) + ")";
                    registry_.get<UITextComponent>(e).text = DesktopCombatSystem::FormatNum(baseAtk) + diff;
                }
            } else if (n.name.find("R_SPD") == 0) {
                int idx = n.name[5] - '0';
                if (idx >= 0 && idx < 5) {
                    float hp, maxHp, atk, spd;
                    float baseMaxHp, baseAtk, baseSpd;
                    combatSys_->GetHeroStats(registry_, idx, hp, maxHp, atk, spd, &baseMaxHp, &baseAtk, &baseSpd);
                    char buf[32]; snprintf(buf, sizeof(buf), "%.1f", baseSpd);
                    std::string diff = "";
                    if (spd > baseSpd) {
                        char dbuf[32]; snprintf(dbuf, sizeof(dbuf), " (+%.1f)", spd - baseSpd);
                        diff = dbuf;
                    }
                    registry_.get<UITextComponent>(e).text = std::string(buf) + diff;
                }
            } else if (n.name == "RoomNormal") {
                if (combatSys_->currentTheme == 0) {
                    registry_.get<UITextComponent>(e).text = "標準チャンバー                  [適用中]";
                    registry_.get<UIButtonComponent>(e).normalColor = {0.7f, 1.0f, 0.7f, 1.0f};
                } else {
                    registry_.get<UITextComponent>(e).text = "標準チャンバー                  [所有済み]";
                    registry_.get<UIButtonComponent>(e).normalColor = {0.98f, 0.98f, 0.98f, 1.0f};
                }
            } else if (n.name == "RoomCyber") {
                if (combatSys_->currentTheme == 1) {
                    registry_.get<UITextComponent>(e).text = "サイバー空間                      [適用中]";
                    registry_.get<UIButtonComponent>(e).normalColor = {0.7f, 1.0f, 0.7f, 1.0f};
                } else if (combatSys_->themeOwned[1]) {
                    registry_.get<UITextComponent>(e).text = "サイバー空間                      [所有済み]";
                    registry_.get<UIButtonComponent>(e).normalColor = {0.98f, 0.98f, 0.98f, 1.0f};
                } else {
                    registry_.get<UITextComponent>(e).text = "サイバー空間                      500 G";
                    registry_.get<UIButtonComponent>(e).normalColor = {0.98f, 0.98f, 0.98f, 1.0f};
                }
            } else if (n.name == "RoomAntique") {
                if (combatSys_->currentTheme == 2) {
                    registry_.get<UITextComponent>(e).text = "アンティーク調                  [適用中]";
                    registry_.get<UIButtonComponent>(e).normalColor = {0.7f, 1.0f, 0.7f, 1.0f};
                } else if (combatSys_->themeOwned[2]) {
                    registry_.get<UITextComponent>(e).text = "アンティーク調                  [所有済み]";
                    registry_.get<UIButtonComponent>(e).normalColor = {0.98f, 0.98f, 0.98f, 1.0f};
                } else {
                    registry_.get<UITextComponent>(e).text = "アンティーク調                  1000 G";
                    registry_.get<UIButtonComponent>(e).normalColor = {0.98f, 0.98f, 0.98f, 1.0f};
                }
            } else if (n.name == "RoomGold") {
                if (combatSys_->currentTheme == 3) {
                    registry_.get<UITextComponent>(e).text = "黄金の聖域                        [適用中]";
                    registry_.get<UIButtonComponent>(e).normalColor = {0.7f, 1.0f, 0.7f, 1.0f};
                } else if (combatSys_->themeOwned[3]) {
                    registry_.get<UITextComponent>(e).text = "黄金の聖域                        [所有済み]";
                    registry_.get<UIButtonComponent>(e).normalColor = {0.98f, 0.98f, 0.98f, 1.0f};
                } else {
                    registry_.get<UITextComponent>(e).text = "黄金の聖域                        5000 G";
                    registry_.get<UIButtonComponent>(e).normalColor = {0.98f, 0.98f, 0.98f, 1.0f};
                }
            } else if (n.name == "CommandBG" || n.name == "StatPanelBG") {
                if (combatSys_->currentTheme == 1) registry_.get<UIImageComponent>(e).color = {0.8f, 0.9f, 0.95f, 1.0f}; // Cyber
                else if (combatSys_->currentTheme == 2) registry_.get<UIImageComponent>(e).color = {0.95f, 0.9f, 0.85f, 1.0f}; // Antique
                else if (combatSys_->currentTheme == 3) registry_.get<UIImageComponent>(e).color = {1.0f, 0.95f, 0.7f, 1.0f}; // Gold
                else registry_.get<UIImageComponent>(e).color = {0.95f, 0.96f, 0.98f, 1.0f}; // Normal
            } else if (n.name == "ChamberBG") {
                if (combatSys_->currentTheme == 1) registry_.get<UIImageComponent>(e).color = {0.05f, 0.15f, 0.2f, 1.0f}; // Cyber
                else if (combatSys_->currentTheme == 2) registry_.get<UIImageComponent>(e).color = {0.2f, 0.15f, 0.1f, 1.0f}; // Antique
                else if (combatSys_->currentTheme == 3) registry_.get<UIImageComponent>(e).color = {0.25f, 0.2f, 0.05f, 1.0f}; // Gold
                else registry_.get<UIImageComponent>(e).color = {0.05f, 0.05f, 0.1f, 1.0f}; // Normal
            } else if (n.name == "ChamberBorderTop" || n.name == "ChamberBorderBottom" || n.name == "ChamberBorderLeft" || n.name == "ChamberBorderRight") {
                if (combatSys_->currentTheme == 1) registry_.get<UIImageComponent>(e).color = {0.0f, 1.0f, 0.8f, 1.0f}; // Cyber
                else if (combatSys_->currentTheme == 2) registry_.get<UIImageComponent>(e).color = {0.6f, 0.4f, 0.2f, 1.0f}; // Antique
                else if (combatSys_->currentTheme == 3) registry_.get<UIImageComponent>(e).color = {1.0f, 0.8f, 0.0f, 1.0f}; // Gold
                else registry_.get<UIImageComponent>(e).color = {0.2f, 0.7f, 1.0f, 1.0f}; // Normal
            }
        }
    }
    
    if (requestClosePopup) {
        ClosePopup();
    }
    if (requestShowRelicPopup) {
        ShowRelicPopup();
    }
    if (requestShowPrestigePopup) {
        ShowPrestigePopup();
    }
    if (requestShowGameOverPopup) {
        ShowGameOverPopup();
    }
    if (requestShowEquipPopupIdx != -1) {
        ShowEquipPopup(requestShowEquipPopupIdx);
    }
    if (requestEquipId != -1 && requestEquipRoleIdx != -1) {
        auto viewProg = registry_.view<DesktopPartyProgressComponent>();
        if (!viewProg.empty()) {
            auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
            prog.equippedIds[requestEquipRoleIdx * 5 + equipPopupActiveSlot_] = requestEquipId;
            AddLog("> 装備を変更しました！");
            if (combatSys_) combatSys_->SaveProgress(registry_);
            ShowEquipPopup(requestEquipRoleIdx); // 再描画して反映
        }
    }
    
    // --- 暗転とリセットの処理 ---
    if (fadeState_ != FadeState::None) {
        fadeTimer_ += dt_;
        
        if (!registry_.valid(fadeOverlay_)) {
            fadeOverlay_ = registry_.create();
            registry_.emplace<NameComponent>(fadeOverlay_, "FadeOverlay");
            auto& t = registry_.emplace<RectTransformComponent>(fadeOverlay_);
            t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
            t.pos = {0, 0}; t.size = {1920, 1080};
            auto& img = registry_.emplace<UIImageComponent>(fadeOverlay_);
            img.texturePath = "Resources/Textures/white1x1.png";
            if (renderer_) img.textureHandle = renderer_->LoadTexture2D(img.texturePath);
            img.color = {0,0,0,0};
            img.layer = 100; // 最前面
        }
        
        auto& img = registry_.get<UIImageComponent>(fadeOverlay_);
        
        if (fadeState_ == FadeState::FadingOut) {
            float alpha = std::min(fadeTimer_ / 1.0f, 1.0f);
            img.color.w = alpha;
            
            if (fadeTimer_ >= 1.2f) { // 暗転完了して少し待つ
                // リセット処理
                if (combatSys_) {
                    if (fadeResetType_ == ResetType::NormalPrestige || fadeResetType_ == ResetType::GameOverPrestige) {
                        combatSys_->prestigeCount++;
                        combatSys_->prestigePoints++;
                        for (int i=0; i<4; i++) combatSys_->heroLevels[i] = 1;
                        combatSys_->currentGold = 300.0;
                        combatSys_->currentStage = 1;
                        combatSys_->enemiesKilledInStage = 0;
                        combatSys_->enemiesSpawnedInStage = 0;
                        
                        auto ev = registry_.view<DesktopEnemyComponent>();
                        for (auto ee : ev) {
                            auto& enemy = registry_.get<DesktopEnemyComponent>(ee);
                            if (registry_.valid(enemy.hpBarBg)) registry_.destroy(enemy.hpBarBg);
                            if (registry_.valid(enemy.hpBarFg)) registry_.destroy(enemy.hpBarFg);
                            registry_.destroy(ee);
                        }
                        auto pv = registry_.view<DesktopProjectileComponent>();
                        for (auto pe : pv) registry_.destroy(pe);
                        
                        auto hv = registry_.view<DesktopHeroComponent>();
                        for (auto he : hv) {
                            auto& hero = registry_.get<DesktopHeroComponent>(he);
                            if (registry_.valid(hero.hpBarBg)) registry_.destroy(hero.hpBarBg);
                            if (registry_.valid(hero.hpBarFg)) registry_.destroy(hero.hpBarFg);
                            if (registry_.valid(hero.weaponEntity)) registry_.destroy(hero.weaponEntity);
                            registry_.destroy(he); // ヒーローを破棄して再雇用状態をリセット
                        }
                        
                        auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                        if (!viewProg.empty()) {
                            auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                            prog.hiredRoles = {1}; // 転生時はロングボウのみにリセット
                        }
                        
                        auto viewCastle = registry_.view<DesktopCastleComponent>();
                        if (!viewCastle.empty()) {
                            auto& castle = registry_.get<DesktopCastleComponent>(viewCastle.front());
                            castle.hp = castle.maxHp;
                        }
                        
                        combatSys_->SaveProgress(registry_);
                        
                        if (fadeResetType_ == ResetType::NormalPrestige) {
                            AddLog("> [転生] レベルが初期化され、スキルポイントを1獲得！");
                        } else {
                            AddLog("> [ゲームオーバー] 全てを失い、システムを再起動(転生)しました。");
                        }
                    } 
                    else if (fadeResetType_ == ResetType::GameOverRetry) {
                        combatSys_->enemiesKilledInStage = 0;
                        combatSys_->enemiesSpawnedInStage = 0;
                        auto viewCastle = registry_.view<DesktopCastleComponent>();
                        if (!viewCastle.empty()) {
                            auto& castle = registry_.get<DesktopCastleComponent>(viewCastle.front());
                            castle.hp = castle.maxHp;
                        }
                        auto hv = registry_.view<DesktopHeroComponent>();
                        for (auto he : hv) {
                            auto& hero = registry_.get<DesktopHeroComponent>(he);
                            hero.hp = hero.maxHp;
                            if (registry_.all_of<UIImageComponent>(he)) {
                                registry_.get<UIImageComponent>(he).color = {1.0f, 1.0f, 1.0f, 1.0f};
                            }
                        }
                        
                        auto ev = registry_.view<DesktopEnemyComponent>();
                        for (auto ee : ev) {
                            auto& enemy = registry_.get<DesktopEnemyComponent>(ee);
                            if (registry_.valid(enemy.hpBarBg)) registry_.destroy(enemy.hpBarBg);
                            if (registry_.valid(enemy.hpBarFg)) registry_.destroy(enemy.hpBarFg);
                            registry_.destroy(ee);
                        }
                        auto pv = registry_.view<DesktopProjectileComponent>();
                        for (auto pe : pv) registry_.destroy(pe);
                        
                        AddLog("> ステージ " + std::to_string(combatSys_->currentStage) + " をリトライします。");
                    }
                    else if (fadeResetType_ == ResetType::HardReset) {
                        std::remove("party_progress.json");
                        
                        combatSys_->enemiesKilledInStage = 0;
                        combatSys_->enemiesSpawnedInStage = 0;
                        combatSys_->currentStage = 1;
                        combatSys_->totalGoldEarned = 300.0;
                        combatSys_->currentGold = 300.0;
                        combatSys_->totalKills = 0;
                        combatSys_->prestigeCount = 0;
                        combatSys_->prestigePoints = 0;
                        combatSys_->skillLevelAtk = 0;
                        combatSys_->skillLevelHp = 0;
                        combatSys_->skillLevelSpd = 0;
                        combatSys_->skillLevelGold = 0;
                        for (int i=0; i<4; i++) combatSys_->themeOwned[i] = (i==0);
                        combatSys_->currentTheme = 0;
                        for (int i=0; i<4; i++) {
                            combatSys_->heroLevels[i] = 1;
                        }
                        
                        auto viewCastle = registry_.view<DesktopCastleComponent>();
                        if (!viewCastle.empty()) {
                            auto& castle = registry_.get<DesktopCastleComponent>(viewCastle.front());
                            castle.hp = castle.maxHp;
                        }
                        
                        auto hv = registry_.view<DesktopHeroComponent>();
                        for (auto he : hv) {
                            auto& hero = registry_.get<DesktopHeroComponent>(he);
                            if (registry_.valid(hero.hpBarBg)) registry_.destroy(hero.hpBarBg);
                            if (registry_.valid(hero.hpBarFg)) registry_.destroy(hero.hpBarFg);
                            if (registry_.valid(hero.weaponEntity)) registry_.destroy(hero.weaponEntity);
                            registry_.destroy(he);
                        }
                        
                        auto ev = registry_.view<DesktopEnemyComponent>();
                        for (auto ee : ev) {
                            auto& enemy = registry_.get<DesktopEnemyComponent>(ee);
                            if (registry_.valid(enemy.hpBarBg)) registry_.destroy(enemy.hpBarBg);
                            if (registry_.valid(enemy.hpBarFg)) registry_.destroy(enemy.hpBarFg);
                            registry_.destroy(ee);
                        }
                        
                        auto pv = registry_.view<DesktopProjectileComponent>();
                        for (auto pe : pv) registry_.destroy(pe);
                        
                        auto viewProg = registry_.view<DesktopPartyProgressComponent>();
                        if (!viewProg.empty()) {
                            auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
                            prog = DesktopPartyProgressComponent();
                            prog.hiredRoles = {1}; // ロングボウのみ
                        }
                        
                        combatSys_->SaveProgress(registry_);
                        AddLog("> [ハードリセット] 全てのデータを初期化しました。");
                    }
                }
                
                fadeState_ = FadeState::FadingIn;
                fadeTimer_ = 0.0f;
            }
        }
        else if (fadeState_ == FadeState::FadingIn) {
            float alpha = std::max(1.0f - (fadeTimer_ / 1.0f), 0.0f);
            img.color.w = alpha;
            
            if (fadeTimer_ >= 1.0f) {
                fadeState_ = FadeState::None;
                if (registry_.valid(fadeOverlay_)) {
                    registry_.destroy(fadeOverlay_);
                    fadeOverlay_ = entt::null;
                }
                context_.isPlaying = true; // 再開
                if (fadeResetType_ == ResetType::NormalPrestige) {
                    requestShowPrestigePopup = true; // 転生スキルツリーを再度開く
                }
                fadeResetType_ = ResetType::None;
            }
        }
    }
    
    // コア・フィーバーの物理挙動
    if (currentTab_ == TabType::Room && registry_.valid(coreEntity_) && combatSys_) {
        float chamberX = 1320.0f;
        float chamberY = 180.0f + 80.0f; // UI_OFFSET_Y を加算して枠の座標を合わせる
        float chamberW = 530.0f;
        float chamberH = 200.0f;
        float coreW = 64.0f;
        float coreH = 48.0f;
        
        timeSinceFever_ += dt_;
        
        auto& t = registry_.get<RectTransformComponent>(coreEntity_);
        t.pos.x += coreVel_.x * dt_;
        t.pos.y += coreVel_.y * dt_;
        
        bool hitX = false;
        bool hitY = false;
        if (t.pos.x <= chamberX) { t.pos.x = chamberX; coreVel_.x *= -1; hitX = true; }
        if (t.pos.x + coreW >= chamberX + chamberW) { t.pos.x = chamberX + chamberW - coreW; coreVel_.x *= -1; hitX = true; }
        if (t.pos.y <= chamberY) { t.pos.y = chamberY; coreVel_.y *= -1; hitY = true; }
        if (t.pos.y + coreH >= chamberY + chamberH) { t.pos.y = chamberY + chamberH - coreH; coreVel_.y *= -1; hitY = true; }
        
        // 四隅ジャストミート！
        if (hitX && hitY) {
            combatSys_->TriggerCoreFever();
            AddLog(">> コア・フィーバー発動！ 全能力アップ！ <<");
            timeSinceFever_ = 0.0f; // リセット
            
            // 四隅ヒット演出
            auto& img = registry_.get<UIImageComponent>(coreEntity_);
            img.color = {1.0f, 0.0f, 0.0f, 1.0f}; // 赤く発光
        }
        else if (hitX || hitY) {
            auto& img = registry_.get<UIImageComponent>(coreEntity_);
            img.color = {0.5f + (rand()%50)/100.0f, 0.5f + (rand()%50)/100.0f, 1.0f, 1.0f};
            
            // 【DVDスクリーンセーバーチート】
            // 長い間(15秒)フィーバーが起きていない場合、次の反射で確実に角に当たるように速度を微調整する
            if (timeSinceFever_ > 15.0f) {
                float targetX = (coreVel_.x > 0) ? (chamberX + chamberW - coreW) : chamberX;
                float targetY = (coreVel_.y > 0) ? (chamberY + chamberH - coreH) : chamberY;
                float dx = targetX - t.pos.x;
                float dy = targetY - t.pos.y;
                
                float originalSpeed = std::sqrt(coreVel_.x * coreVel_.x + coreVel_.y * coreVel_.y);
                
                if (hitX && std::abs(dx) > 0.1f) {
                    coreVel_.y = dy * (coreVel_.x / dx);
                } else if (hitY && std::abs(dy) > 0.1f) {
                    coreVel_.x = dx * (coreVel_.y / dy);
                }
                
                float currentSpeed = std::sqrt(coreVel_.x * coreVel_.x + coreVel_.y * coreVel_.y);
                if (currentSpeed > 0.001f) {
                    coreVel_.x = (coreVel_.x / currentSpeed) * originalSpeed;
                    coreVel_.y = (coreVel_.y / currentSpeed) * originalSpeed;
                }
            }
        }
    }
    
    // 格納ボタンクリック判定
    checkBtnClick(stowButton_, [&](){ SwitchAppMode(AppMode::TaskbarBottom); });
    checkBtnClick(unstowBtn_, [&](){ SwitchAppMode(AppMode::Fullscreen); });

    // 格納中のタスクバー位置・サイズの動的更新（OSのタスクバー変化に追従）
    if (appMode_ == AppMode::TaskbarBottom) {
        RECT appBarRect = Engine::WindowDX::GetWorkArea();
        float bx = static_cast<float>(appBarRect.right) - 120.0f;
        float by = static_cast<float>(appBarRect.bottom) - 48.0f;

        if (registry_.valid(unstowBtn_)) {
            registry_.get<RectTransformComponent>(unstowBtn_).pos.x = bx;
            registry_.get<RectTransformComponent>(unstowBtn_).pos.y = by;
        }
    }
}

void MainScene::Draw() {
    // Draw Systems
    for (auto& sys : systems_) {
        sys->Draw(registry_, context_);
    }

    // ★追加: MeshRendererComponentを持つエンティティを描画する処理
    if (renderer_) {
        auto view = registry_.view<TransformComponent, MeshRendererComponent>();
        for (auto entity : view) {
            auto& t = view.get<TransformComponent>(entity);
            auto& mr = view.get<MeshRendererComponent>(entity);
            
            // ハンドルが未ロードならロードする
            if (mr.modelHandle == 0 && !mr.modelPath.empty()) {
                mr.modelHandle = renderer_->LoadObjMesh(mr.modelPath);
            }
            if (mr.textureHandle == 0 && !mr.texturePath.empty()) {
                mr.textureHandle = renderer_->LoadTexture2D(mr.texturePath);
            }
            
            renderer_->DrawMeshInstanced(mr.modelHandle, mr.textureHandle, t.ToMatrix(), {mr.color.x, mr.color.y, mr.color.z, mr.color.w}, mr.shaderName);
        }
    }
}

void MainScene::DrawUI() {
    // DrawUI Systems
    for (auto& sys : systems_) {
        sys->DrawUI(registry_, context_);
    }
}

entt::entity MainScene::CreateEntity(const std::string& /*name*/) {
    auto e = registry_.create();
    return e;
}

void MainScene::DestroyObject(uint32_t id) {
    entt::entity e = static_cast<entt::entity>(id);
    if (registry_.valid(e)) {
        registry_.destroy(e);
    }
}

void MainScene::SwitchTab(TabType newTab) {
    currentTab_ = newTab;

    // Update tab button visuals
    auto setTabBtn = [&](entt::entity e, bool active) {
        if (!registry_.valid(e)) return;
        auto& btn = registry_.get<UIButtonComponent>(e);
        auto& text = registry_.get<UITextComponent>(e);
        if (active) {
            btn.normalColor = {0.9f, 0.9f, 1.0f, 1.0f}; // Selected BG
            text.color = {0.0f, 0.0f, 0.0f, 1.0f};      // Selected Text
        } else {
            btn.normalColor = {0.95f, 0.95f, 0.95f, 1.0f}; // Unselected BG
            text.color = {0.4f, 0.4f, 0.4f, 1.0f};         // Unselected Text
        }
    };
    
    setTabBtn(tabCommandBtn_, currentTab_ == TabType::Command);
    setTabBtn(tabRoomBtn_,    currentTab_ == TabType::Room);
    setTabBtn(tabUpgradeBtn_, currentTab_ == TabType::Upgrade);
    setTabBtn(tabSystemBtn_,  currentTab_ == TabType::System);
    setTabBtn(tabStatusBtn_,  currentTab_ == TabType::Status);

    // Move active line
    if (registry_.valid(tabActiveLine_)) {
        auto& t = registry_.get<RectTransformComponent>(tabActiveLine_);
        float offset = 0.0f;
        if (currentTab_ == TabType::Command) t.pos.x = 1320 + offset;
        else if (currentTab_ == TabType::Room) t.pos.x = 1430 + offset;
        else if (currentTab_ == TabType::Upgrade) t.pos.x = 1540 + offset;
        else if (currentTab_ == TabType::System) t.pos.x = 1650 + offset;
        else if (currentTab_ == TabType::Status) t.pos.x = 1760 + offset;
    }

    // Toggle entities
    auto setEntitiesActive = [&](const std::vector<entt::entity>& entities, bool active) {
        for (auto e : entities) {
            if (registry_.valid(e)) {
                if (registry_.all_of<RectTransformComponent>(e)) {
                    registry_.get<RectTransformComponent>(e).enabled = active;
                }
                if (registry_.all_of<UITextComponent>(e)) {
                    registry_.get<UITextComponent>(e).enabled = active;
                }
                if (registry_.all_of<UIButtonComponent>(e)) {
                    registry_.get<UIButtonComponent>(e).enabled = active;
                }
                if (registry_.all_of<UIImageComponent>(e)) {
                    registry_.get<UIImageComponent>(e).enabled = active;
                }
            }
        }
    };

    setEntitiesActive(commandTabEntities_, currentTab_ == TabType::Command);
    setEntitiesActive(roomTabEntities_, currentTab_ == TabType::Room);
    setEntitiesActive(upgradeTabEntities_, currentTab_ == TabType::Upgrade);
    setEntitiesActive(systemTabEntities_, currentTab_ == TabType::System);
    setEntitiesActive(statusTabEntities_, currentTab_ == TabType::Status);
}

void MainScene::SwitchAppMode(AppMode mode) {
    appMode_ = mode;
    
    // UIをすべて一旦非表示にする処理
    auto view = registry_.view<RectTransformComponent>();
    for (auto e : view) {
        if (registry_.all_of<UITextComponent>(e) || registry_.all_of<UIImageComponent>(e)) {
            // 格納バー関連のエンティティか判定
            bool isBarEntity = (e == unstowBtn_);

            // ★追加: ヒーローと敵（ゲームプレイキャラ）は格納時もオービタル表示するため非表示にしない
            bool isGameplay = false;
            if (registry_.all_of<NameComponent>(e)) {
                const auto& name = registry_.get<NameComponent>(e).name;
                if (name.find("Hero") != std::string::npos || name.find("Enemy") != std::string::npos || name.find("Char_") != std::string::npos) {
                    isGameplay = true;
                }
            }
            
            if (isGameplay) {
                registry_.get<RectTransformComponent>(e).enabled = true;
            } else {
                registry_.get<RectTransformComponent>(e).enabled = (mode == AppMode::Fullscreen) ? !isBarEntity : isBarEntity;
            }
            
            // ★追加: 状態変更時にフリーズしていた古いホバー状態を強制リセットして暴発を防ぐ
            if (registry_.all_of<UIButtonComponent>(e)) {
                registry_.get<UIButtonComponent>(e).isHovered = false;
                registry_.get<UIButtonComponent>(e).isPressed = false;
            }
        }
    }
    
    // 3Dモデルなどの表示制御
    auto renderView = registry_.view<MeshRendererComponent>();
    for (auto e : renderView) {
        registry_.get<MeshRendererComponent>(e).enabled = (mode == AppMode::Fullscreen);
    }
    auto particleView = registry_.view<ParticleEmitterComponent>();
    for (auto e : particleView) {
        registry_.get<ParticleEmitterComponent>(e).enabled = (mode == AppMode::Fullscreen);
    }
    
    if (mode == AppMode::Fullscreen) {
        SwitchTab(currentTab_); // タブ表示状態を復元
    } else {
        // オービタル格納時は復帰ボタンのみ表示
        if (registry_.valid(unstowBtn_)) registry_.get<RectTransformComponent>(unstowBtn_).enabled = true;
    }
}

void MainScene::AddLog(const std::string& msg) {
    logMessages_.push_back(msg);
    if (logMessages_.size() > 100) {
        logMessages_.erase(logMessages_.begin());
    }
    // 新しいログが追加されたら一番下までスクロールさせる
    logScrollOffset_ = 0;
    UpdateLogUI();
}

void MainScene::UpdateLogUI() {
    int maxDisplay = static_cast<int>(logTextEntities_.size());
    int totalLogs = static_cast<int>(logMessages_.size());
    
    // スクロールオフセットの上限を調整
    if (logScrollOffset_ > totalLogs - maxDisplay) {
        logScrollOffset_ = std::max(0, totalLogs - maxDisplay);
    }
    if (logScrollOffset_ < 0) logScrollOffset_ = 0;
    
    int startIndex = totalLogs - maxDisplay - logScrollOffset_;
    if (startIndex < 0) startIndex = 0;
    
    for (int i = 0; i < maxDisplay; i++) {
        if (registry_.valid(logTextEntities_[i])) {
            int msgIdx = startIndex + i;
            if (msgIdx >= 0 && msgIdx < totalLogs) {
                registry_.get<UITextComponent>(logTextEntities_[i]).text = logMessages_[msgIdx];
            } else {
                registry_.get<UITextComponent>(logTextEntities_[i]).text = "";
            }
        }
    }
}

void MainScene::ShowEquipPopup(int roleIdx) {
    ClosePopup(); // 古いポップアップを削除
    
    auto viewText = registry_.view<UITextComponent>();
    for (auto e : viewText) { registry_.get<UITextComponent>(e).enabled = false; }
    auto viewBtn = registry_.view<UIButtonComponent>();
    for (auto e : viewBtn) { registry_.get<UIButtonComponent>(e).enabled = false; }

    auto createPanel = [&](const std::string& name, float x, float y, float w, float h, DirectX::XMFLOAT4 color, int z = 0) {
        auto ent = registry_.create();
        registry_.emplace<NameComponent>(ent, name);
        auto& t = registry_.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
        t.pos = {x, y}; t.size = {w, h};
        auto& img = registry_.emplace<UIImageComponent>(ent);
        img.texturePath = "Resources/Textures/white1x1.png";
        img.textureHandle = renderer_->LoadTexture2D(img.texturePath);
        img.color = color;
        img.layer = z;
        return ent;
    };
    auto createText = [&](const std::string& name, const std::string& textStr, float x, float y, float fontSize, DirectX::XMFLOAT4 color) {
        auto ent = registry_.create();
        registry_.emplace<NameComponent>(ent, name);
        auto& t = registry_.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
        t.pos = {x, y}; t.size = {0, 0};
        auto& text = registry_.emplace<UITextComponent>(ent);
        text.text = textStr;
        text.fontSize = fontSize;
        text.color = color;
        text.fontPath = "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf";
        return ent;
    };
    auto createButton = [&](const std::string& name, const std::string& textStr, float x, float y, float w, float h, DirectX::XMFLOAT4 baseColor, float fontSize = 20.0f, DirectX::XMFLOAT4 textColor = {0.0f, 0.0f, 0.0f, 1.0f}, int z = 8) {
        auto ent = registry_.create();
        registry_.emplace<NameComponent>(ent, name);
        auto& t = registry_.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
        t.pos = {x, y}; t.size = {w, h};
        auto& img = registry_.emplace<UIImageComponent>(ent);
        img.texturePath = "Resources/Textures/white1x1.png";
        img.textureHandle = renderer_->LoadTexture2D(img.texturePath);
        img.color = baseColor;
        img.layer = z;
        auto& btn = registry_.emplace<UIButtonComponent>(ent);
        btn.normalColor = baseColor;
        btn.hoverColor = {baseColor.x * 0.85f, baseColor.y * 0.85f, baseColor.z * 0.85f, 1.0f};
        btn.pressedColor = {baseColor.x * 0.7f, baseColor.y * 0.7f, baseColor.z * 0.7f, 1.0f};
        auto& text = registry_.emplace<UITextComponent>(ent);
        text.text = textStr;
        text.fontSize = fontSize;
        text.color = textColor;
        text.fontPath = "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf";
        return ent;
    };

    popupEntities_.push_back(createPanel("PopupOverlay", 0, 0, 1920, 1080, {0,0,0,0.5f}, 3));
    // BGのサイズを広げ、左寄りにする
    popupEntities_.push_back(createPanel("PopupBG", 850, 100, 1000, 800, {0.95f, 0.95f, 0.95f, 1.0f}, 4));
    
    std::string roleNames[] = {"タンク", "スナイパー", "スカウト", "エンジニア", "ヒーラー"};
    std::string popupTitle = (roleIdx == -1) ? "装備図鑑" : roleNames[roleIdx] + "の装備変更";
    popupEntities_.push_back(createText("PopupTitle", popupTitle, 870, 120, 24.0f, {0.1f, 0.1f, 0.1f, 1.0f}));
    
    popupEntities_.push_back(createButton("ClosePopupBtn", "閉じる", 1720, 120, 100, 40, {0.8f, 0.2f, 0.2f, 1.0f}, 16.0f, {1,1,1,1}));
    
    auto viewProg = registry_.view<DesktopPartyProgressComponent>();
    if (!viewProg.empty()) {
        auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
        
        equipHoverRects_.clear();
        
        // --- 左側ペイン (装備スロット 5つ) ---
        if (roleIdx != -1) {
            std::string slotNames[] = {"USBメモリ(頭)", "拡張基板(胴)", "コンデンサ(手)", "冷却ファン(足)", "LANケーブル(腰)"};
            for (int i = 0; i < 5; ++i) {
                float slotY = 180.0f + i * 140.0f;
                // アクティブなスロットは色を変える
                DirectX::XMFLOAT4 bgColor = (equipPopupActiveSlot_ == i) ? DirectX::XMFLOAT4{0.8f, 0.9f, 1.0f, 1.0f} : DirectX::XMFLOAT4{1.0f, 1.0f, 1.0f, 1.0f};
                popupEntities_.push_back(createPanel("SlotBG_" + std::to_string(i), 870, slotY, 350, 130, bgColor, 5));
                popupEntities_.push_back(createText("SlotName_" + std::to_string(i), slotNames[i], 880, slotY + 10, 18.0f, {0.3f, 0.3f, 0.3f, 1.0f}));
                
                // 現在の装備
                int eqId = prog.equippedIds[roleIdx * 5 + i];
                if (eqId == -1) {
                    popupEntities_.push_back(createText("SlotEq_" + std::to_string(i), "未装備", 890, slotY + 50, 20.0f, {0.7f, 0.7f, 0.7f, 1.0f}));
                } else {
                    for (const auto& eq : prog.inventory) {
                        if (eq.id == static_cast<uint32_t>(eqId)) {
                            popupEntities_.push_back(createText("SlotEq_" + std::to_string(i), eq.name, 890, slotY + 40, 18.0f, eq.GetRarityColor()));
                            popupEntities_.push_back(createText("SlotSt_" + std::to_string(i), "ATK+" + std::to_string((int)(eq.atkMul*100)) + "% HP+" + std::to_string((int)(eq.hpMul*100)) + "%", 890, slotY + 70, 14.0f, {0.5f, 0.5f, 0.5f, 1.0f}));
                            
                            // 外すボタン
                            popupEntities_.push_back(createButton("UnequipBtn_" + std::to_string(i), "外す", 1140, slotY + 10, 70, 30, {0.8f, 0.8f, 0.8f, 1.0f}, 14.0f, {0.1f, 0.1f, 0.1f, 1.0f}));
                            
                            // ツールチップ用に矩形を登録
                            equipHoverRects_.push_back({870.0f, slotY, 350.0f, 130.0f, eq});
                            break;
                        }
                    }
                }
                
                // 選択ボタン
                popupEntities_.push_back(createButton("SelectSlotBtn_" + std::to_string(i), "変更", 1140, slotY + 90, 70, 30, {0.4f, 0.6f, 1.0f, 1.0f}, 14.0f, {1, 1, 1, 1}));
            }
        }
        
        // --- 右側ペイン (インベントリ) ---
        float invX = 1240.0f;
        float invY = 180.0f;
        
        // フィルタボタン群
        popupEntities_.push_back(createText("FilterLbl", "レアリティ:", invX, invY, 16.0f, {0.3f, 0.3f, 0.3f, 1.0f}));
        std::string rarities[] = {"All", "Common", "Rare", "Epic", "Artifact"};
        for (int i = -1; i < 4; ++i) {
            DirectX::XMFLOAT4 btnCol = (equipPopupFilterRarity_ == i) ? DirectX::XMFLOAT4{0.2f, 0.6f, 0.2f, 1.0f} : DirectX::XMFLOAT4{0.8f, 0.8f, 0.8f, 1.0f};
            popupEntities_.push_back(createButton("FilterRar_" + std::to_string(i), rarities[i+1], invX + 100 + (i+1)*80, invY - 5, 70, 30, btnCol, 14.0f, (equipPopupFilterRarity_ == i) ? DirectX::XMFLOAT4{1,1,1,1} : DirectX::XMFLOAT4{0,0,0,1}));
        }
        
        // アイコン(グリッド)表示領域
        float gridStartX = invX;
        float gridStartY = invY + 50.0f;
        int cols = 5;
        float iconSize = 100.0f;
        float padding = 15.0f;
        
        // フィルタリング
        std::vector<Equipment> filtered;
        for (const auto& eq : prog.inventory) {
            if (roleIdx != -1 && (int)eq.type != equipPopupActiveSlot_) continue; // 部位フィルタ
            if (equipPopupFilterRarity_ != -1 && (int)eq.rarity != equipPopupFilterRarity_) continue; // レアリティフィルタ
            filtered.push_back(eq);
        }
        
        float clipYMin = gridStartY;
        float clipYMax = 880.0f;
        
        for (size_t i = 0; i < filtered.size(); ++i) {
            const auto& eq = filtered[i];
            int col = static_cast<int>(i) % cols;
            int row = static_cast<int>(i) / cols;
            
            float itemX = gridStartX + col * (iconSize + padding);
            float itemY = gridStartY + row * (iconSize + padding) - equipPopupScrollY_;
            
            if (itemY < clipYMin || itemY + iconSize > clipYMax) continue; // 範囲内に完全に収まっているものだけ表示
            
            DirectX::XMFLOAT4 rColor = eq.GetRarityColor();
            // アイコン背景と枠 (背景を少し明るく、Z順序を整理)
            popupEntities_.push_back(createPanel("InvIconBG_" + std::to_string(eq.id), itemX, itemY, iconSize, iconSize, {rColor.x*0.4f, rColor.y*0.4f, rColor.z*0.4f, 1.0f}, 5));
            popupEntities_.push_back(createPanel("InvIconBorderT_" + std::to_string(eq.id), itemX, itemY, iconSize, 4, rColor, 6)); // Top
            popupEntities_.push_back(createPanel("InvIconBorderB_" + std::to_string(eq.id), itemX, itemY+iconSize-4, iconSize, 4, rColor, 6)); // Bottom
            popupEntities_.push_back(createPanel("InvIconBorderL_" + std::to_string(eq.id), itemX, itemY, 4, iconSize, rColor, 6)); // Left
            popupEntities_.push_back(createPanel("InvIconBorderR_" + std::to_string(eq.id), itemX+iconSize-4, itemY, 4, iconSize, rColor, 6)); // Right
            
            // 短縮情報
            std::string shortName = "Lv" + std::to_string((int)((eq.atkMul + eq.hpMul + eq.spdMul)*10)); 
            popupEntities_.push_back(createText("InvIconName_" + std::to_string(eq.id), shortName, itemX + 5, itemY + 10, 14.0f, {1,1,1,1}));
            popupEntities_.push_back(createText("InvIconSt_" + std::to_string(eq.id), "+" + std::to_string((int)(eq.atkMul*100)), itemX + 5, itemY + 40, 16.0f, {0.8f, 0.8f, 0.8f, 1.0f}));
            
            // ツールチップ用に矩形を登録
            equipHoverRects_.push_back({itemX, itemY, iconSize, iconSize, eq});
            
            if (roleIdx != -1) {
                popupEntities_.push_back(createButton("EquipBtn_" + std::to_string(eq.id) + "_" + std::to_string(roleIdx), "装備", itemX + 15, itemY + 65, 70, 25, {0.8f, 0.8f, 0.8f, 1.0f}, 12.0f, {0,0,0,1}));
            }
        }
    }
}

void MainScene::ShowRelicPopup() {
    ClosePopup(); // 古いポップアップを削除
    
    auto viewText = registry_.view<UITextComponent>();
    for (auto e : viewText) { registry_.get<UITextComponent>(e).enabled = false; }
    auto viewBtn = registry_.view<UIButtonComponent>();
    for (auto e : viewBtn) { registry_.get<UIButtonComponent>(e).enabled = false; }

    auto createPanel = [&](const std::string& name, float x, float y, float w, float h, DirectX::XMFLOAT4 color, int z = 0) {
        auto ent = registry_.create();
        registry_.emplace<NameComponent>(ent, name);
        auto& t = registry_.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
        t.pos = {x, y}; t.size = {w, h};
        auto& img = registry_.emplace<UIImageComponent>(ent);
        img.texturePath = "Resources/Textures/white1x1.png";
        img.textureHandle = renderer_->LoadTexture2D(img.texturePath);
        img.color = color;
        img.layer = z;
        return ent;
    };
    auto createText = [&](const std::string& name, const std::string& textStr, float x, float y, float fontSize, DirectX::XMFLOAT4 color) {
        auto ent = registry_.create();
        registry_.emplace<NameComponent>(ent, name);
        auto& t = registry_.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
        t.pos = {x, y}; t.size = {0, 0};
        auto& text = registry_.emplace<UITextComponent>(ent);
        text.text = textStr;
        text.fontSize = fontSize;
        text.color = color;
        text.fontPath = "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf";
        return ent;
    };
    auto createButton = [&](const std::string& name, const std::string& textStr, float x, float y, float w, float h, DirectX::XMFLOAT4 baseColor, float fontSize = 20.0f, DirectX::XMFLOAT4 textColor = {0.0f, 0.0f, 0.0f, 1.0f}) {
        auto ent = registry_.create();
        registry_.emplace<NameComponent>(ent, name);
        auto& t = registry_.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
        t.pos = {x, y}; t.size = {w, h};
        auto& img = registry_.emplace<UIImageComponent>(ent);
        img.texturePath = "Resources/Textures/white1x1.png";
        img.textureHandle = renderer_->LoadTexture2D(img.texturePath);
        img.color = baseColor;
        img.layer = 5;
        auto& btn = registry_.emplace<UIButtonComponent>(ent);
        btn.normalColor = baseColor;
        btn.hoverColor = {baseColor.x * 0.85f, baseColor.y * 0.85f, baseColor.z * 0.85f, 1.0f};
        btn.pressedColor = {baseColor.x * 0.7f, baseColor.y * 0.7f, baseColor.z * 0.7f, 1.0f};
        auto& text = registry_.emplace<UITextComponent>(ent);
        text.text = textStr;
        text.fontSize = fontSize;
        text.color = textColor;
        text.fontPath = "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf";
        return ent;
    };

    popupEntities_.push_back(createPanel("PopupOverlay", 0, 0, 1920, 1080, {0,0,0,0.5f}, 3));
    popupEntities_.push_back(createPanel("PopupBG", 1300, 200, 580, 600, {0.1f, 0.1f, 0.2f, 1.0f}, 4));
    
    popupEntities_.push_back(createText("PopupTitle", "インターネット遺物図鑑", 1320, 220, 24.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
    popupEntities_.push_back(createButton("ClosePopupBtn", "閉じる", 1750, 220, 100, 40, {0.8f, 0.2f, 0.2f, 1.0f}, 16.0f, {1,1,1,1}));
    
    popupEntities_.push_back(createText("RelicDesc", "発掘されたインターネットの遺物たち", 1320, 280, 18.0f, {0.7f, 0.7f, 0.8f, 1.0f}));
    
    auto viewProg = registry_.view<DesktopPartyProgressComponent>();
    if (!viewProg.empty()) {
        auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
        
        if (prog.relicEncyclopedia.empty()) {
            prog.relicEncyclopedia = {
                {0, "フロッピーディスク", "保存容量1.44MB。 全体ATK+5%", false, 0.05f, 0.0f, 0.0f},
                {1, "MS-DOS画面", "黒画面と緑の文字。 全体HP+5%", false, 0.0f, 0.05f, 0.0f},
                {2, "ダイヤルアップ音", "接続時のあの音。 全体SPD+5%", false, 0.0f, 0.0f, 0.05f},
                {3, "ブラウン管モニタ", "重くてデカい。 全体HP+10%", false, 0.0f, 0.10f, 0.0f},
                {4, "IEショートカット", "もう開くことはない。 全体ATK+10%", false, 0.10f, 0.0f, 0.0f}
            };
        }
        
        float currentY = 320.0f;
        for (const auto& relic : prog.relicEncyclopedia) {
            popupEntities_.push_back(createPanel("RelicSilBG", 1320.0f, currentY, 540.0f, 60.0f, {0.2f, 0.2f, 0.3f, 1.0f}, 5));
            if (relic.isUnlocked) {
                popupEntities_.push_back(createText("RelicName", relic.name, 1340.0f, currentY + 10.0f, 20.0f, {1.0f, 0.8f, 0.2f, 1.0f}));
                popupEntities_.push_back(createText("RelicDesc", relic.description, 1340.0f, currentY + 35.0f, 14.0f, {0.8f, 0.8f, 0.8f, 1.0f}));
            } else {
                popupEntities_.push_back(createText("RelicSilTxt", "??? (未発見)", 1340.0f, currentY + 20.0f, 20.0f, {0.4f, 0.4f, 0.5f, 1.0f}));
            }
            currentY += 70.0f;
        }
    }
}

void MainScene::ClosePopup() {
    for (auto e : popupEntities_) {
        if (registry_.valid(e)) {
            registry_.destroy(e);
        }
    }
    popupEntities_.clear();

    auto viewText = registry_.view<UITextComponent>();
    for (auto e : viewText) { registry_.get<UITextComponent>(e).enabled = true; }
    auto viewBtn = registry_.view<UIButtonComponent>();
    for (auto e : viewBtn) { registry_.get<UIButtonComponent>(e).enabled = true; }
    
    SwitchTab(currentTab_);
    SwitchAppMode(appMode_);
}

void MainScene::ShowPrestigePopup() {
    ClosePopup();
    
    auto viewText = registry_.view<UITextComponent>();
    for (auto e : viewText) { registry_.get<UITextComponent>(e).enabled = false; }
    auto viewBtn = registry_.view<UIButtonComponent>();
    for (auto e : viewBtn) { registry_.get<UIButtonComponent>(e).enabled = false; }

    auto createPanel = [&](const std::string& name, float x, float y, float w, float h, DirectX::XMFLOAT4 color, int z = 0) {
        auto ent = registry_.create();
        registry_.emplace<NameComponent>(ent, name);
        auto& t = registry_.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
        t.pos = {x, y}; t.size = {w, h};
        auto& img = registry_.emplace<UIImageComponent>(ent);
        img.texturePath = "Resources/Textures/white1x1.png";
        img.textureHandle = renderer_->LoadTexture2D(img.texturePath);
        img.color = color;
        img.layer = z;
        return ent;
    };
    auto createText = [&](const std::string& name, const std::string& textStr, float x, float y, float fontSize, DirectX::XMFLOAT4 color) {
        auto ent = registry_.create();
        registry_.emplace<NameComponent>(ent, name);
        auto& t = registry_.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
        t.pos = {x, y}; t.size = {0, 0};
        auto& text = registry_.emplace<UITextComponent>(ent);
        text.text = textStr;
        text.fontSize = fontSize;
        text.color = color;
        text.fontPath = "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf";
        return ent;
    };
    auto createButton = [&](const std::string& name, const std::string& textStr, float x, float y, float w, float h, DirectX::XMFLOAT4 baseColor, float fontSize = 20.0f, DirectX::XMFLOAT4 textColor = {0.0f, 0.0f, 0.0f, 1.0f}) {
        auto ent = registry_.create();
        registry_.emplace<NameComponent>(ent, name);
        auto& t = registry_.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
        t.pos = {x, y}; t.size = {w, h};
        auto& img = registry_.emplace<UIImageComponent>(ent);
        img.texturePath = "Resources/Textures/white1x1.png";
        img.textureHandle = renderer_->LoadTexture2D(img.texturePath);
        img.color = baseColor;
        img.layer = 5;
        auto& btn = registry_.emplace<UIButtonComponent>(ent);
        btn.normalColor = baseColor;
        btn.hoverColor = {baseColor.x * 0.85f, baseColor.y * 0.85f, baseColor.z * 0.85f, 1.0f};
        btn.pressedColor = {baseColor.x * 0.7f, baseColor.y * 0.7f, baseColor.z * 0.7f, 1.0f};
        auto& text = registry_.emplace<UITextComponent>(ent);
        text.text = textStr;
        text.fontSize = fontSize;
        text.color = textColor;
        text.fontPath = "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf";
        return ent;
    };

    popupEntities_.push_back(createPanel("PopupOverlay", 0, 0, 1920, 1080, {0,0,0,0.5f}, 3));
    popupEntities_.push_back(createPanel("PopupBG", 1300, 200, 580, 600, {0.95f, 0.95f, 0.95f, 1.0f}, 4));
    
    popupEntities_.push_back(createText("PopupTitle", "転生スキルツリー", 1320, 220, 24.0f, {0.1f, 0.1f, 0.1f, 1.0f}));
    popupEntities_.push_back(createButton("ClosePopupBtn", "閉じる", 1750, 220, 100, 40, {0.8f, 0.2f, 0.2f, 1.0f}, 16.0f, {1,1,1,1}));
    
    if (combatSys_) {
        popupEntities_.push_back(createText("PrestigePointsTxt", "残りスキルポイント: " + std::to_string(combatSys_->prestigePoints), 1320, 280, 20.0f, {0.1f, 0.6f, 0.1f, 1.0f}));
        
        float btnBaseY = 330.0f;
        float spacing = 60.0f;
        
        // ATK Skill
        popupEntities_.push_back(createPanel("SkillAtkBG", 1320, btnBaseY, 530, 50, {1,1,1,1}, 4));
        popupEntities_.push_back(createText("SkillAtkTxt", "パーティ全体の攻撃力 +" + std::to_string((int)((combatSys_->skillLevelAtk + 1) * 50)) + "%", 1330, btnBaseY + 15, 18.0f, {0.2f,0.2f,0.2f,1}));
        popupEntities_.push_back(createText("SkillAtkLv", "Lv." + std::to_string(combatSys_->skillLevelAtk), 1700, btnBaseY + 15, 18.0f, {0.1f,0.5f,0.8f,1}));
        if (combatSys_->prestigePoints > 0) {
            popupEntities_.push_back(createButton("PrestigeSkillAtkBtn", "習得", 1770, btnBaseY + 10, 60, 30, {0.3f, 0.8f, 0.3f, 1.0f}, 16.0f, {1,1,1,1}));
        }
        
        // HP Skill
        btnBaseY += spacing;
        popupEntities_.push_back(createPanel("SkillHpBG", 1320, btnBaseY, 530, 50, {1,1,1,1}, 4));
        popupEntities_.push_back(createText("SkillHpTxt", "パーティ全体の最大HP +" + std::to_string((int)((combatSys_->skillLevelHp + 1) * 50)) + "%", 1330, btnBaseY + 15, 18.0f, {0.2f,0.2f,0.2f,1}));
        popupEntities_.push_back(createText("SkillHpLv", "Lv." + std::to_string(combatSys_->skillLevelHp), 1700, btnBaseY + 15, 18.0f, {0.1f,0.5f,0.8f,1}));
        if (combatSys_->prestigePoints > 0) {
            popupEntities_.push_back(createButton("PrestigeSkillHpBtn", "習得", 1770, btnBaseY + 10, 60, 30, {0.3f, 0.8f, 0.3f, 1.0f}, 16.0f, {1,1,1,1}));
        }

        // SPD Skill
        btnBaseY += spacing;
        popupEntities_.push_back(createPanel("SkillSpdBG", 1320, btnBaseY, 530, 50, {1,1,1,1}, 4));
        popupEntities_.push_back(createText("SkillSpdTxt", "パーティ全体の移動速度 +" + std::to_string((int)((combatSys_->skillLevelSpd + 1) * 10)) + "%", 1330, btnBaseY + 15, 18.0f, {0.2f,0.2f,0.2f,1}));
        popupEntities_.push_back(createText("SkillSpdLv", "Lv." + std::to_string(combatSys_->skillLevelSpd), 1700, btnBaseY + 15, 18.0f, {0.1f,0.5f,0.8f,1}));
        if (combatSys_->prestigePoints > 0) {
            popupEntities_.push_back(createButton("PrestigeSkillSpdBtn", "習得", 1770, btnBaseY + 10, 60, 30, {0.3f, 0.8f, 0.3f, 1.0f}, 16.0f, {1,1,1,1}));
        }

        // Gold Skill
        btnBaseY += spacing;
        popupEntities_.push_back(createPanel("SkillGoldBG", 1320, btnBaseY, 530, 50, {1,1,1,1}, 4));
        popupEntities_.push_back(createText("SkillGoldTxt", "敵からの獲得ゴールド +" + std::to_string((int)((combatSys_->skillLevelGold + 1) * 50)) + "%", 1330, btnBaseY + 15, 18.0f, {0.2f,0.2f,0.2f,1}));
        popupEntities_.push_back(createText("SkillGoldLv", "Lv." + std::to_string(combatSys_->skillLevelGold), 1700, btnBaseY + 15, 18.0f, {0.1f,0.5f,0.8f,1}));
        if (combatSys_->prestigePoints > 0) {
            popupEntities_.push_back(createButton("PrestigeSkillGoldBtn", "習得", 1770, btnBaseY + 10, 60, 30, {0.3f, 0.8f, 0.3f, 1.0f}, 16.0f, {1,1,1,1}));
        }
        
        // Reset Button
        popupEntities_.push_back(createText("ResetNote", "転生するとLvと進行ステージが初期化され、1pt獲得します", 1320, 680, 16.0f, {0.8f, 0.3f, 0.3f, 1.0f}));
        popupEntities_.push_back(createButton("PrestigeResetRunBtn", "このセーブデータで転生を実行する", 1320, 710, 530, 50, {1.0f, 0.4f, 0.4f, 1.0f}, 20.0f, {1,1,1,1}));
    }
}

void MainScene::ShowGameOverPopup() {
    ClosePopup();
    
    auto viewText = registry_.view<UITextComponent>();
    for (auto e : viewText) { registry_.get<UITextComponent>(e).enabled = false; }
    auto viewBtn = registry_.view<UIButtonComponent>();
    for (auto e : viewBtn) { registry_.get<UIButtonComponent>(e).enabled = false; }

    auto createPanel = [&](const std::string& name, float x, float y, float w, float h, DirectX::XMFLOAT4 color, int z = 0) {
        auto ent = registry_.create();
        registry_.emplace<NameComponent>(ent, name);
        auto& t = registry_.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
        t.pos = {x, y}; t.size = {w, h};
        auto& img = registry_.emplace<UIImageComponent>(ent);
        img.texturePath = "Resources/Textures/white1x1.png";
        img.textureHandle = renderer_->LoadTexture2D(img.texturePath);
        img.color = color;
        img.layer = z;
        return ent;
    };
    auto createText = [&](const std::string& name, const std::string& textStr, float x, float y, float fontSize, DirectX::XMFLOAT4 color) {
        auto ent = registry_.create();
        registry_.emplace<NameComponent>(ent, name);
        auto& t = registry_.emplace<RectTransformComponent>(ent);
        t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
        t.pos = {x, y}; t.size = {0,0};
        auto& txt = registry_.emplace<UITextComponent>(ent);
        txt.text = textStr; txt.fontSize = fontSize; txt.color = color;
        txt.fontPath = "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf";
        return ent;
    };
    auto createButton = [&](const std::string& name, const std::string& textStr, float x, float y, float w, float h, DirectX::XMFLOAT4 bgCol, float fontSize) {
        auto ent = createPanel(name, x, y, w, h, bgCol, 4);
        registry_.emplace<UIButtonComponent>(ent);
        auto& btn = registry_.get<UIButtonComponent>(ent);
        btn.normalColor = bgCol;
        btn.hoverColor = {bgCol.x * 0.85f, bgCol.y * 0.85f, bgCol.z * 0.85f, 1.0f};
        btn.pressedColor = {bgCol.x * 0.7f, bgCol.y * 0.7f, bgCol.z * 0.7f, 1.0f};
        auto txtEnt = createText(name + "Txt", textStr, x + 40, y + (h/2) - (fontSize/2), fontSize, {1,1,1,1});
        popupEntities_.push_back(txtEnt);
        return ent;
    };

    popupEntities_.push_back(createPanel("PopupOverlay", 0, 0, 1920, 1080, {0,0,0,0.8f}, 3));
    popupEntities_.push_back(createPanel("PopupBG", 1300, 300, 580, 400, {0.1f, 0.1f, 0.15f, 1.0f}, 4));
    
    popupEntities_.push_back(createText("PopupTitle", "GAME OVER", 1450, 350, 48.0f, {1.0f, 0.3f, 0.3f, 1.0f}));
    popupEntities_.push_back(createText("PopupDesc", "司令室が破壊されました...", 1450, 430, 24.0f, {0.8f, 0.8f, 0.8f, 1.0f}));
    
    int maxHeroLv = 1;
    if (combatSys_) {
        for (int i=0; i<4; i++) { if (combatSys_->heroLevels[i] > maxHeroLv) maxHeroLv = combatSys_->heroLevels[i]; }
    }
    
    if (maxHeroLv >= 20) {
        auto btnPrestige = createButton("BtnGameOverPrestige", "システム再起動 (転生)", 1350, 520, 480, 50, {0.8f, 0.2f, 0.2f, 1.0f}, 24.0f);
        registry_.get<UIImageComponent>(btnPrestige).layer = 5;
        popupEntities_.push_back(btnPrestige);
    } else {
        auto btnPrestige = createButton("BtnGameOverPrestigeDisabled", "システム再起動にはLv20のキャラが必要", 1350, 520, 480, 50, {0.4f, 0.4f, 0.4f, 1.0f}, 18.0f);
        registry_.get<UIImageComponent>(btnPrestige).layer = 5;
        popupEntities_.push_back(btnPrestige);
    }
    
    auto btnRetry = createButton("BtnGameOverRetry", "現在のステージをやり直す", 1350, 590, 480, 50, {0.2f, 0.4f, 0.8f, 1.0f}, 24.0f);
    registry_.get<UIImageComponent>(btnRetry).layer = 5;
    popupEntities_.push_back(btnRetry);
}

void MainScene::UpdateTooltip(float mouseX, float mouseY) {
    if (popupEntities_.empty() || currentEquipRoleIdx_ == -1) {
        // ポップアップが閉じていれば非表示
        for (auto e : tooltipEntities_) {
            if (registry_.valid(e) && registry_.any_of<RectTransformComponent>(e)) {
                registry_.get<RectTransformComponent>(e).enabled = false;
            }
        }
        return;
    }

    bool isHovering = false;
    Game::Equipment hoveredEq;
    
    for (const auto& rect : equipHoverRects_) {
        if (mouseX >= rect.x && mouseX <= rect.x + rect.w &&
            mouseY >= rect.y && mouseY <= rect.y + rect.h) {
            isHovering = true;
            hoveredEq = rect.eq;
            break;
        }
    }

    if (!isHovering) {
        // ツールチップ非表示
        for (auto e : tooltipEntities_) {
            if (registry_.valid(e) && registry_.any_of<RectTransformComponent>(e)) {
                registry_.get<RectTransformComponent>(e).enabled = false;
            }
        }
        return;
    }

    // ツールチップエンティティの初期化(初回のみ)
    if (tooltipEntities_.empty()) {
        auto createEntity = [&]() {
            auto ent = registry_.create();
            tooltipEntities_.push_back(ent);
            return ent;
        };

        auto bg = createEntity();
        registry_.emplace<NameComponent>(bg, "TooltipBG");
        auto& t1 = registry_.emplace<RectTransformComponent>(bg);
        t1.anchor = {0.0f, 0.0f}; t1.pivot = {0.0f, 0.0f};
        t1.size = {280.0f, 130.0f};
        auto& img1 = registry_.emplace<UIImageComponent>(bg);
        img1.texturePath = "Resources/Textures/white1x1.png";
        img1.textureHandle = renderer_->LoadTexture2D(img1.texturePath);
        img1.color = {0.05f, 0.05f, 0.05f, 0.95f};
        img1.layer = 20;

        auto title = createEntity();
        registry_.emplace<NameComponent>(title, "TooltipTitle");
        auto& t2 = registry_.emplace<RectTransformComponent>(title);
        t2.anchor = {0.0f, 0.0f}; t2.pivot = {0.0f, 0.0f};
        t2.size = {280.0f, 30.0f};
        auto& txt1 = registry_.emplace<UITextComponent>(title);
        txt1.fontPath = "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf";
        txt1.fontSize = 20.0f;
        txt1.color = {1.0f, 1.0f, 1.0f, 1.0f};

        auto desc = createEntity();
        registry_.emplace<NameComponent>(desc, "TooltipDesc");
        auto& t3 = registry_.emplace<RectTransformComponent>(desc);
        t3.anchor = {0.0f, 0.0f}; t3.pivot = {0.0f, 0.0f};
        t3.size = {280.0f, 60.0f};
        auto& txt2 = registry_.emplace<UITextComponent>(desc);
        txt2.fontPath = "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf";
        txt2.fontSize = 16.0f;
        txt2.color = {0.9f, 0.9f, 0.9f, 1.0f};
    }

    // 表示位置の決定 (マウスの右下に表示、画面端なら調整)
    float ttX = mouseX + 15.0f;
    float ttY = mouseY + 15.0f;
    if (ttX + 280.0f > 1900.0f) ttX = mouseX - 295.0f;
    if (ttY + 130.0f > 1050.0f) ttY = mouseY - 145.0f;

    // ツールチップの内容と位置を更新して表示
    for (size_t i = 0; i < tooltipEntities_.size(); ++i) {
        auto e = tooltipEntities_[i];
        registry_.get<RectTransformComponent>(e).enabled = true;
        
        auto& rect = registry_.get<RectTransformComponent>(e);
        if (i == 0) { // BG
            rect.pos = {ttX, ttY};
        } else if (i == 1) { // Title
            rect.pos = {ttX + 10.0f, ttY + 10.0f};
            auto& txt = registry_.get<UITextComponent>(e);
            txt.text = hoveredEq.name;
            txt.color = hoveredEq.GetRarityColor();
        } else if (i == 2) { // Desc
            rect.pos = {ttX + 10.0f, ttY + 45.0f};
            auto& txt = registry_.get<UITextComponent>(e);
            std::string st = "ATK: +" + std::to_string(static_cast<int>(hoveredEq.atkMul * 100)) + "%\n" +
                             "HP:  +" + std::to_string(static_cast<int>(hoveredEq.hpMul * 100)) + "%\n" +
                             "SPD: +" + std::to_string(static_cast<int>(hoveredEq.spdMul * 100)) + "%\n\n" +
                             hoveredEq.flavorText;
            txt.text = st;
        }
    }
}

} // namespace Game
