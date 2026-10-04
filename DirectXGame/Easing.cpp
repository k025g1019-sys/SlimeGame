#include "Easing.h"

float EaseInOut(float t) {
	// 0.0f ～ 1.0f を想定
	return t * t * (3.0f - 2.0f * t);
}

float EaseOutCubic(float t) {
	// 0.0f ～ 1.0f を想定。最初が速く、終わり際が遅い
	const float u = 1.0f - t;
	return 1.0f - u * u * u;
}

float EaseOutBack(float t) {
	// 0.0f ～ 1.0f を想定。終わり際に約10%行き過ぎてから1へ戻る
	const float overshoot = 1.70158f;
	const float u = t - 1.0f;
	return 1.0f + (overshoot + 1.0f) * u * u * u + overshoot * u * u;
}
