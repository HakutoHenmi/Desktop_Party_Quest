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

    // ヒーローはDesktopCombatSystemが自動生成するようになりました。

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
        shopTabEntities_.push_back(createButton("BuyTank", "重装クロスボウ Lv.1    ATK:10/HP:100", 1320, 220, 430, 45, {0.98f, 0.98f, 0.98f, 1.0f}, 16.0f));
        shopTabEntities_.push_back(createButton("Btn1", "100G", 1760, 222, 90, 40, {0.3f, 0.5f, 0.9f, 1.0f}, 18.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        shopTabEntities_.push_back(createButton("BuySniper", "ロングボウ Lv.1      ATK:25/HP:60", 1320, 270, 430, 45, {1.0f, 1.0f, 1.0f, 1.0f}, 16.0f));
        shopTabEntities_.push_back(createButton("Btn2", "100G", 1760, 272, 90, 40, {0.3f, 0.5f, 0.9f, 1.0f}, 18.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        shopTabEntities_.push_back(createButton("BuyScout", "連射クロスボウ Lv.1    ATK:5/HP:80", 1320, 320, 430, 45, {0.98f, 0.98f, 0.98f, 1.0f}, 16.0f));
        shopTabEntities_.push_back(createButton("Btn3", "100G", 1760, 322, 90, 40, {0.3f, 0.5f, 0.9f, 1.0f}, 18.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        shopTabEntities_.push_back(createButton("BuyEngineer", "癒やしの矢 Lv.1      ATK:8/HP:80", 1320, 370, 430, 45, {1.0f, 1.0f, 1.0f, 1.0f}, 16.0f));
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
            statusTabEntities_.push_back(createText("R_Lv" + std::to_string(idx), lv, 1480, y + 10, 16.0f, {0.2f,0.2f,0.2f,1}));
            statusTabEntities_.push_back(createText("R_HP" + std::to_string(idx), hp, 1530, y + 10, 16.0f, {0.2f,0.6f,0.2f,1}));
            statusTabEntities_.push_back(createText("R_ATK" + std::to_string(idx), atk, 1620, y + 10, 16.0f, {0.8f,0.2f,0.2f,1}));
            statusTabEntities_.push_back(createText("R_SPD" + std::to_string(idx), spd, 1680, y + 10, 16.0f, {0.2f,0.2f,0.8f,1}));
            statusTabEntities_.push_back(createButton("R_Equip" + std::to_string(idx), "装備変更", 1750, y + 5, 90, 30, {0.9f, 0.9f, 0.9f, 1.0f}, 14.0f));
        };
        createStatusRow(0, "重装クロスボウ", "1", "100/100", "10", "2.0");
        createStatusRow(1, "ロングボウ", "1", "60/60", "25", "2.2");
        createStatusRow(2, "連射クロスボウ", "1", "80/80", "5", "4.0");
        createStatusRow(3, "癒やしの矢", "1", "80/80", "8", "1.5");
        createStatusRow(4, "支援枠", "1", "90/90", "5", "2.5");
        
        // --- 遺物図鑑ボタン ---
        statusTabEntities_.push_back(createButton("RelicEncyclopediaBtn", "インターネット遺物図鑑を開く", 1320, 715, 530, 45, {0.9f, 0.8f, 1.0f, 1.0f}, 18.0f, {0.2f, 0.0f, 0.4f, 1.0f}));

        // --- コマンドタブ ---
        commandTabEntities_.push_back(createPanel("CommandBG", 1320, 180, 530, 200, {0.95f, 0.96f, 0.98f, 1.0f}, 0));
        commandTabEntities_.push_back(createText("CommandHeaderG", "所持ゴールド:", 1330, 200, 16.0f, {0.4f, 0.4f, 0.4f, 1.0f}));
        commandTabEntities_.push_back(createText("CommandHeaderGV", "300 G", 1430, 198, 20.0f, {0.8f, 0.6f, 0.0f, 1.0f}));
        commandTabEntities_.push_back(createText("CommandHeaderE", "雇用枠:", 1540, 200, 16.0f, {0.4f, 0.4f, 0.4f, 1.0f}));
        commandTabEntities_.push_back(createText("CommandHeaderEV", "4/4", 1600, 200, 16.0f, {0.2f, 0.5f, 0.8f, 1.0f}));
        commandTabEntities_.push_back(createText("CommandHeaderWE", "エネルギー:", 1660, 200, 16.0f, {0.4f, 0.4f, 0.4f, 1.0f}));
        commandTabEntities_.push_back(createText("CommandHeaderWEV", "0 / 100", 1750, 200, 16.0f, {0.8f, 0.2f, 0.5f, 1.0f}));
        
        commandTabEntities_.push_back(createButton("HireTank", "重装クロスボウ雇用(100G)", 1340, 235, 230, 35, {1.0f, 0.7f, 0.7f, 1.0f}, 15.0f));
        commandTabEntities_.push_back(createButton("HireSniper", "ロングボウ雇用(150G)", 1590, 235, 230, 35, {1.0f, 0.9f, 0.6f, 1.0f}, 15.0f));
        commandTabEntities_.push_back(createButton("HireScout", "連射クロスボウ雇用(50G)", 1340, 280, 230, 35, {0.6f, 0.9f, 0.6f, 1.0f}, 15.0f));
        commandTabEntities_.push_back(createButton("HireHealer", "癒やしの矢雇用(200G)", 1590, 280, 230, 35, {1.0f, 0.7f, 0.9f, 1.0f}, 15.0f));
        
        commandTabEntities_.push_back(createButton("ExpandLimit", "雇用枠を拡張する (500G)", 1340, 325, 480, 40, {0.2f, 0.4f, 0.9f, 1.0f}, 18.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        
        // チャンバーの枠線（はっきり見えるように手前のレイヤーに描画）
        DirectX::XMFLOAT4 borderColor = {0.2f, 0.7f, 1.0f, 1.0f}; // 蛍光ブルー風
        commandTabEntities_.push_back(createPanel("ChamberBorderTop",    1316, 386, 538, 4, borderColor, 2));
        commandTabEntities_.push_back(createPanel("ChamberBorderBottom", 1316, 590, 538, 4, borderColor, 2));
        commandTabEntities_.push_back(createPanel("ChamberBorderLeft",   1316, 390, 4, 200, borderColor, 2));
        commandTabEntities_.push_back(createPanel("ChamberBorderRight",  1850, 390, 4, 200, borderColor, 2));
        commandTabEntities_.push_back(createPanel("ChamberBG", 1320, 390, 530, 200, {0.05f, 0.05f, 0.1f, 1.0f}, 0));
        
        // エネルギーコア (フィーバーシステム)
        coreEntity_ = createPanel("EnergyCore", 1320 + 265, 390 + 100, 64, 48, {1.0f, 1.0f, 1.0f, 1.0f}, 1);
        auto& coreImg = registry_.get<UIImageComponent>(coreEntity_);
        coreImg.texturePath = "Resources/Textures/white1x1.png"; // 四角
        coreImg.color = {1.0f, 0.3f, 0.5f, 1.0f};
        if (renderer_) coreImg.textureHandle = renderer_->LoadTexture2D(coreImg.texturePath);
        
        commandTabEntities_.push_back(coreEntity_);
        
        commandTabEntities_.push_back(createText("StatusInfo", "キャラ画像変更 (UGC)", 1320, 610, 16.0f, {0.2f, 0.2f, 0.2f, 1.0f}));
        commandTabEntities_.push_back(createButton("UGCSelectChar", "タンク", 1320, 640, 150, 35, {1.0f, 1.0f, 1.0f, 1.0f}, 16.0f, {0.0f, 0.0f, 0.0f, 1.0f}));
        commandTabEntities_.push_back(createButton("UGCFile", "ファイルを選択", 1490, 640, 120, 35, {0.9f, 0.9f, 0.9f, 1.0f}, 14.0f, {0.2f, 0.2f, 0.2f, 1.0f}));
        commandTabEntities_.push_back(createText("UGCNothing", "選択されていません", 1630, 650, 14.0f, {0.5f, 0.5f, 0.5f, 1.0f}));
        
        // --- ガチャ要素 ---
        commandTabEntities_.push_back(createPanel("GachaBG", 1320, 695, 530, 45, {0.8f, 0.9f, 0.8f, 1.0f}, 0));
        commandTabEntities_.push_back(createText("GachaHeader", "謎のデータ断片: 0個", 1340, 705, 18.0f, {0.1f, 0.3f, 0.1f, 1.0f}));
        commandTabEntities_.push_back(createButton("GachaBtn", "データ解析 (ガチャ) - 10断片", 1600, 700, 240, 35, {0.2f, 0.8f, 0.2f, 1.0f}, 16.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        
        // ログ領域
        commandTabEntities_.push_back(createPanel("LogBG", 1320, 750, 530, 150, {0.95f, 0.95f, 0.95f, 1.0f}, 0));
        for (int i = 0; i < 6; i++) {
            auto logTxt = createText("LogText" + std::to_string(i), "", 1330.0f, 760.0f + static_cast<float>(i) * 24.0f, 14.0f, {0.2f, 0.2f, 0.2f, 1.0f});
            commandTabEntities_.push_back(logTxt);
            logTextEntities_.push_back(logTxt);
        }
        
        // --- プレステージ（転生）ボタン ---
        commandTabEntities_.push_back(createButton("PrestigeBtn", "システム再起動 (プレステージ) - レベル初期化＆永続ボーナス獲得", 1320, 910, 530, 40, {1.0f, 0.4f, 0.4f, 1.0f}, 16.0f, {1.0f, 1.0f, 1.0f, 1.0f}));

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
    
    bool requestClosePopup = false;
    bool requestShowRelicPopup = false;
    bool requestShowPrestigePopup = false;
    int requestShowEquipPopupIdx = -1;
    int requestEquipId = -1;
    int requestEquipRoleIdx = -1;

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
        else if (n.name == "RelicEncyclopediaBtn") {
            checkBtnClick(e, [&](){ requestShowRelicPopup = true; });
        }
        else if (n.name.find("R_Equip") == 0) {
            int idx = n.name[7] - '0';
            checkBtnClick(e, [&, idx](){ requestShowEquipPopupIdx = idx; currentEquipRoleIdx_ = idx; equipPopupPage_ = 0; });
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
        else if (n.name == "EquipPrevPageBtn") {
            checkBtnClick(e, [&](){
                equipPopupPage_--;
                requestShowEquipPopupIdx = currentEquipRoleIdx_;
            });
        }
        else if (n.name == "EquipNextPageBtn") {
            checkBtnClick(e, [&](){
                equipPopupPage_++;
                requestShowEquipPopupIdx = currentEquipRoleIdx_;
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
                        combatSys_->prestigeCount++;
                        combatSys_->prestigePoints++;
                        for (int i=0; i<4; i++) combatSys_->heroLevels[i] = 1;
                        combatSys_->currentGold = 300.0;
                        combatSys_->currentStage = 1;
                        combatSys_->enemiesKilledInStage = 0;
                        combatSys_->enemiesSpawnedInStage = 0;
                        combatSys_->SaveProgress(registry_);
                        AddLog("> [転生] レベルが初期化され、スキルポイントを1獲得！");
                        requestShowPrestigePopup = true; 
                    } else {
                        AddLog("転生するには、いずれかのキャラがLv.20以上必要です。");
                    }
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
            } else if (n.name == "CommandHeaderGV") {
                registry_.get<UITextComponent>(e).text = DesktopCombatSystem::FormatNum(combatSys_->currentGold) + " G";
            } else if (n.name == "Gold") {
                registry_.get<UITextComponent>(e).text = "STAGE " + std::to_string(combatSys_->currentStage) + " | 所持: " + DesktopCombatSystem::FormatNum(combatSys_->currentGold) + " G";
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
                    combatSys_->GetHeroStats(registry_, idx, hp, maxHp, atk, spd);
                    registry_.get<UITextComponent>(e).text = DesktopCombatSystem::FormatNum(hp) + "/" + DesktopCombatSystem::FormatNum(maxHp);
                }
            } else if (n.name.find("R_ATK") == 0) {
                int idx = n.name[5] - '0';
                if (idx >= 0 && idx < 5) {
                    float hp, maxHp, atk, spd;
                    combatSys_->GetHeroStats(registry_, idx, hp, maxHp, atk, spd);
                    registry_.get<UITextComponent>(e).text = DesktopCombatSystem::FormatNum(atk);
                }
            } else if (n.name.find("R_SPD") == 0) {
                int idx = n.name[5] - '0';
                if (idx >= 0 && idx < 5) {
                    float hp, maxHp, atk, spd;
                    combatSys_->GetHeroStats(registry_, idx, hp, maxHp, atk, spd);
                    char buf[32]; snprintf(buf, sizeof(buf), "%.1f", spd);
                    registry_.get<UITextComponent>(e).text = buf;
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
    if (requestShowEquipPopupIdx != -1) {
        ShowEquipPopup(requestShowEquipPopupIdx);
    }
    if (requestEquipId != -1 && requestEquipRoleIdx != -1) {
        auto viewProg = registry_.view<DesktopPartyProgressComponent>();
        if (!viewProg.empty()) {
            auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
            prog.equippedIds[requestEquipRoleIdx] = requestEquipId;
            AddLog("> 装備を変更しました！");
            if (combatSys_) combatSys_->SaveProgress(registry_);
            ShowEquipPopup(requestEquipRoleIdx); // 再描画して反映
        }
    }
    
    // コア・フィーバーの物理挙動
    if (currentTab_ == TabType::Command && registry_.valid(coreEntity_) && combatSys_) {
        float chamberX = 1320.0f;
        float chamberY = 390.0f;
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
    
    std::string roleNames[] = {"タンク", "スナイパー", "スカウト", "エンジニア", "ヒーラー"};
    popupEntities_.push_back(createText("PopupTitle", roleNames[roleIdx] + "の装備変更", 1320, 220, 24.0f, {0.1f, 0.1f, 0.1f, 1.0f}));
    
    popupEntities_.push_back(createButton("ClosePopupBtn", "閉じる", 1750, 220, 100, 40, {0.8f, 0.2f, 0.2f, 1.0f}, 16.0f, {1,1,1,1}));
    
    auto viewProg = registry_.view<DesktopPartyProgressComponent>();
    if (!viewProg.empty()) {
        auto& prog = registry_.get<DesktopPartyProgressComponent>(viewProg.front());
        int equippedId = prog.equippedIds[roleIdx];
        
        popupEntities_.push_back(createText("EquipCurrent", "現在装備中:", 1320, 280, 18.0f, {0.3f, 0.3f, 0.3f, 1.0f}));
        if (equippedId == -1) {
            popupEntities_.push_back(createText("EquipCurrentName", "なし", 1450, 280, 18.0f, {0.5f, 0.5f, 0.5f, 1.0f}));
        } else {
            for (const auto& eq : prog.inventory) {
                if (eq.id == static_cast<uint32_t>(equippedId)) {
                    popupEntities_.push_back(createText("EquipCurrentName", eq.name, 1450, 280, 18.0f, eq.GetRarityColor()));
                    break;
                }
            }
        }
        
        popupEntities_.push_back(createText("EquipInv", "インベントリ:", 1320, 330, 18.0f, {0.3f, 0.3f, 0.3f, 1.0f}));
        
        int itemsPerPage = 5;
        int maxPage = (int)(prog.inventory.size() - 1) / itemsPerPage;
        if (prog.inventory.empty()) maxPage = 0;
        if (equipPopupPage_ > maxPage) equipPopupPage_ = maxPage;
        if (equipPopupPage_ < 0) equipPopupPage_ = 0;
        
        popupEntities_.push_back(createText("EquipPageTxt", std::to_string(equipPopupPage_ + 1) + " / " + std::to_string(maxPage + 1), 1550, 330, 16.0f, {0.3f, 0.3f, 0.3f, 1.0f}));
        if (equipPopupPage_ > 0) {
            popupEntities_.push_back(createButton("EquipPrevPageBtn", "◀", 1500, 325, 30, 30, {0.8f, 0.8f, 0.8f, 1.0f}, 14.0f, {0,0,0,1}));
        }
        if (equipPopupPage_ < maxPage) {
            popupEntities_.push_back(createButton("EquipNextPageBtn", "▶", 1620, 325, 30, 30, {0.8f, 0.8f, 0.8f, 1.0f}, 14.0f, {0,0,0,1}));
        }
        
        float currentY = 370.0f;
        int startIndex = equipPopupPage_ * itemsPerPage;
        for (int i = startIndex; i < startIndex + itemsPerPage && i < prog.inventory.size(); ++i) {
            const auto& eq = prog.inventory[i];
            popupEntities_.push_back(createPanel("InvItemBG", 1320, currentY, 530, 40, {1.0f, 1.0f, 1.0f, 1.0f}, 4));
            popupEntities_.push_back(createText("InvItemName", eq.name, 1340, currentY + 10, 16.0f, eq.GetRarityColor()));
            popupEntities_.push_back(createText("InvItemStats", "ATK+" + std::to_string((int)(eq.atkMul*100)) + "% HP+" + std::to_string((int)(eq.hpMul*100)) + "%", 1550, currentY + 10, 14.0f, {0.4f, 0.4f, 0.4f, 1.0f}));
            popupEntities_.push_back(createButton("EquipBtn_" + std::to_string(eq.id) + "_" + std::to_string(roleIdx), "装備", 1750, currentY + 5, 80, 30, {0.8f, 0.8f, 0.8f, 1.0f}, 14.0f, {0.1f, 0.1f, 0.1f, 1.0f}));
            currentY += 45.0f;
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

} // namespace Game
