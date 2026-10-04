#define NOMINMAX
#include "Field.h"
#include "GameModels.h"
#include "GlobalVariables.h"
#include "LineDrawer.h"
#include "MathUtil.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace KamataEngine;

namespace {
// 調整項目のグループ名
const char* kGroupName = "Field";
constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTwoPi = 2.0f * kPi;
// カメラのすぐ前（この距離より手前）にある層は描かない（カメラが輪の中を通り抜けるときに視界を覆わないように）
const float kNearClipMargin = 1.0f;
// 縁の線を内面より少しだけ内側に描く（箱の面と重なってちらつかないように）
const float kEdgeInset = 0.03f;
// 判定の可視化の弧の分割数（1つの判定で 弧4本×2 + 縦2本 = 10本）
const int kFootprintArcSegments = 4;
static_assert(kFootprintArcSegments * 2 + 2 == Field::kFootprintLineCount, "判定の可視化の線の本数を変えたら Field::kFootprintLineCount も直す");
} // namespace

void Field::Initialize(const GameModels* models) {
	models_ = models;
	for (ObjectColor& color : layerColors_) {
		color.Initialize();
	}
	// 既定の設定で描く分だけ先に作っておく（調整で増やしたときは Update で足りない分を作る）
	EnsureSegmentCapacity(std::clamp(kVisibleLayers, 1, kMaxVisibleLayers) * std::clamp(kRingSegments, 8, kMaxRingSegments));
}

void Field::EnsureSegmentCapacity(int count) {
	while (static_cast<int>(segmentTransforms_.size()) < count) {
		auto worldTransform = std::make_unique<WorldTransform>();
		worldTransform->Initialize();
		segmentTransforms_.push_back(std::move(worldTransform));
	}
}

void Field::Update(float cameraZ, float focusZ, int layerCount) {
	// 調整項目は ApplyGlobalVariables で範囲を制限済み
	segmentCount_ = kRingSegments;
	const int maxLayers = kVisibleLayers;

	// カメラより奥にある層を、手前から順に maxLayers 層ぶん選ぶ
	visibleCount_ = 0;
	for (int layer = 0; layer < layerCount && visibleCount_ < maxLayers; layer++) {
		const float z = LayerZ(layer);
		if (z + kRingWidth * 0.5f < cameraZ + kNearClipMargin) {
			continue; // クリア済み（カメラより手前）の層は描かない
		}
		VisibleLayer& visible = visibleLayers_[visibleCount_];
		visible.layer = layer;
		visible.z = z;
		// 見ている層からの距離（層の数）で暗くする
		const float layerDistance = std::max((z - focusZ) / kLayerSpacing, 0.0f);
		visible.brightness = std::clamp(1.0f - layerDistance * kLayerDim, kMinBrightness, 1.0f);
		visibleCount_++;
	}

	EnsureSegmentCapacity(visibleCount_ * segmentCount_);

	// セグメントの箱：内面が半径 R に来るよう、箱の中心は R + 厚み/2 に置く
	const float centerRadius = kRingRadius + kRingThickness * 0.5f;
	const float segmentLength = kTwoPi * centerRadius / static_cast<float>(segmentCount_) * kSegmentFill;
	for (int i = 0; i < visibleCount_; i++) {
		const VisibleLayer& visible = visibleLayers_[i];
		for (int j = 0; j < segmentCount_; j++) {
			WorldTransform& worldTransform = *segmentTransforms_[static_cast<size_t>(i * segmentCount_ + j)];
			const float angle = kTwoPi * static_cast<float>(j) / static_cast<float>(segmentCount_);
			worldTransform.translation_ = {std::cos(angle) * centerRadius, std::sin(angle) * centerRadius, visible.z};
			worldTransform.rotation_ = {0.0f, 0.0f, SurfaceRotationZ(angle)};
			worldTransform.scale_ = {segmentLength, kRingThickness, kRingWidth};
			UpdateWorldTransform(worldTransform);
		}
		// 層ごとの交互の2色（奥ほど暗い）
		const float b = visible.brightness;
		layerColors_[static_cast<size_t>(i * 2)].SetColor({kRingColor.x * b, kRingColor.y * b, kRingColor.z * b, 1.0f});
		layerColors_[static_cast<size_t>(i * 2 + 1)].SetColor({kRingColorAlt.x * b, kRingColorAlt.y * b, kRingColorAlt.z * b, 1.0f});
	}
}

void Field::Draw(const Camera& camera) const {
	for (int i = 0; i < visibleCount_; i++) {
		for (int j = 0; j < segmentCount_; j++) {
			const WorldTransform& worldTransform = *segmentTransforms_[static_cast<size_t>(i * segmentCount_ + j)];
			models_->box->Draw(worldTransform, camera, &layerColors_[static_cast<size_t>(i * 2 + (j % 2))]);
		}
	}
}

void Field::DrawLines() const {
	const float edgeRadius = kRingRadius - kEdgeInset;
	const int tubeLines = kTubeLines;
	for (int i = 0; i < visibleCount_; i++) {
		const VisibleLayer& visible = visibleLayers_[i];
		const float b = visible.brightness;
		const Vector4 edgeColor = {kEdgeColor.x * b, kEdgeColor.y * b, kEdgeColor.z * b, 1.0f};
		const float frontZ = visible.z - kRingWidth * 0.5f;
		const float backZ = visible.z + kRingWidth * 0.5f;
		// 輪の前後の縁（セグメントと同じ分割数の多角形）
		for (int j = 0; j < segmentCount_; j++) {
			const float a0 = kTwoPi * static_cast<float>(j) / static_cast<float>(segmentCount_);
			const float a1 = kTwoPi * static_cast<float>(j + 1) / static_cast<float>(segmentCount_);
			const Vector3 p0 = {std::cos(a0) * edgeRadius, std::sin(a0) * edgeRadius, 0.0f};
			const Vector3 p1 = {std::cos(a1) * edgeRadius, std::sin(a1) * edgeRadius, 0.0f};
			LineDrawer::Draw({p0.x, p0.y, frontZ}, {p1.x, p1.y, frontZ}, edgeColor);
			LineDrawer::Draw({p0.x, p0.y, backZ}, {p1.x, p1.y, backZ}, edgeColor);
		}
		// 次の層との間の筒の縦線（奥の層の明るさで描く）
		if (i + 1 < visibleCount_ && tubeLines > 0) {
			const VisibleLayer& next = visibleLayers_[i + 1];
			const float nb = next.brightness;
			const Vector4 tubeColor = {kTubeColor.x * nb, kTubeColor.y * nb, kTubeColor.z * nb, 1.0f};
			const float tubeRadius = kRingRadius + kRingThickness * 0.5f;
			for (int t = 0; t < tubeLines; t++) {
				const float angle = kTwoPi * (static_cast<float>(t) + 0.5f) / static_cast<float>(tubeLines);
				const float x = std::cos(angle) * tubeRadius;
				const float y = std::sin(angle) * tubeRadius;
				LineDrawer::Draw({x, y, backZ}, {x, y, next.z - kRingWidth * 0.5f}, tubeColor);
			}
		}
	}
}

float Field::WrapAngle(float angle) {
	// (−π, π] に収める（fmod は負の値で負を返すので、0～2π に直してからずらす）
	float wrapped = std::fmod(angle + kPi, kTwoPi);
	if (wrapped <= 0.0f) {
		wrapped += kTwoPi;
	}
	return wrapped - kPi;
}

float Field::ArcDistance(float thetaFrom, float thetaTo) { return WrapAngle(thetaTo - thetaFrom) * kRingRadius; }

int Field::DirectionTo(float thetaFrom, float thetaTo) {
	const float difference = WrapAngle(thetaTo - thetaFrom);
	return (difference > 0.0f && difference < kPi) ? 1 : -1;
}

Vector3 Field::PolarToWorld(float theta, float height, float z) {
	const float radius = kRingRadius - height;
	return {std::cos(theta) * radius, std::sin(theta) * radius, z};
}

float Field::SurfaceRotationZ(float theta) { return theta + kPi * 0.5f; }

Vector3 Field::Tangent(float theta) { return {-std::sin(theta), std::cos(theta), 0.0f}; }

float Field::LayerZ(int layer) { return static_cast<float>(layer) * kLayerSpacing; }

bool Field::Overlaps(const Footprint& a, const Footprint& b) {
	const float arc = std::abs(ArcDistance(a.theta, b.theta));
	if (arc >= a.halfWidth + b.halfWidth) {
		return false;
	}
	return a.bottom < b.top && b.bottom < a.top;
}

bool Field::CircleOverlaps(float theta, float height, float radius, const Footprint& footprint) {
	// 箱（弧×高さ）の外側にはみ出した分の距離で判定する（円と矩形の当たり判定）
	const float arc = std::abs(ArcDistance(theta, footprint.theta));
	const float dx = std::max(arc - footprint.halfWidth, 0.0f);
	float dy = 0.0f;
	if (height < footprint.bottom) {
		dy = footprint.bottom - height;
	} else if (height > footprint.top) {
		dy = height - footprint.top;
	}
	return dx * dx + dy * dy <= radius * radius;
}

void Field::DrawFootprintLines(const Footprint& footprint, float z, const Vector4& color) {
	// 手前の縁に描く（物の手前に出るように）
	const float lineZ = z - kRingWidth * 0.5f;
	const float halfAngle = footprint.halfWidth / kRingRadius;
	const float startTheta = footprint.theta - halfAngle;
	for (int i = 0; i < kFootprintArcSegments; i++) {
		const float t0 = startTheta + halfAngle * 2.0f * static_cast<float>(i) / static_cast<float>(kFootprintArcSegments);
		const float t1 = startTheta + halfAngle * 2.0f * static_cast<float>(i + 1) / static_cast<float>(kFootprintArcSegments);
		LineDrawer::Draw(PolarToWorld(t0, footprint.bottom, lineZ), PolarToWorld(t1, footprint.bottom, lineZ), color);
		LineDrawer::Draw(PolarToWorld(t0, footprint.top, lineZ), PolarToWorld(t1, footprint.top, lineZ), color);
	}
	const float endTheta = footprint.theta + halfAngle;
	LineDrawer::Draw(PolarToWorld(startTheta, footprint.bottom, lineZ), PolarToWorld(startTheta, footprint.top, lineZ), color);
	LineDrawer::Draw(PolarToWorld(endTheta, footprint.bottom, lineZ), PolarToWorld(endTheta, footprint.top, lineZ), color);
}

void Field::UpdateLightGroup(LightGroup* lightGroup, const Vector3& cameraPosition) {
	if (!lightGroup) {
		return;
	}
	lightGroup->SetAmbientColor(kAmbientColor);
	// 平行光源（SetDirLightDir は「光の進む向き」）。0=主光源（カメラ側の上から奥へ）、1=補助光、2=使わない
	lightGroup->SetDirLightActive(0, true);
	lightGroup->SetDirLightDir(0, Normalize(kKeyLightDirection));
	lightGroup->SetDirLightColor(0, kKeyLightColor);
	lightGroup->SetDirLightActive(1, true);
	lightGroup->SetDirLightDir(1, Normalize(kFillLightDirection));
	lightGroup->SetDirLightColor(1, kFillLightColor);
	lightGroup->SetDirLightActive(2, false);
	// 点光源（カメラの少し前に置く「ヘッドライト」。筒の内側の面は向きがばらばらなので、平行光源だけだと暗い面ができる）
	lightGroup->SetPointLightActive(0, kHeadlampIntensity > 0.0f);
	lightGroup->SetPointLightPos(0, cameraPosition + Vector3{0.0f, 0.0f, kHeadlampForward});
	lightGroup->SetPointLightColor(0, kHeadlampColor);
	lightGroup->SetPointLightIntensity(0, kHeadlampIntensity);
	lightGroup->SetPointLightRadius(0, kHeadlampRadius);
	lightGroup->SetPointLightDecay(0, kHeadlampDecay);
	lightGroup->SetPointLightActive(1, false);
	lightGroup->SetPointLightActive(2, false);
	// 変更を定数バッファへ転送する
	lightGroup->Update();
}

void Field::RegisterGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	globalVariables->CreateGroup(kGroupName);
	globalVariables->AddItem(kGroupName, "RingRadius", kRingRadius);
	globalVariables->AddItem(kGroupName, "LayerSpacing", kLayerSpacing);
	globalVariables->AddItem(kGroupName, "RingWidth", kRingWidth);
	globalVariables->AddItem(kGroupName, "RingThickness", kRingThickness);
	globalVariables->AddItem(kGroupName, "RingSegments", kRingSegments);
	globalVariables->AddItem(kGroupName, "SegmentFill", kSegmentFill);
	globalVariables->AddItem(kGroupName, "VisibleLayers", kVisibleLayers);
	globalVariables->AddItem(kGroupName, "TubeLines", kTubeLines);
	globalVariables->AddItem(kGroupName, "RingColor", kRingColor);
	globalVariables->AddItem(kGroupName, "RingColorAlt", kRingColorAlt);
	globalVariables->AddItem(kGroupName, "EdgeColor", kEdgeColor);
	globalVariables->AddItem(kGroupName, "TubeColor", kTubeColor);
	globalVariables->AddItem(kGroupName, "LayerDim", kLayerDim);
	globalVariables->AddItem(kGroupName, "MinBrightness", kMinBrightness);
	globalVariables->AddItem(kGroupName, "BackgroundColor", kBackgroundColor);
	globalVariables->AddItem(kGroupName, "AmbientColor", kAmbientColor);
	globalVariables->AddItem(kGroupName, "KeyLightDirection", kKeyLightDirection);
	globalVariables->AddItem(kGroupName, "KeyLightColor", kKeyLightColor);
	globalVariables->AddItem(kGroupName, "FillLightDirection", kFillLightDirection);
	globalVariables->AddItem(kGroupName, "FillLightColor", kFillLightColor);
	globalVariables->AddItem(kGroupName, "HeadlampColor", kHeadlampColor);
	globalVariables->AddItem(kGroupName, "HeadlampIntensity", kHeadlampIntensity);
	globalVariables->AddItem(kGroupName, "HeadlampRadius", kHeadlampRadius);
	globalVariables->AddItem(kGroupName, "HeadlampDecay", kHeadlampDecay);
	globalVariables->AddItem(kGroupName, "HeadlampForward", kHeadlampForward);
}

void Field::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	// 半径・間隔・幅・厚みは0以下だと割り算や判定が壊れるので下限を付ける
	kRingRadius = std::max(globalVariables->GetFloatValue(kGroupName, "RingRadius"), 1.0f);
	kLayerSpacing = std::max(globalVariables->GetFloatValue(kGroupName, "LayerSpacing"), 1.0f);
	kRingWidth = std::max(globalVariables->GetFloatValue(kGroupName, "RingWidth"), 0.1f);
	kRingThickness = std::max(globalVariables->GetFloatValue(kGroupName, "RingThickness"), 0.05f);
	// セグメント数は交互の色がつながるよう偶数にそろえ、上限（線の本数の見積もり）で止める
	const int32_t segments = std::clamp(globalVariables->GetIntValue(kGroupName, "RingSegments"), 8, kMaxRingSegments);
	kRingSegments = segments + (segments % 2);
	kSegmentFill = std::clamp(globalVariables->GetFloatValue(kGroupName, "SegmentFill"), 0.1f, 1.0f);
	kVisibleLayers = std::clamp(globalVariables->GetIntValue(kGroupName, "VisibleLayers"), 1, kMaxVisibleLayers);
	kTubeLines = std::clamp(globalVariables->GetIntValue(kGroupName, "TubeLines"), 0, kMaxTubeLines);
	kRingColor = globalVariables->GetVector3Value(kGroupName, "RingColor");
	kRingColorAlt = globalVariables->GetVector3Value(kGroupName, "RingColorAlt");
	kEdgeColor = globalVariables->GetVector3Value(kGroupName, "EdgeColor");
	kTubeColor = globalVariables->GetVector3Value(kGroupName, "TubeColor");
	kLayerDim = std::max(globalVariables->GetFloatValue(kGroupName, "LayerDim"), 0.0f);
	kMinBrightness = std::clamp(globalVariables->GetFloatValue(kGroupName, "MinBrightness"), 0.0f, 1.0f);
	kBackgroundColor = globalVariables->GetVector3Value(kGroupName, "BackgroundColor");
	kAmbientColor = globalVariables->GetVector3Value(kGroupName, "AmbientColor");
	kKeyLightDirection = globalVariables->GetVector3Value(kGroupName, "KeyLightDirection");
	kKeyLightColor = globalVariables->GetVector3Value(kGroupName, "KeyLightColor");
	kFillLightDirection = globalVariables->GetVector3Value(kGroupName, "FillLightDirection");
	kFillLightColor = globalVariables->GetVector3Value(kGroupName, "FillLightColor");
	kHeadlampColor = globalVariables->GetVector3Value(kGroupName, "HeadlampColor");
	kHeadlampIntensity = std::max(globalVariables->GetFloatValue(kGroupName, "HeadlampIntensity"), 0.0f);
	kHeadlampRadius = std::max(globalVariables->GetFloatValue(kGroupName, "HeadlampRadius"), 0.1f);
	kHeadlampDecay = std::max(globalVariables->GetFloatValue(kGroupName, "HeadlampDecay"), 0.0f);
	kHeadlampForward = globalVariables->GetFloatValue(kGroupName, "HeadlampForward");
}
