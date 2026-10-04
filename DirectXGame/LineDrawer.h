#pragma once
#include "KamataEngine.h"

/// <summary>
/// PrimitiveDrawer（3Dの線）を、1フレームに描いた本数を数えながら使う小さなラッパー。
/// PrimitiveDrawer は1フレーム4096本までで、上限のチェックが無い（超えるとメモリを壊す）。
/// ゲームの線はすべてここを通し、kLineBudget を超えた分は描かずに捨てる（本数は Game Debug に表示）。
///
/// 【1フレームの線の本数の見積もり（最大値。調整項目は Apply で、数は定数で上限を付けている）】
/// 　輪の縁（前後）     : 描く層 8（Field::kMaxVisibleLayers）× 2 × セグメント 64（Field::kMaxRingSegments）= 1024
/// 　層の間の縦線       : 層の間 7 × 32（Field::kMaxTubeLines）= 224
/// 　着弾予告マーカー   : 24個（GameScene::kMaxMarkersPerFrame。空中の弾がもっと多くても描くのはここまで）× 44本（Projectile::kMarkerLineCount）= 1056
/// 　判定の可視化(Debug): (スライム1 + 敵32（GameScene::kMaxEnemies）+ 弾64（GameScene::kMaxProjectiles）) × 10本（Field::kFootprintLineCount）= 970
/// 　　（空中の弾は判定を描かずマーカーだけなので、実際のマーカー＋判定はこれより少ない）
/// 　合計 3274 本以下 < kLineBudget(4000) < 4096。GameScene.cpp の static_assert で同じ計算をしている（定数を増やして超えるとコンパイルが止まる）
/// 使い方：線を描く前に BeginFrame(&camera)（Model の PreDraw～PostDraw の外で）→ Draw を何度でも
/// </summary>
class LineDrawer {
public:
	// 1フレームに描く線の上限（PrimitiveDrawer の上限4096より少し余裕を持たせる）
	static constexpr int kLineBudget = 4000;

	// 線を描き始める（カメラを渡し、本数の数え直しを始める。PrimitiveDrawer 側の Reset は main が毎フレーム行う）
	static void BeginFrame(const KamataEngine::Camera* camera) {
		KamataEngine::PrimitiveDrawer::GetInstance()->SetCamera(camera);
		sCount = 0;
		sDropped = 0;
	}

	// 線を1本描く（上限を超えたら描かずに数だけ数える）
	static void Draw(const KamataEngine::Vector3& start, const KamataEngine::Vector3& end, const KamataEngine::Vector4& color) {
		if (sCount >= kLineBudget) {
			sDropped++;
			return;
		}
		KamataEngine::PrimitiveDrawer::GetInstance()->DrawLine3d(start, end, color);
		sCount++;
	}

	// このフレームに描いた本数・上限で捨てた本数（デバッグ表示用）
	static int GetCount() { return sCount; }
	static int GetDroppedCount() { return sDropped; }

private:
	static inline int sCount = 0;
	static inline int sDropped = 0;
};
