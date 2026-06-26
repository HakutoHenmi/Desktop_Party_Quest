#include "MainScene.h"
#include "../../Engine/Audio.h"
#include "../../Engine/Input.h"
#include "../../Engine/Renderer.h"
#include "../../Engine/SceneManager.h"
#include "../../Engine/WindowDX.h"

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

    // Initialize Systems
    auto uiSystem = std::make_unique<UISystem>();
    uiSystem->Reset(registry_);
    systems_.push_back(std::move(uiSystem));

    auto workEnergySystem = std::make_unique<WorkEnergySystem>();
    workEnergySystem->Reset(registry_);
    systems_.push_back(std::move(workEnergySystem));

    // ★追加: 画面に表示するオブジェクトが何もない状態だったため、床とテストキャラクター（スライム等）を生成する
    {
/*
        // 床
        auto floor = registry_.create();
        registry_.emplace<NameComponent>(floor, "Floor");
        auto& tFloor = registry_.emplace<TransformComponent>(floor);
        tFloor.translate = {0, -1.0f, 0};
        tFloor.scale = {20.0f, 1.0f, 20.0f};
        auto& mrFloor = registry_.emplace<MeshRendererComponent>(floor);
        mrFloor.modelPath = "Resources/Models/plane.obj"; // 実在するモデル
        mrFloor.color = {0.3f, 0.5f, 0.3f, 1.0f};
        mrFloor.shaderName = "Default";

        // キャラクター（スライム代わり）
        auto player = registry_.create();
        registry_.emplace<NameComponent>(player, "Player");
        auto& tPlayer = registry_.emplace<TransformComponent>(player);
        tPlayer.translate = {0, 1.0f, 0};
        tPlayer.scale = {1.0f, 1.0f, 1.0f};
        auto& mrPlayer = registry_.emplace<MeshRendererComponent>(player);
        mrPlayer.modelPath = "Resources/Models/plane.obj"; // 実在するモデル(立ててキャラ代わりにする)
        tPlayer.rotate = {DirectX::XM_PIDIV2, 0, 0};
        mrPlayer.color = {0.2f, 0.6f, 1.0f, 1.0f};
        mrPlayer.shaderName = "Default";
*/
        
        // プレイヤーとして動かせるようにタグと入力コンポーネントを追加
/*
        registry_.emplace<TagComponent>(player, TagType::Player);
        registry_.emplace<PlayerInputComponent>(player);
        registry_.emplace<CharacterMovementComponent>(player);
        registry_.emplace<RigidbodyComponent>(player);
        
        // カメラがプレイヤーを追従するようにする
        auto& camTarget = registry_.emplace<CameraTargetComponent>(player);
        camTarget.distance = 15.0f;
        camTarget.height = 8.0f;
*/
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
        
        // ★追加: 格納ボタン
        stowButton_ = createButton("StowBtn", "⬇下へ格納", 1680, 65, 100, 30, {0.9f, 0.9f, 0.9f, 1.0f}, 16.0f);
        stowRightBtn_ = createButton("StowRightBtn", "➡右へ格納", 1790, 65, 100, 30, {0.9f, 0.9f, 0.9f, 1.0f}, 16.0f);

        // ★追加: タスクバー（下部）格納モード用UI（初期は非表示）
        bottomBarPanel_ = createPanel("BottomBar", 0, 1032, 1920, 48, {0.1f, 0.1f, 0.15f, 0.95f}, 0);
        bottomBarText_ = createText("BottomText", "Desktop Party Quest  |  作業エネルギー収集モード...", 20, 1045, 16.0f, {1.0f, 1.0f, 1.0f, 1.0f});
        bottomUnstowBtn_ = createButton("BottomUnstow", "復帰 ⬆", 1800, 1040, 100, 32, {0.3f, 0.5f, 0.9f, 1.0f}, 16.0f, {1.0f, 1.0f, 1.0f, 1.0f});

        // ★追加: タスクバー（右部）格納モード用UI（初期は非表示）
        rightBarPanel_ = createPanel("RightBar", 1856, 0, 64, 1080, {0.1f, 0.1f, 0.15f, 0.95f}, 0);
        rightBarText_ = createText("RightText", "D\nP\nQ", 1878, 80, 20.0f, {1.0f, 1.0f, 1.0f, 1.0f});
        rightUnstowBtn_ = createButton("RightUnstow", "復帰\n⬅", 1860, 20, 56, 40, {0.3f, 0.5f, 0.9f, 1.0f}, 14.0f, {1.0f, 1.0f, 1.0f, 1.0f});

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
            statusTabEntities_.push_back(createText("R_Name", name, 1340, y + 10, 16.0f, {0.2f,0.2f,0.2f,1}));
            statusTabEntities_.push_back(createText("R_Lv", lv, 1520, y + 10, 16.0f, {0.2f,0.2f,0.2f,1}));
            statusTabEntities_.push_back(createText("R_HP", hp, 1580, y + 10, 16.0f, {0.2f,0.6f,0.2f,1}));
            statusTabEntities_.push_back(createText("R_ATK", atk, 1680, y + 10, 16.0f, {0.8f,0.2f,0.2f,1}));
            statusTabEntities_.push_back(createText("R_SPD", spd, 1760, y + 10, 16.0f, {0.2f,0.2f,0.8f,1}));
        };
        createStatusRow(0, "タンク", "1", "100/100", "10", "2.0");
        createStatusRow(1, "スナイパー", "1", "60/60", "25", "2.2");
        createStatusRow(2, "スカウト", "1", "80/80", "5", "4.0");
        createStatusRow(3, "エンジニア", "1", "80/80", "8", "1.5");

        // --- コマンドタブ ---
        commandTabEntities_.push_back(createPanel("CommandBG", 1320, 180, 530, 160, {0.95f, 0.96f, 0.98f, 1.0f}, 0));
        commandTabEntities_.push_back(createText("CommandHeaderG", "獲得ゴールド:", 1340, 200, 16.0f, {0.4f, 0.4f, 0.4f, 1.0f}));
        commandTabEntities_.push_back(createText("CommandHeaderGV", "300 G", 1460, 198, 20.0f, {0.8f, 0.6f, 0.0f, 1.0f}));
        commandTabEntities_.push_back(createText("CommandHeaderE", "作業エネルギー:", 1620, 200, 16.0f, {0.4f, 0.4f, 0.4f, 1.0f}));
        commandTabEntities_.push_back(createText("CommandHeaderEV", "9/30", 1760, 200, 16.0f, {0.2f, 0.5f, 0.8f, 1.0f}));
        
        commandTabEntities_.push_back(createPanel("FocusModeBG", 1340, 235, 490, 30, {0.85f, 0.9f, 1.0f, 1.0f}, 1));
        commandTabEntities_.push_back(createText("FocusMode", "集中モード：防御＆ダメージUP中！", 1450, 240, 16.0f, {0.1f, 0.3f, 0.8f, 1.0f}));
        
        commandTabEntities_.push_back(createButton("ActionAttack", "突撃", 1340, 280, 150, 45, {1.0f, 0.7f, 0.7f, 1.0f}, 20.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        commandTabEntities_.push_back(createButton("ActionRecover", "回収", 1510, 280, 150, 45, {0.6f, 0.9f, 0.6f, 1.0f}, 20.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        commandTabEntities_.push_back(createButton("ActionFormat", "陣形", 1680, 280, 150, 45, {0.2f, 0.3f, 0.9f, 1.0f}, 20.0f, {1.0f, 1.0f, 1.0f, 1.0f}));
        
        commandTabEntities_.push_back(createPanel("ChamberBG", 1320, 350, 530, 200, {0.05f, 0.05f, 0.1f, 1.0f}, 0));
        commandTabEntities_.push_back(createText("ChamberText", "エネルギーチャンバー (準備中)", 1460, 440, 16.0f, {0.3f, 0.3f, 0.4f, 1.0f}));
        
        commandTabEntities_.push_back(createText("StatusInfo", "キャラ画像変更 (UGC)", 1320, 570, 16.0f, {0.2f, 0.2f, 0.2f, 1.0f}));
        commandTabEntities_.push_back(createButton("UGCSelectChar", "タンク", 1320, 600, 150, 35, {1.0f, 1.0f, 1.0f, 1.0f}, 16.0f, {0.0f, 0.0f, 0.0f, 1.0f}));
        commandTabEntities_.push_back(createButton("UGCFile", "ファイルを選択", 1490, 600, 120, 35, {0.9f, 0.9f, 0.9f, 1.0f}, 14.0f, {0.2f, 0.2f, 0.2f, 1.0f}));
        commandTabEntities_.push_back(createText("UGCNothing", "選択されていません", 1630, 610, 14.0f, {0.5f, 0.5f, 0.5f, 1.0f}));
        
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
    
    // 格納ボタンクリック判定
    checkBtnClick(stowButton_, [&](){ SwitchAppMode(AppMode::TaskbarBottom); });
    checkBtnClick(stowRightBtn_, [&](){ SwitchAppMode(AppMode::TaskbarRight); });
    checkBtnClick(bottomUnstowBtn_, [&](){ SwitchAppMode(AppMode::Fullscreen); });
    checkBtnClick(rightUnstowBtn_, [&](){ SwitchAppMode(AppMode::Fullscreen); });

    // 格納中のタスクバー位置・サイズの動的更新（OSのタスクバー変化に追従）
    // 格納中のタスクバー位置・サイズの動的更新（OSのタスクバー変化に追従）
    if (appMode_ == AppMode::TaskbarBottom) {
        RECT appBarRect = Engine::WindowDX::GetAppBarRect();
        float bx = static_cast<float>(appBarRect.left);
        float by = static_cast<float>(appBarRect.top);
        float bw = static_cast<float>(appBarRect.right - appBarRect.left);

        // フォールバック（万が一AppBar登録に失敗した場合でも画面内に強制表示して「復帰」ボタンを押せるようにする）
        if (bw <= 0.0f) {
            bx = 0.0f;
            bw = static_cast<float>(Engine::WindowDX::kW);
            by = static_cast<float>(Engine::WindowDX::kH) - 48.0f;
        }

        if (registry_.valid(bottomBarPanel_)) {
            auto& t = registry_.get<RectTransformComponent>(bottomBarPanel_);
            t.pos.x = bx;
            t.pos.y = by;
            t.size.x = bw;
        }
        if (registry_.valid(bottomBarText_)) {
            registry_.get<RectTransformComponent>(bottomBarText_).pos.x = bx + 20.0f;
            registry_.get<RectTransformComponent>(bottomBarText_).pos.y = by + 13.0f;
        }
        if (registry_.valid(bottomUnstowBtn_)) {
            registry_.get<RectTransformComponent>(bottomUnstowBtn_).pos.x = bx + bw - 120.0f;
            registry_.get<RectTransformComponent>(bottomUnstowBtn_).pos.y = by + 8.0f;
        }
    } else if (appMode_ == AppMode::TaskbarRight) {
        RECT appBarRect = Engine::WindowDX::GetAppBarRect();
        float bx = static_cast<float>(appBarRect.left);
        float by = static_cast<float>(appBarRect.top);
        float bh = static_cast<float>(appBarRect.bottom - appBarRect.top);

        // フォールバック
        if (bh <= 0.0f) {
            bx = static_cast<float>(Engine::WindowDX::kW) - 64.0f;
            by = 0.0f;
            bh = static_cast<float>(Engine::WindowDX::kH);
        }

        if (registry_.valid(rightBarPanel_)) {
            auto& t = registry_.get<RectTransformComponent>(rightBarPanel_);
            t.pos.x = bx;
            t.pos.y = by;
            t.size.y = bh;
        }
        if (registry_.valid(rightBarText_)) {
            registry_.get<RectTransformComponent>(rightBarText_).pos.x = bx + 22.0f;
            registry_.get<RectTransformComponent>(rightBarText_).pos.y = by + 80.0f;
        }
        if (registry_.valid(rightUnstowBtn_)) {
            registry_.get<RectTransformComponent>(rightUnstowBtn_).pos.x = bx + 4.0f;
            registry_.get<RectTransformComponent>(rightUnstowBtn_).pos.y = by + 20.0f;
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
            bool isBarEntity = (e == bottomBarPanel_ || e == bottomBarText_ || e == bottomUnstowBtn_ ||
                                e == rightBarPanel_ || e == rightBarText_ || e == rightUnstowBtn_);
            
            registry_.get<RectTransformComponent>(e).enabled = (mode == AppMode::Fullscreen) ? !isBarEntity : isBarEntity;
            
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
        Engine::WindowDX::SetAppBarMode(0);
        SwitchTab(currentTab_); // タブ表示状態を復元
    } else {
        // 格納モードの場合は、指定されたバーのみ表示
        if (mode == AppMode::TaskbarBottom) {
            Engine::WindowDX::SetAppBarMode(1);
            if (registry_.valid(bottomBarPanel_)) registry_.get<RectTransformComponent>(bottomBarPanel_).enabled = true;
            if (registry_.valid(bottomBarText_)) registry_.get<RectTransformComponent>(bottomBarText_).enabled = true;
            if (registry_.valid(bottomUnstowBtn_)) registry_.get<RectTransformComponent>(bottomUnstowBtn_).enabled = true;
            
            if (registry_.valid(rightBarPanel_)) registry_.get<RectTransformComponent>(rightBarPanel_).enabled = false;
            if (registry_.valid(rightBarText_)) registry_.get<RectTransformComponent>(rightBarText_).enabled = false;
            if (registry_.valid(rightUnstowBtn_)) registry_.get<RectTransformComponent>(rightUnstowBtn_).enabled = false;
        } else if (mode == AppMode::TaskbarRight) {
            Engine::WindowDX::SetAppBarMode(2);
            if (registry_.valid(rightBarPanel_)) registry_.get<RectTransformComponent>(rightBarPanel_).enabled = true;
            if (registry_.valid(rightBarText_)) registry_.get<RectTransformComponent>(rightBarText_).enabled = true;
            if (registry_.valid(rightUnstowBtn_)) registry_.get<RectTransformComponent>(rightUnstowBtn_).enabled = true;
            
            if (registry_.valid(bottomBarPanel_)) registry_.get<RectTransformComponent>(bottomBarPanel_).enabled = false;
            if (registry_.valid(bottomBarText_)) registry_.get<RectTransformComponent>(bottomBarText_).enabled = false;
            if (registry_.valid(bottomUnstowBtn_)) registry_.get<RectTransformComponent>(bottomUnstowBtn_).enabled = false;
        }
    }
}

} // namespace Game
