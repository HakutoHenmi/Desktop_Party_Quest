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
        auto title = registry_.create();
        registry_.emplace<NameComponent>(title, "UITitle");
        auto& tTitle = registry_.emplace<TransformComponent>(title);
        // 右下付近 (画面サイズ1920x1080を想定して適当に調整)
        tTitle.translate = { 1400.0f, 600.0f, 0.0f }; 
        auto& titleText = registry_.emplace<UITextComponent>(title);
        titleText.text = "【 司令室 】\n所持: 1200 G\n\n聖域チャンバーの模様替え\n・標準チャンバー [適用中]\n・サイバー空間 [購入: 500G]\n・アンティーク調 [購入: 1000G]\n・黄金の聖域 [購入: 5000G]";
        titleText.fontSize = 24.0f;
        titleText.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        titleText.fontPath = "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf";
    }

    // ★追加: 適当なスプライトを描画する
    {
        auto spriteEnt = registry_.create();
        registry_.emplace<NameComponent>(spriteEnt, "TestSprite");
        auto& tSprite = registry_.emplace<RectTransformComponent>(spriteEnt);
        tSprite.pos.x = 100.0f;
        tSprite.pos.y = 100.0f;
        tSprite.size.x = 256.0f;
        tSprite.size.y = 256.0f;
        tSprite.rotation = 0.0f;

        auto& img = registry_.emplace<UIImageComponent>(spriteEnt);
        img.texturePath = "Resources/Textures/uvChecker.png";
        img.textureHandle = renderer_->LoadTexture2D(img.texturePath);
        img.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        img.layer = 0;
    }
}

void MainScene::Update() {
    auto now = std::chrono::steady_clock::now();
    dt_ = std::chrono::duration<float>(now - lastTime_).count();
    lastTime_ = now;
    if (dt_ > 0.1f)
        dt_ = 1.0f / 60.0f;
    totalTime_ += dt_;

    camera_.Tick(dt_);
    if (renderer_) {
        renderer_->SetCamera(camera_);
    }

    // Update Systems
    for (auto& sys : systems_) {
        sys->Update(registry_, context_);
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

} // namespace Game
