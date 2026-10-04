#include "Matrix4x4.h"
#include <cassert>
#include <cmath>
#include <utility>

namespace KamataEngine {

// 行列の積
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result{};
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			result.m[i][j] = m1.m[i][0] * m2.m[0][j] + m1.m[i][1] * m2.m[1][j] + m1.m[i][2] * m2.m[2][j] + m1.m[i][3] * m2.m[3][j];
		}
	}
	return result;
}

// 単位行列の作成
Matrix4x4 MakeIdentity4x4() {
	Matrix4x4 result{};
	for (int i = 0; i < 4; i++) {
		result.m[i][i] = 1.0f;
	}
	return result;
}

// 平行移動行列
Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
	Matrix4x4 result = MakeIdentity4x4();
	result.m[3][0] = translate.x;
	result.m[3][1] = translate.y;
	result.m[3][2] = translate.z;
	return result;
}

// 拡大縮小行列
Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
	Matrix4x4 result = MakeIdentity4x4();
	result.m[0][0] = scale.x;
	result.m[1][1] = scale.y;
	result.m[2][2] = scale.z;
	return result;
}

// X軸回転行列
Matrix4x4 MakeRotateXMatrix(float radian) {
	Matrix4x4 result = MakeIdentity4x4();
	float c = std::cos(radian);
	float s = std::sin(radian);

	result.m[1][1] = c;
	result.m[1][2] = s;
	result.m[2][1] = -s;
	result.m[2][2] = c;

	return result;
}

// Y軸回転行列
Matrix4x4 MakeRotateYMatrix(float radian) {
	Matrix4x4 result = MakeIdentity4x4();
	float c = std::cos(radian);
	float s = std::sin(radian);

	result.m[0][0] = c;
	result.m[0][2] = -s;
	result.m[2][0] = s;
	result.m[2][2] = c;

	return result;
}

// Z軸回転行列
Matrix4x4 MakeRotateZMatrix(float radian) {
	Matrix4x4 result = MakeIdentity4x4();
	float c = std::cos(radian);
	float s = std::sin(radian);

	result.m[0][0] = c;
	result.m[0][1] = s;
	result.m[1][0] = -s;
	result.m[1][1] = c;

	return result;
}

// 3次元アフィン変換行列
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
	// 各行列を作成
	Matrix4x4 scaleMat = MakeScaleMatrix(scale);
	Matrix4x4 rotXMat = MakeRotateXMatrix(rotate.x);
	Matrix4x4 rotYMat = MakeRotateYMatrix(rotate.y);
	Matrix4x4 rotZMat = MakeRotateZMatrix(rotate.z);
	Matrix4x4 transMat = MakeTranslateMatrix(translate);

	// 回転行列を合成（X → Y → Z）
	Matrix4x4 rotMat = Multiply(rotXMat, Multiply(rotYMat, rotZMat));

	// アフィン行列 = S * R * T
	Matrix4x4 affine = Multiply(scaleMat, Multiply(rotMat, transMat));

	return affine;
}

// ビューポート行列
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
	Matrix4x4 result = MakeIdentity4x4();

	// 正規化デバイス座標（-1〜+1）をスクリーンの大きさへ引き伸ばす。
	// スクリーンのY軸は下向きなので、Yだけ符号を反転させる
	result.m[0][0] = width / 2.0f;
	result.m[1][1] = -height / 2.0f;
	result.m[2][2] = maxDepth - minDepth;
	// 原点をスクリーンの中心へずらす
	result.m[3][0] = left + width / 2.0f;
	result.m[3][1] = top + height / 2.0f;
	result.m[3][2] = minDepth;

	return result;
}

// 逆行列（ガウス・ジョルダン法。部分ピボット選択つき）
Matrix4x4 Inverse(const Matrix4x4& matrix) {
	// 拡大行列 [matrix | 単位行列] を作り、左半分が単位行列になるよう掃き出すと右半分が逆行列になる
	float work[4][8]{};
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			work[i][j] = matrix.m[i][j];
		}
		work[i][4 + i] = 1.0f;
	}

	for (int column = 0; column < 4; column++) {
		// この列で絶対値が最大の行を軸（ピボット）に選ぶ（0除算と誤差の拡大を防ぐ）
		int pivot = column;
		for (int row = column + 1; row < 4; row++) {
			if (std::abs(work[row][column]) > std::abs(work[pivot][column])) {
				pivot = row;
			}
		}
		// 軸が0＝逆行列が存在しない（拡縮0などで潰れた行列）。単位行列を返して以降の計算を壊さない
		if (std::abs(work[pivot][column]) < 1.0e-8f) {
			return MakeIdentity4x4();
		}
		if (pivot != column) {
			for (int j = 0; j < 8; j++) {
				std::swap(work[column][j], work[pivot][j]);
			}
		}

		// 軸の行を軸の値で割って、軸を1にする
		const float inversePivot = 1.0f / work[column][column];
		for (int j = 0; j < 8; j++) {
			work[column][j] *= inversePivot;
		}

		// ほかの行からこの列の成分を消す
		for (int row = 0; row < 4; row++) {
			if (row == column) {
				continue;
			}
			const float factor = work[row][column];
			for (int j = 0; j < 8; j++) {
				work[row][j] -= factor * work[column][j];
			}
		}
	}

	// 右半分が逆行列
	Matrix4x4 result{};
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			result.m[i][j] = work[i][4 + j];
		}
	}
	return result;
}

// 座標変換（w除算あり）
Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix) {
	Vector3 result{};
	result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + matrix.m[3][0];
	result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + matrix.m[3][1];
	result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + matrix.m[3][2];
	float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + matrix.m[3][3];
	// wが0（カメラの真横など射影できない位置）でなければw除算する
	assert(w != 0.0f);
	result.x /= w;
	result.y /= w;
	result.z /= w;
	return result;
}

// 方向ベクトルの変換（平行移動の行 m[3] と w を使わない）
Vector3 TransformNormal(const Vector3& vector, const Matrix4x4& matrix) {
	Vector3 result{};
	result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0];
	result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1];
	result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2];
	return result;
}

} // namespace KamataEngine
