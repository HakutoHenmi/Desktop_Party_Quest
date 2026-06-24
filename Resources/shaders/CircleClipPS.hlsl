Texture2D tex : register(t0);
SamplerState smp : register(s0);

cbuffer Params : register(b0) {
    float isHit;    // 1.0 if hit, 0.0 otherwise
    float time;     // Time for shake effect
    float2 padding;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET {
    // 1. Calculate UV coordinates for circular clipping
    float2 centeredUV = input.uv - 0.5f;
    float distance = length(centeredUV);

    // Discard pixels outside the circle
    if (distance > 0.5f) {
        discard;
    }

    // 2. Shake effect logic
    float2 sampleUV = input.uv;
    if (isHit > 0.5f) {
        // Simple horizontal shake
        sampleUV.x += sin(time * 50.0f) * 0.05f;
    }

    // Sample the texture
    float4 color = tex.Sample(smp, sampleUV);

    // 3. Red flash effect logic
    if (isHit > 0.5f) {
        color.r = min(1.0f, color.r + 0.5f); // Increase red channel
    }

    // Anti-aliasing the edge
    float alphaEdge = smoothstep(0.5f, 0.48f, distance);
    color.a *= alphaEdge;

    return color;
}
