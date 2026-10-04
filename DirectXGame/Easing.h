#pragma once

/// <summary>
/// イージング関数集（前作から流用）。引数 t は 0.0f〜1.0f を想定
/// </summary>

// ゆっくり始まり、ゆっくり終わる（スムーズステップ）
float EaseInOut(float t);
// 最初が速く、終わり際が遅い（メニューの開閉など、素早く出てふわっと止まる動きに使う）
float EaseOutCubic(float t);
// 少し行き過ぎてから戻る（出現時に「ぽんっ」と膨らむ動きに使う。途中で1を少し超える）
float EaseOutBack(float t);
