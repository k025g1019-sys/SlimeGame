#pragma once
#include "KamataEngine.h"

/// <summary>
/// 行列計算ヘルパー（前作から流用し、逆行列・方向ベクトル変換を追加）。
/// エンジンの Matrix4x4 は素の構造体のため、アフィン変換などの計算関数をここで提供する。
/// 行ベクトル規約（v * M。平行移動は m[3][0～2]）で、エンジンのシェーダー・Camera と同じ並び。
/// ※エンジンのlibにも KamataEngine::MathUtility::～ が入っているがヘッダーが無いので使えない。
/// 　同じ名前空間（KamataEngine::MathUtility）に自前で定義するとリンクエラー（LNK2005）になるので、
/// 　ここでは KamataEngine 直下に定義している
/// </summary>
namespace KamataEngine {
// 行列の積（m1 → m2 の順に変換を適用する行列）
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);
// 単位行列の作成
Matrix4x4 MakeIdentity4x4();
// 平行移動行列
Matrix4x4 MakeTranslateMatrix(const Vector3& translate);
// 拡大縮小行列
Matrix4x4 MakeScaleMatrix(const Vector3& scale);
// X軸回転行列
Matrix4x4 MakeRotateXMatrix(float radian);
// Y軸回転行列
Matrix4x4 MakeRotateYMatrix(float radian);
// Z軸回転行列
Matrix4x4 MakeRotateZMatrix(float radian);
// 3次元アフィン変換行列（拡縮 → 回転(X→Y→Z) → 平行移動。エンジンの Object3d と同じ順）
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
// ビューポート行列（正規化デバイス座標→スクリーン座標。ワールド座標をスクリーン座標へ変換するのに使う）
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);
// 逆行列（一般の4x4。逆行列が存在しない＝拡縮0などで潰れた行列のときは単位行列を返す）
Matrix4x4 Inverse(const Matrix4x4& matrix);
// 座標変換（ベクトルに行列を適用してw除算する。ワールド→スクリーン変換などに使う）
Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);
// 方向ベクトルの変換（平行移動を無視して回転・拡縮だけ適用する。向きや速度の回転に使う）
Vector3 TransformNormal(const Vector3& vector, const Matrix4x4& matrix);
} // namespace KamataEngine
