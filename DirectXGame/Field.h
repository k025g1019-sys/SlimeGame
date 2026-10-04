#pragma once
#include "KamataEngine.h"
#include <array>
#include <memory>
#include <vector>

struct GameModels;

/// <summary>
/// 足元の当たり判定の範囲（弧方向 × 高さ の2軸の箱）。
/// 3Dの距離ではなく、輪の内面に沿った「弧の差」と「高さ h」で比べる
/// </summary>
struct Footprint {
	float theta = 0.0f;     // 中心の角度（ラジアン）
	float halfWidth = 0.0f; // 弧方向の半幅（輪の内面に沿った長さ）
	float bottom = 0.0f;    // 高さの下端（0=接地）
	float top = 0.0f;       // 高さの上端
};

/// <summary>
/// フィールド（円形の輪が層になって奥へ続く筒）。輪の描画と、極座標のヘルパー（すべて static）を持つ。
///
/// 【座標系】左手系・+Y上・+Z奥。筒の軸＝Z軸。層 k（k=0..フェーズ数-1）の輪の中心 z = k × kLayerSpacing
/// 【極座標】θ（ラジアン。0=右(+X)、π/2=上、カメラから見て反時計回りに増える）と、
/// 　　　　　h（輪の内面から軸の方へ測った高さ。0=接地）。ワールド座標 = ((R−h)cosθ, (R−h)sinθ, z)
/// 　　　　　輪の内面に立つ物の向きは rotation_.z = θ + π/2（ローカル+Y が軸の方＝「上」、ローカル+X が θ の増える向き）
/// 【描画】現在の層から奥へ kVisibleLayers 層ぶんの輪を描く（奥ほど暗く。カメラより手前の層は描かない）。
/// 　　　　輪の帯＝箱を kRingSegments 個並べる。輪の前後の縁と、層の間の筒の縦線は PrimitiveDrawer の線（LineDrawer 経由）
/// </summary>
class Field {
public:
	// 輪1つあたりのセグメント（箱）の数の上限・描く層の数の上限・層の間の縦線の本数の上限
	// （線の本数の見積もりは LineDrawer.h。ここを増やすときは見積もりも直すこと）
	static constexpr int kMaxRingSegments = 64;
	static constexpr int kMaxVisibleLayers = 8;
	static constexpr int kMaxTubeLines = 32;
	// 判定の可視化（DrawFootprintLines）1つの線の本数
	static constexpr int kFootprintLineCount = 10;

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(const GameModels* models);

	/// <summary>
	/// 描く層を決め、輪のセグメントの行列・色を更新する
	/// </summary>
	/// <param name="cameraZ">カメラの z（これより手前の層は描かない）</param>
	/// <param name="focusZ">カメラが見ている層の z（明るさの基準。移動中は補間中の値）</param>
	/// <param name="layerCount">層の総数（＝フェーズ数）</param>
	void Update(float cameraZ, float focusZ, int layerCount);

	/// <summary>
	/// 輪の描画（不透明パスの中で呼ぶ）
	/// </summary>
	void Draw(const KamataEngine::Camera& camera) const;

	/// <summary>
	/// 輪の縁と筒の縦線（LineDrawer::BeginFrame の後、Model の PreDraw～PostDraw の外で呼ぶ）
	/// </summary>
	void DrawLines() const;

	// --- 極座標のヘルパー ---

	// 角度を (−π, π] に収める
	static float WrapAngle(float angle);
	// θfrom から θto への弧の長さ（符号付き。+＝θが増える向き。短い側を通る）
	static float ArcDistance(float thetaFrom, float thetaTo);
	// θfrom から見て θto が短い弧で行ける向き（+1 / −1。真反対や同じ位置は −1）
	static int DirectionTo(float thetaFrom, float thetaTo);
	// 極座標 → ワールド座標
	static KamataEngine::Vector3 PolarToWorld(float theta, float height, float z);
	// 輪の内面に立つ物の Z 回転（ローカル+Y が軸の方を向く）
	static float SurfaceRotationZ(float theta);
	// θ の増える向き（接線方向の単位ベクトル。XY平面）
	static KamataEngine::Vector3 Tangent(float theta);
	// 層 k の輪の中心の z
	static float LayerZ(int layer);
	// 足元の判定どうしが重なっているか（弧の差と高さの2軸）
	static bool Overlaps(const Footprint& a, const Footprint& b);
	// 円（中心 θ・高さ h・半径。爆風など）と足元の判定が重なっているか
	static bool CircleOverlaps(float theta, float height, float radius, const Footprint& footprint);
	// 足元の判定を輪の上に線で描く（判定の可視化。kFootprintLineCount 本使う）
	static void DrawFootprintLines(const Footprint& footprint, float z, const KamataEngine::Vector4& color);
	// ライトの設定を反映する（毎フレーム。点光源はカメラに付いて動く）
	static void UpdateLightGroup(KamataEngine::LightGroup* lightGroup, const KamataEngine::Vector3& cameraPosition);

	// 調整項目を登録（既定値をGlobalVariablesへ追加する）
	static void RegisterGlobalVariables();
	// 調整項目を適用（GlobalVariablesの現在値を静的メンバへ反映する）
	static void ApplyGlobalVariables();

	// --- 調整項目（GlobalVariables の "Field" グループ） ---
	// 輪の半径（内面までの距離）・層の間隔（z）・輪の帯の幅（z方向）・輪の厚み（半径方向）
	static inline float kRingRadius = 10.0f;
	static inline float kLayerSpacing = 40.0f;
	static inline float kRingWidth = 6.0f;
	static inline float kRingThickness = 1.2f;
	// 輪1つのセグメント（箱）の数（偶数に丸める。8～64）・セグメントの隙間（1=隙間なし）
	static inline int32_t kRingSegments = 48;
	static inline float kSegmentFill = 0.94f;
	// 描く層の数（1～8）・層の間の筒の縦線の本数（0～32）
	static inline int32_t kVisibleLayers = 6;
	static inline int32_t kTubeLines = 16;
	// 輪の色（交互の2色）・縁の線の色・筒の縦線の色（RGB）
	static inline KamataEngine::Vector3 kRingColor = {0.05f, 0.09f, 0.2f};
	static inline KamataEngine::Vector3 kRingColorAlt = {0.03f, 0.055f, 0.13f};
	static inline KamataEngine::Vector3 kEdgeColor = {0.35f, 0.7f, 1.0f};
	static inline KamataEngine::Vector3 kTubeColor = {0.15f, 0.3f, 0.55f};
	// 奥の層ほど暗くする割合（1層ごと）と、明るさの下限
	static inline float kLayerDim = 0.16f;
	static inline float kMinBrightness = 0.2f;
	// 背景色（画面全体に敷く板の色）
	static inline KamataEngine::Vector3 kBackgroundColor = {0.003f, 0.004f, 0.012f};
	// ライト：環境光・主光源（光の進む向きと色）・補助光・カメラに付いた点光源（色・強さ・届く距離・減衰・カメラからの前方距離）
	static inline KamataEngine::Vector3 kAmbientColor = {1.0f, 1.0f, 1.0f};
	static inline KamataEngine::Vector3 kKeyLightDirection = {-0.3f, -0.5f, 1.0f};
	static inline KamataEngine::Vector3 kKeyLightColor = {0.6f, 0.6f, 0.6f};
	static inline KamataEngine::Vector3 kFillLightDirection = {0.4f, 0.6f, 0.5f};
	static inline KamataEngine::Vector3 kFillLightColor = {0.2f, 0.2f, 0.25f};
	static inline KamataEngine::Vector3 kHeadlampColor = {1.0f, 0.95f, 0.85f};
	static inline float kHeadlampIntensity = 0.6f;
	static inline float kHeadlampRadius = 90.0f;
	static inline float kHeadlampDecay = 1.0f;
	static inline float kHeadlampForward = 6.0f;

private:
	// 描く層1つ分の情報
	struct VisibleLayer {
		int layer = 0;
		float z = 0.0f;
		float brightness = 1.0f;
	};

	// 必要な数までセグメントの WorldTransform を増やす（足りないときだけ作る。上限は kMaxVisibleLayers × kMaxRingSegments）
	void EnsureSegmentCapacity(int count);

	const GameModels* models_ = nullptr;
	// 輪のセグメントの行列（描く箱1つにつき1つ。WorldTransform はコピー・ムーブ不可なので unique_ptr で持つ）
	std::vector<std::unique_ptr<KamataEngine::WorldTransform>> segmentTransforms_;
	// 輪の色（層ごとに交互の2色。同じ色の箱は同じ ObjectColor を共有してよい＝値が同じなので最後の値で描かれても問題ない）
	std::array<KamataEngine::ObjectColor, kMaxVisibleLayers * 2> layerColors_;
	// 今フレーム描く層
	std::array<VisibleLayer, kMaxVisibleLayers> visibleLayers_{};
	int visibleCount_ = 0;
	// 今フレームの1輪あたりのセグメント数（調整項目を制限した値）
	int segmentCount_ = 0;
};
