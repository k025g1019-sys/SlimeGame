#include "Sprite.hlsli"

SamplerState smp : register(s0);

float4 main(VSOutput input) : SV_TARGET {
    // テクスチャ番号はスプライトごとに異なる（=wave内で不均一になり得る）ため、
    // NonUniformResourceIndex を付けないとGPUによっては境界ピクセルが別のテクスチャを
    // 誤サンプリングし、画面にゴミ（他テクスチャの欠片）が出る
    Texture2D tex = ResourceDescriptorHeap[NonUniformResourceIndex(input.textureDescriptorIndex)];
    return tex.Sample(smp, input.uv) * input.color;
}
