#pragma pack_matrix(row_major)

#include "Space.hlsli"

TextureCube gCubeMap : register(t0);
SamplerState gSampler : register(s0);

cbuffer CBFrame : register(b0) {
    matrix gView;
    matrix gProj;
    matrix gViewProj;
    float3 gCameraPos;
    float  gTime;
    float4 gWindParams;
    float3 gPlayerPos;
    uint   gUseCubemapBackground;
};

struct PSIn {
    float4 svpos : SV_POSITION;
    float3 texCoord : TEXCOORD0;
};

float4 main(PSIn p) : SV_TARGET {
    float3 dir = normalize(p.texCoord);
    
    if (gUseCubemapBackground != 0) {
        // Cubemapから景色をサンプリング
        float3 color = gCubeMap.Sample(gSampler, dir).rgb;
        return float4(0.0, 0.0, 0.0, 0.0); // ★強制的に透明な黒を返す
    } else {
        // ★修正: 透過デスクトップアプリのため、背景を使わない場合は「完全に透明な黒」を出力する
        // これにより、このピクセルはOS(DWM)側でデスクトップ画面として透過表示される
        return float4(0.0, 0.0, 0.0, 0.0);
    }
}
