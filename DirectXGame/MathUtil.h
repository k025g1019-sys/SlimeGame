#pragma once
#include "GameConfig.h"
#include "KamataEngine.h"
#include "Matrix4x4.h"
#include <cmath>

/// <summary>
/// Vector2 / Vector3 の演算子・数学ヘルパー集と、WorldTransform の行列更新ヘルパー。
/// エンジンの Vector2 / Vector3 は素の構造体で演算子を持たないため、ここで一括定義する。
/// 物理・当たり判定のコードを「v = a + b * t」のように数式どおり書けるようにするのが目的。
/// 座標系：3Dは左手系・+Yが上・カメラ既定の向きは+Z（奥）。2D（スプライト）はピクセル単位・左上原点・下が+y
/// ※KamataEngine::MathUtility 名前空間には定義しないこと（エンジンのlibと重複してLNK2005になる）
/// </summary>
namespace KamataEngine {

// --- Vector2 の演算子 ---

inline Vector2 operator+(const Vector2& a, const Vector2& b) { return {a.x + b.x, a.y + b.y}; }
inline Vector2 operator-(const Vector2& a, const Vector2& b) { return {a.x - b.x, a.y - b.y}; }
inline Vector2 operator-(const Vector2& v) { return {-v.x, -v.y}; }
inline Vector2 operator*(const Vector2& v, float s) { return {v.x * s, v.y * s}; }
inline Vector2 operator*(float s, const Vector2& v) { return v * s; }
inline Vector2 operator/(const Vector2& v, float s) { return {v.x / s, v.y / s}; }

inline Vector2& operator+=(Vector2& a, const Vector2& b) {
	a.x += b.x;
	a.y += b.y;
	return a;
}

inline Vector2& operator-=(Vector2& a, const Vector2& b) {
	a.x -= b.x;
	a.y -= b.y;
	return a;
}

inline Vector2& operator*=(Vector2& v, float s) {
	v.x *= s;
	v.y *= s;
	return v;
}

// --- Vector3 の演算子 ---

inline Vector3 operator+(const Vector3& a, const Vector3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vector3 operator-(const Vector3& a, const Vector3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vector3 operator-(const Vector3& v) { return {-v.x, -v.y, -v.z}; }
inline Vector3 operator*(const Vector3& v, float s) { return {v.x * s, v.y * s, v.z * s}; }
inline Vector3 operator*(float s, const Vector3& v) { return v * s; }
inline Vector3 operator/(const Vector3& v, float s) { return {v.x / s, v.y / s, v.z / s}; }

inline Vector3& operator+=(Vector3& a, const Vector3& b) {
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;
	return a;
}

inline Vector3& operator-=(Vector3& a, const Vector3& b) {
	a.x -= b.x;
	a.y -= b.y;
	a.z -= b.z;
	return a;
}

inline Vector3& operator*=(Vector3& v, float s) {
	v.x *= s;
	v.y *= s;
	v.z *= s;
	return v;
}

// --- Vector2 のヘルパー関数 ---

// 内積
inline float Dot(const Vector2& a, const Vector2& b) { return a.x * b.x + a.y * b.y; }

// 長さの2乗（距離比較は平方根を省けるこちらを使う）
inline float LengthSquared(const Vector2& v) { return Dot(v, v); }

// 長さ
inline float Length(const Vector2& v) { return std::sqrt(LengthSquared(v)); }

// 正規化（ゼロベクトルはゼロベクトルのまま返す）
inline Vector2 Normalize(const Vector2& v) {
	const float length = Length(v);
	if (length == 0.0f) {
		return {0.0f, 0.0f};
	}
	return v / length;
}

// 2点間の距離が radius 以下か（円同士の当たり判定。半径は合計値を渡す）
inline bool IsWithinDistance(const Vector2& a, const Vector2& b, float radius) { return LengthSquared(a - b) <= radius * radius; }

// --- Vector3 のヘルパー関数 ---

// 内積
inline float Dot(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// 外積（左手系。Cross(+X, +Y) = +Z）
inline Vector3 Cross(const Vector3& a, const Vector3& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

// 長さの2乗（距離比較は平方根を省けるこちらを使う）
inline float LengthSquared(const Vector3& v) { return Dot(v, v); }

// 長さ
inline float Length(const Vector3& v) { return std::sqrt(LengthSquared(v)); }

// 正規化（ゼロベクトルはゼロベクトルのまま返す）
inline Vector3 Normalize(const Vector3& v) {
	const float length = Length(v);
	if (length == 0.0f) {
		return {0.0f, 0.0f, 0.0f};
	}
	return v / length;
}

// 2点間の距離が radius 以下か（球同士の当たり判定。半径は合計値を渡す）
inline bool IsWithinDistance(const Vector3& a, const Vector3& b, float radius) { return LengthSquared(a - b) <= radius * radius; }

// --- 線形補間（float版はここで1つだけ定義する。重複定義するとコンパイルエラー） ---

inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline Vector2 Lerp(const Vector2& a, const Vector2& b, float t) { return a + (b - a) * t; }
inline Vector3 Lerp(const Vector3& a, const Vector3& b, float t) { return a + (b - a) * t; }

// --- WorldTransform のヘルパー ---

// scale_/rotation_/translation_ からワールド行列を計算してGPUへ転送する。
// エンジンの WorldTransform は行列計算（UpdateMatrix）を持たないため、毎フレームの更新はこれを呼ぶ。
// 親子関係があるときは親を先に更新すること（子は更新済みの親の matWorld_ を掛ける）
inline void UpdateWorldTransform(WorldTransform& worldTransform) {
	worldTransform.matWorld_ = MakeAffineMatrix(worldTransform.scale_, worldTransform.rotation_, worldTransform.translation_);
	// 親がいれば親のワールド行列を掛ける（子は親の移動・回転・拡縮に追従する）
	if (worldTransform.parent_) {
		worldTransform.matWorld_ = Multiply(worldTransform.matWorld_, worldTransform.parent_->matWorld_);
	}
	worldTransform.TransferMatrix();
}

// --- 3D → 2D 変換 ---

// ワールド座標をスクリーン座標（ピクセル。左上原点・下が+y）へ変換する（3D物体の上にスプライトやテキストを出す用）。
// カメラより後ろ（画面に映らない位置）のときは false を返し、screen は変更しない。
// camera は UpdateMatrix 済みであること
inline bool WorldToScreen(const Vector3& world, const Camera& camera, Vector2* screen) {
	const Matrix4x4 viewProjection = Multiply(camera.matView, camera.matProjection);
	// 射影後のw（＝カメラから見た奥行き）を先に求め、カメラの後ろなら変換しない（w除算で反転・0除算するため）
	const float w = world.x * viewProjection.m[0][3] + world.y * viewProjection.m[1][3] + world.z * viewProjection.m[2][3] + viewProjection.m[3][3];
	if (w <= 0.0f) {
		return false;
	}
	const Matrix4x4 viewport = MakeViewportMatrix(0.0f, 0.0f, static_cast<float>(GameConfig::kScreenWidth), static_cast<float>(GameConfig::kScreenHeight), 0.0f, 1.0f);
	const Vector3 position = Transform(Transform(world, viewProjection), viewport);
	*screen = {position.x, position.y};
	return true;
}

} // namespace KamataEngine
