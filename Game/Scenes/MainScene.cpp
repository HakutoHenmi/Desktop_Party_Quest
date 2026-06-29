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
    workEnergySystem->Reset(registry_);
    systems_.push_back(std::move(workEnergySystem));

    auto combatSystem = std::make_unique<DesktopCombatSystem>();
    combatSys_ = combatSystem.get();
    combatSystem->Reset(registry_);
    systems_.push_back(std::move(combatSystem));

    auto spriteAISystem = std::make_unique<SpriteCharacterAISystem>();
    systems_.push_back(std::move(spriteAISystem));

    // ★追加: 画面に表示するオブジェクトが何もない状態だったため、生成したドット絵キャラクターを配置
    {
        auto createCharacter = [&](const std::string& name, const std::string& texPath, float startX, float startY) {
            auto ent = registry_.create();
            registry_.emplace<NameComponent>(ent, name);
            auto& t = registry_.emplace<RectTransformComponent>(ent);
            t.pos = {startX, startY};
            t.size = {128, 128}; 
            t.anchor = {0.0f, 0.0f}; t.pivot = {0.5f, 0.5f};
            auto& img = registry_.emplace<UIImageComponent>(ent);
            img.texturePath = texPath;
            if (renderer_) {
                img.textureHandle = renderer_->LoadTexture2D(img.texturePath, false); // ★ sRGB = false (ガンマ補正による暗色化を防ぐ)
            }
            img.color = {1, 1, 1, 1};
            img.layer = -5; // デスクトップの奥側
            registry_.emplace<SpriteCharacterAIComponent>(ent);
            return ent;
        };

        createCharacter("Char_Tank", "Resources/Textures/Characters/char_tank.png", 300, 600);
        createCharacter("Char_Sniper", "Resources/Textures/Characters/char_sniper.png", 600, 500);
        createCharacter("Char_Scout", "Resources/Textures/Characters/char_scout.png", 900, 700);
        createCharacter("Char_Engineer", "Resources/Textures/Characters/char_engineer.png", 1200, 450);
        createCharacter("Char_Healer", "Resources/Textures/Characters/char_healer.png", 500, 550);
    }

    {
        auto createPanel = [&](const std::string& name, float x, float y, float w, float h, DirectX::XMFLOAT4 color, int z = 0) {
            auto ent = registry_.create();
            registry_.emplace<NameComponent>(ent, name);
            auto& t = registry_.emplace<RectTransformComponent>(ent);
            t.anchor = {0.0f, 0.0f}; t.pivot = {0.0f, 0.0f};
            t.pos = {x, y}; t.size = {w, h};
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
        createPanel("Shadow", 1290, 40, 590, 950, {0.0f, 0.0f, 0.0f, 0.2f}, 0);
        // 背景パネル (白)
        createPanel("MainPanel", 1300, 40, 580, 940, {1.0f, 1.0f, 1.0f, 1.0f}, 0);

        // --- ヘッダー領域 ---
        createText("Title", "司令室", 1320, 60, 32.0f, {0.1f, 0.1f, 0.1f, 1.0f});
        createText("Gold", "所持: 1250 G", 1520, 70, 20.0f, {0.7f, 0.5f, 0.0f, 1.0f});
        
        // ★追加: 格納ボタン (オービタル格納)
        stowButton_ = createButton("StowBtn", "⬇ オービタル格納", 1680, 65, 210, 30, {0.9f, 0.9f, 0.9f, 1.0f}, 16.0f);

        // ★追加: オービタル格納時の復帰ボタン（画面右下に配置）
        RECT wa = Engine::WindowDX::GetWorkArea();
        float bottomY = wa.bottom - 48.0f;
        float rightX = wa.right - 120.0f;
        unstowBtn_ = createButton("BottomUnstow", "司令室 復帰 ⬆", rightX, bottomY, 110, 32, {0.3f, 0.5f, 0.9f, 1.0f}, 14.0f, {1.0f, 1.0f, 1.0f, 1.0f});

        // 初期モード設定
        SwitchAppMode(AppMode::Fullscreen);

        // --- タブメニュー ---
        tabCommandBtn_ = createButton("TabCommand", "コマンド", 1320, 110, 170, 45, {0.95f, 0.95f, 0.95f, 1.0f}, 18.0f, {0.4f, 0.4f, 0.4f, 1.0f});
        tabShopBtn_ = createButton("TabShop", "ショップ", 1500, 110, 170, 45, {0.9f, 0.9f, 1.0f, 1.0f}, 18.0f, {0.0f, 0.0f, 0.0f, 1.0f}); // 初期選択用ダミー
        tabStatusBtn_ = createButton("TabStatus", "ステータス", 1680, 110, 170, 45, {0.95f, 0.95f, 0.95f, 1.0f}, 18.0f, {0.4f, 0.4f, 0.4f, 1.0f});
        tabActiveLine_ = createPanel("ActiveTabLine", 1320, 151, 170, 4, {0.2f, 0.4f, 1.0f, 1.0f}, 1);

        // --- ショップタブ ---
        shopTabEntities_.push_back(createText("ShopHeader", "キャラクター強化 (ATK・HP増加)", 1320, 180, 20.0f, {0.2f, 0.2f, 0.2f, 1.0f}));
        shopTabEntities_.push_back(createButton("BuyTank", "タンク Lv.1         ATK:10/HP:100", 1320, 220, 430, 45, {0.98f, 0.98f, 0.98f, 1.0f}, 16.0f));
        shopTabEntities_.push_back(createButton("Btn1", "100G", 1760, 222, 90, 40, {0.3f, 0.5f, 0.9f, 1.0f}, 18.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        shopTabEntities_.push_back(createButton("BuySniper", "スナイパー Lv.1  ATK:25/HP:60", 1320, 270, 430, 45, {1.0f, 1.0f, 1.0f, 1.0f}, 16.0f));
        shopTabEntities_.push_back(createButton("Btn2", "100G", 1760, 272, 90, 40, {0.3f, 0.5f, 0.9f, 1.0f}, 18.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        shopTabEntities_.push_back(createButton("BuyScout", "スカウト Lv.1      ATK:5/HP:80", 1320, 320, 430, 45, {0.98f, 0.98f, 0.98f, 1.0f}, 16.0f));
        shopTabEntities_.push_back(createButton("Btn3", "100G", 1760, 322, 90, 40, {0.3f, 0.5f, 0.9f, 1.0f}, 18.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        shopTabEntities_.push_back(createButton("BuyEngineer", "エンジニア Lv.1  ATK:8/HP:80", 1320, 370, 430, 45, {1.0f, 1.0f, 1.0f, 1.0f}, 16.0f));
        shopTabEntities_.push_back(createButton("Btn4", "100G", 1760, 372, 90, 40, {0.3f, 0.5f, 0.9f, 1.0f}, 18.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        shopTabEntities_.push_back(createText("RoomHeader", "聖域チャンバーの模様替え", 1320, 440, 20.0f, {0.2f, 0.2f, 0.2f, 1.0f}));
        shopTabEntities_.push_back(createButton("RoomNormal", "標準チャンバー                  [適用中]", 1320, 480, 530, 45, {0.7f, 1.0f, 0.7f, 1.0f}, 16.0f));
        shopTabEntities_.push_back(createButton("RoomCyber", "サイバー空間                      500 G", 1320, 530, 530, 45, {0.98f, 0.98f, 0.98f, 1.0f}, 16.0f));
        shopTabEntities_.push_back(createButton("RoomAntique", "アンティーク調                  1000 G", 1320, 580, 530, 45, {1.0f, 1.0f, 1.0f, 1.0f}, 16.0f));
        shopTabEntities_.push_back(createButton("RoomGold", "黄金の聖域                        5000 G", 1320, 630, 530, 45, {0.98f, 0.98f, 0.98f, 1.0f}, 16.0f));

        // --- ステータスタブ ---
        statusTabEntities_.push_back(createPanel("StatPanelTop", 1320, 180, 530, 40, {0.15f, 0.25f, 0.35f, 1.0f}, 0));
        statusTabEntities_.push_back(createText("StatHeader1", "過去の戦績・統計", 1340, 190, 18.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        statusTabEntities_.push_back(createPanel("StatPanelBG", 1320, 220, 530, 160, {0.95f, 0.96f, 0.98f, 1.0f}, 0));
        statusTabEntities_.push_back(createText("Stat1", "倒した敵の数", 1340, 240, 16.0f, {0.3f, 0.3f, 0.3f, 1.0f}));
        statusTabEntities_.push_back(createText("Stat1V", "19 体", 1750, 240, 16.0f, {0.1f, 0.1f, 0.1f, 1.0f}));
        statusTabEntities_.push_back(createText("Stat2", "総与ダメージ", 1340, 275, 16.0f, {0.3f, 0.3f, 0.3f, 1.0f}));
        statusTabEntities_.push_back(createText("Stat2V", "1,150 pts", 1750, 275, 16.0f, {0.1f, 0.1f, 0.1f, 1.0f}));
        statusTabEntities_.push_back(createText("Stat3", "総獲得ゴールド", 1340, 310, 16.0f, {0.3f, 0.3f, 0.3f, 1.0f}));
        statusTabEntities_.push_back(createText("Stat3V", "1,200 G", 1750, 310, 16.0f, {0.1f, 0.1f, 0.1f, 1.0f}));
        statusTabEntities_.push_back(createText("Stat4", "取得アイテム数", 1340, 345, 16.0f, {0.3f, 0.3f, 0.3f, 1.0f}));
        statusTabEntities_.push_back(createText("Stat4V", "0 個", 1750, 345, 16.0f, {0.1f, 0.1f, 0.1f, 1.0f}));
        
        statusTabEntities_.push_back(createText("StatHeader2", "パーティーステータス詳細", 1320, 410, 18.0f, {0.2f, 0.2f, 0.2f, 1.0f}));
        statusTabEntities_.push_back(createPanel("StatPartyHeaderBG", 1320, 440, 530, 30, {0.9f, 0.9f, 0.9f, 1.0f}, 0));
        statusTabEntities_.push_back(createText("SH_Char", "キャラ", 1340, 445, 14.0f, {0.4f, 0.4f, 0.4f, 1.0f}));
        statusTabEntities_.push_back(createText("SH_Lv", "Lv", 1520, 445, 14.0f, {0.4f, 0.4f, 0.4f, 1.0f}));
        statusTabEntities_.push_back(createText("SH_HP", "HP", 1580, 445, 14.0f, {0.4f, 0.4f, 0.4f, 1.0f}));
        statusTabEntities_.push_back(createText("SH_ATK", "ATK", 1680, 445, 14.0f, {0.4f, 0.4f, 0.4f, 1.0f}));
        statusTabEntities_.push_back(createText("SH_SPD", "SPD", 1760, 445, 14.0f, {0.4f, 0.4f, 0.4f, 1.0f}));
        
        auto createStatusRow = [&](int idx, const std::string& name, const std::string& lv, const std::string& hp, const std::string& atk, const std::string& spd) {
            float y = 475.0f + idx * 45.0f;
            statusTabEntities_.push_back(createPanel("RowBG", 1320, y, 530, 40, (idx % 2 == 0) ? DirectX::XMFLOAT4{1,1,1,1} : DirectX::XMFLOAT4{0.98f,0.98f,0.98f,1}, 0));
            statusTabEntities_.push_back(createText("R_Name" + std::to_string(idx), name, 1340, y + 10, 16.0f, {0.2f,0.2f,0.2f,1}));
            statusTabEntities_.push_back(createText("R_Lv" + std::to_string(idx), lv, 1520, y + 10, 16.0f, {0.2f,0.2f,0.2f,1}));
            statusTabEntities_.push_back(createText("R_HP" + std::to_string(idx), hp, 1580, y + 10, 16.0f, {0.2f,0.6f,0.2f,1}));
            statusTabEntities_.push_back(createText("R_ATK" + std::to_string(idx), atk, 1680, y + 10, 16.0f, {0.8f,0.2f,0.2f,1}));
            statusTabEntities_.push_back(createText("R_SPD" + std::to_string(idx), spd, 1760, y + 10, 16.0f, {0.2f,0.2f,0.8f,1}));
        };
        createStatusRow(0, "タンク", "1", "100/100", "10", "2.0");
        createStatusRow(1, "スナイパー", "1", "60/60", "25", "2.2");
        createStatusRow(2, "スカウト", "1", "80/80", "5", "4.0");
        createStatusRow(3, "エンジニア", "1", "80/80", "8", "1.5");

        // --- コマンドタブ ---
        commandTabEntities_.push_back(createPanel("CommandBG", 1320, 180, 530, 160, {0.95f, 0.96f, 0.98f, 1.0f}, 0));
        commandTabEntities_.push_back(createText("CommandHeaderG", "所持ゴールド:", 1340, 200, 16.0f, {0.4f, 0.4f, 0.4f, 1.0f}));
        commandTabEntities_.push_back(createText("CommandHeaderGV", "300 G", 1460, 198, 20.0f, {0.8f, 0.6f, 0.0f, 1.0f}));
        commandTabEntities_.push_back(createText("CommandHeaderE", "作業エネルギー:", 1620, 200, 16.0f, {0.4f, 0.4f, 0.4f, 1.0f}));
        commandTabEntities_.push_back(createText("CommandHeaderEV", "9/30", 1760, 200, 16.0f, {0.2f, 0.5f, 0.8f, 1.0f}));
        
        commandTabEntities_.push_back(createPanel("FocusModeBG", 1340, 235, 490, 30, {0.85f, 0.9f, 1.0f, 1.0f}, 1));
        commandTabEntities_.push_back(createText("FocusMode", "集中モード：防御＆ダメージUP中！", 1450, 240, 16.0f, {0.1f, 0.3f, 0.8f, 1.0f}));
        
        commandTabEntities_.push_back(createButton("ActionAttack", "突撃", 1340, 280, 150, 45, {1.0f, 0.7f, 0.7f, 1.0f}, 20.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        commandTabEntities_.push_back(createButton("ActionRecover", "回収", 1510, 280, 150, 45, {0.6f, 0.9f, 0.6f, 1.0f}, 20.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        commandTabEntities_.push_back(createButton("ActionFormat", "陣形", 1680, 280, 150, 45, {0.2f, 0.3f, 0.9f, 1.0f}, 20.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        
        // チャンバーの枠線（はっきり見えるように手前のレイヤーに描画）
        DirectX::XMFLOAT4 borderColor = {0.2f, 0.7f, 1.0f, 1.0f}; // 蛍光ブルー風
        commandTabEntities_.push_back(createPanel("ChamberBorderTop",    1316, 346, 538, 4, borderColor, 2));
        commandTabEntities_.push_back(createPanel("ChamberBorderBottom", 1316, 550, 538, 4, borderColor, 2));
        commandTabEntities_.push_back(createPanel("ChamberBorderLeft",   1316, 350, 4, 200, borderColor, 2));
        commandTabEntities_.push_back(createPanel("ChamberBorderRight",  1850, 350, 4, 200, borderColor, 2));
        commandTabEntities_.push_back(createPanel("ChamberBG", 1320, 350, 530, 200, {0.05f, 0.05f, 0.1f, 1.0f}, 0));
        
        // エネルギーコア (フィーバーシステム)
        coreEntity_ = createPanel("EnergyCore", 1320 + 265, 350 + 100, 64, 48, {1.0f, 1.0f, 1.0f, 1.0f}, 1);
        auto& coreImg = registry_.get<UIImageComponent>(coreEntity_);
        coreImg.texturePath = "Resources/Textures/white1x1.png"; // 四角
        coreImg.color = {1.0f, 0.3f, 0.5f, 1.0f};
        if (renderer_) coreImg.textureHandle = renderer_->LoadTexture2D(coreImg.texturePath);
        
        commandTabEntities_.push_back(coreEntity_);
        
        commandTabEntities_.push_back(createText("StatusInfo", "キャラ画像変更 (UGC)", 1320, 570, 16.0f, {0.2f, 0.2f, 0.2f, 1.0f}));
        commandTabEntities_.push_back(createButton("UGCSelectChar", "タンク", 1320, 600, 150, 35, {1.0f, 1.0f, 1.0f, 1.0f}, 16.0f, {0.0f, 0.0f, 0.0f, 1.0f}));
        commandTabEntities_.push_back(createButton("UGCFile", "ファイルを選択", 1490, 600, 120, 35, {0.9f, 0.9f, 0.9f, 1.0f}, 14.0f, {0.2f, 0.2f, 0.2f, 1.0f}));
        commandTabEntities_.push_back(createText("UGCNothing", "選択されていません", 1630, 610, 14.0f, {0.5f, 0.5f, 0.5f, 1.0f}));
        
        // ログ領域
        commandTabEntities_.push_back(createPanel("LogBG", 1320, 650, 530, 250, {0.95f, 0.95f, 0.95f, 1.0f}, 0));
        for (int i = 0; i < 9; i++) {
            auto logTxt = createText("LogText" + std::to_string(i), "", 1330.0f, 660.0f + static_cast<float>(i) * 25.0f, 14.0f, {0.2f, 0.2f, 0.2f, 1.0f});
            commandTabEntities_.push_back(logTxt);
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
        auto& btn = registry_.get<UIButtonComponent>(btnEnt);
        auto& rect = registry_.get<RectTransformComponent>(btnEnt);
        if (rect.enabled && btn.isHovered && context_.input->IsMouseTrigger(0)) {
            onClick();
        }
    };
    checkBtnClick(tabCommandBtn_, [&](){ SwitchTab(TabType::Command); });
    checkBtnClick(tabShopBtn_, [&](){ SwitchTab(TabType::Shop); });
    checkBtnClick(tabStatusBtn_, [&](){ SwitchTab(TabType::Status); });
    
    // AIモード変更ボタン
    auto nameView = registry_.view<NameComponent>();
    for (auto e : nameView) {
        auto& n = nameView.get<NameComponent>(e);
        if (n.name == "ActionAttack") {
            checkBtnClick(e, [&](){ 
                if (combatSys_) combatSys_->currentAiMode = PartyAIMode::Attack; 
                AddLog("> 司令：パーティーを突撃モードに変更");
            });
        }
        else if (n.name == "ActionRecover") {
            checkBtnClick(e, [&](){ 
                if (combatSys_) combatSys_->currentAiMode = PartyAIMode::Collect; 
                AddLog("> 司令：パーティーを回収モードに変更");
            });
        }
        else if (n.name == "ActionFormat") {
            checkBtnClick(e, [&](){ 
                if (combatSys_) combatSys_->currentAiMode = PartyAIMode::Formation; 
                AddLog("> 司令：パーティーを陣形モードに変更");
            });
        }
        else if (n.name == "Btn1" || n.name == "Btn2" || n.name == "Btn3" || n.name == "Btn4") {
            int heroIdx = (n.name == "Btn1") ? 0 : (n.name == "Btn2") ? 1 : (n.name == "Btn3") ? 2 : 3;
            std::string heroNames[] = {"タンク", "スナイパー", "スカウト", "エンジニア"};
            checkBtnClick(e, [&, heroIdx, heroNames](){ 
                if (combatSys_ && combatSys_->currentGold >= 100) {
                    combatSys_->currentGold -= 100;
                    combatSys_->heroLevels[heroIdx]++;
                    AddLog("> " + heroNames[heroIdx] + "を強化しました！ (Lv." + std::to_string(combatSys_->heroLevels[heroIdx]) + ")");
                } else {
                    AddLog("ゴールドが足りません！");
                }
            });
        }
        else if (n.name == "RoomNormal") {
            checkBtnClick(e, [&](){ 
                if (combatSys_) {
                    combatSys_->currentTheme = 0;
                    AddLog("> 司令室を標準チャンバーに変更しました。"); 
                }
            });
        }
        else if (n.name == "RoomCyber") {
            checkBtnClick(e, [&](){ 
                if (combatSys_) {
                    if (combatSys_->themeOwned[1]) {
                        combatSys_->currentTheme = 1;
                        AddLog("> 司令室をサイバー空間に変更しました！");
                    } else if (combatSys_->currentGold >= 500) {
                        combatSys_->currentGold -= 500;
                        combatSys_->themeOwned[1] = true;
                        combatSys_->currentTheme = 1;
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
                        AddLog("> 司令室をアンティーク調に変更しました！");
                    } else if (combatSys_->currentGold >= 1000) {
                        combatSys_->currentGold -= 1000;
                        combatSys_->themeOwned[2] = true;
                        combatSys_->currentTheme = 2;
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
                        AddLog("> 司令室を黄金の聖域に変更しました！");
                    } else if (combatSys_->currentGold >= 5000) {
                        combatSys_->currentGold -= 5000;
                        combatSys_->themeOwned[3] = true;
                        combatSys_->currentTheme = 3;
                        AddLog("> 司令室を黄金の聖域に変更しました！");
                    } else { AddLog("ゴールドが足りません！"); }
                }
            });
        }
        else if (n.name == "UGCSelectChar") {
            checkBtnClick(e, [&](){ 
                ugcTargetCharIndex_ = (ugcTargetCharIndex_ + 1) % 5;
                std::string names[] = {"タンク", "スナイパー", "スカウト", "エンジニア", "ヒーラー"};
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

                    std::string targetNames[] = {"Char_Tank", "Char_Sniper", "Char_Scout", "Char_Engineer", "Char_Healer"};
                    std::string tName = targetNames[ugcTargetCharIndex_];
                    
                    auto view = registry_.view<NameComponent, UIImageComponent>();
                    for (auto ent : view) {
                        if (view.get<NameComponent>(ent).name == tName) {
                            auto& img = view.get<UIImageComponent>(ent);
                            img.texturePath = utf8Path; // UTF-8形式でパスを保持
                            if (renderer_) {
                                img.textureHandle = renderer_->LoadTexture2D(img.texturePath, false);
                            }
                            
                            auto tView = registry_.view<NameComponent, UITextComponent>();
                            for (auto te : tView) {
                                if (tView.get<NameComponent>(te).name == "UGCNothing") {
                                    tView.get<UITextComponent>(te).text = shortName + " を適用";
                                }
                            }
                            
                            AddLog("> 画像を " + shortName + " に差し替えました");
                            break;
                        }
                    }
                }
            });
        }
        
        // UIテキストの動的更新 (ゴールドやステータスなど)
        if (combatSys_) {
            if (n.name == "CommandHeaderGV") {
                registry_.get<UITextComponent>(e).text = std::to_string(combatSys_->currentGold) + " G";
            } else if (n.name == "Gold") {
                registry_.get<UITextComponent>(e).text = "所持: " + std::to_string(combatSys_->currentGold) + " G";
            } else if (n.name == "Stat1V") {
                registry_.get<UITextComponent>(e).text = std::to_string(combatSys_->totalKills) + " 体";
            } else if (n.name == "Stat2V") {
                registry_.get<UITextComponent>(e).text = std::to_string((int)combatSys_->totalDamage) + " pts";
            } else if (n.name == "Stat3V") {
                registry_.get<UITextComponent>(e).text = std::to_string(combatSys_->totalGoldEarned) + " G";
            } else if (n.name == "Stat4V") {
                registry_.get<UITextComponent>(e).text = std::to_string(combatSys_->totalItems) + " 個";
            } else if (n.name == "BuyTank") {
                int lv = combatSys_->heroLevels[0];
                registry_.get<UITextComponent>(e).text = "タンク Lv." + std::to_string(lv) + "         ATK:" + std::to_string(10 + (lv-1)*2) + "/HP:" + std::to_string(100 + (lv-1)*20);
            } else if (n.name == "BuySniper") {
                int lv = combatSys_->heroLevels[1];
                registry_.get<UITextComponent>(e).text = "スナイパー Lv." + std::to_string(lv) + "  ATK:" + std::to_string(25 + (lv-1)*5) + "/HP:" + std::to_string(60 + (lv-1)*10);
            } else if (n.name == "BuyScout") {
                int lv = combatSys_->heroLevels[2];
                registry_.get<UITextComponent>(e).text = "スカウト Lv." + std::to_string(lv) + "      ATK:" + std::to_string(5 + (lv-1)*1) + "/HP:" + std::to_string(80 + (lv-1)*15);
            } else if (n.name == "BuyEngineer") {
                int lv = combatSys_->heroLevels[3];
                registry_.get<UITextComponent>(e).text = "エンジニア Lv." + std::to_string(lv) + "  ATK:" + std::to_string(8 + (lv-1)*1) + "/HP:" + std::to_string(80 + (lv-1)*15);
            } else if (n.name.find("R_Lv") == 0) {
                int idx = n.name[4] - '0';
                if (idx >= 0 && idx < 4) registry_.get<UITextComponent>(e).text = std::to_string(combatSys_->heroLevels[idx]);
            } else if (n.name.find("R_HP") == 0) {
                int idx = n.name[4] - '0';
                if (idx >= 0 && idx < 4) {
                    int lv = combatSys_->heroLevels[idx];
                    int hp = (idx==0) ? (100 + (lv-1)*20) : (idx==1) ? (60 + (lv-1)*10) : (idx==2) ? (80 + (lv-1)*15) : (80 + (lv-1)*15);
                    registry_.get<UITextComponent>(e).text = std::to_string(hp) + "/" + std::to_string(hp);
                }
            } else if (n.name.find("R_ATK") == 0) {
                int idx = n.name[5] - '0';
                if (idx >= 0 && idx < 4) {
                    int lv = combatSys_->heroLevels[idx];
                    int atk = (idx==0) ? (10 + (lv-1)*2) : (idx==1) ? (25 + (lv-1)*5) : (idx==2) ? (5 + (lv-1)*1) : (8 + (lv-1)*1);
                    registry_.get<UITextComponent>(e).text = std::to_string(atk);
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
    
    // コア・フィーバーの物理挙動
    if (currentTab_ == TabType::Command && registry_.valid(coreEntity_) && combatSys_) {
        float chamberX = 1320.0f;
        float chamberY = 350.0f;
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
    setTabBtn(tabShopBtn_, currentTab_ == TabType::Shop);
    setTabBtn(tabStatusBtn_, currentTab_ == TabType::Status);

    // Move active line
    if (registry_.valid(tabActiveLine_)) {
        auto& t = registry_.get<RectTransformComponent>(tabActiveLine_);
        if (currentTab_ == TabType::Command) t.pos.x = 1320;
        else if (currentTab_ == TabType::Shop) t.pos.x = 1500;
        else if (currentTab_ == TabType::Status) t.pos.x = 1680;
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
    setEntitiesActive(shopTabEntities_, currentTab_ == TabType::Shop);
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
    if (logMessages_.size() > 9) {
        logMessages_.erase(logMessages_.begin());
    }
    for (size_t i = 0; i < logTextEntities_.size(); i++) {
        if (i < logMessages_.size()) {
            if (registry_.valid(logTextEntities_[i])) {
                registry_.get<UITextComponent>(logTextEntities_[i]).text = logMessages_[i];
            }
        }
    }
}

} // namespace Game
