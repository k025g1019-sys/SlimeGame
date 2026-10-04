#pragma once
#include "KamataEngine.h"

/// <summary>
/// ゲーム中で共有する仮モデル（素材が無いので、箱・四角すい・球の組み合わせ＋ObjectColor の色で見た目を作る）と、
/// それらに使うライト。GameScene が作って持ち、各クラスにはポインタで渡す（作った GameScene が Destroy する）。
/// ・box   : Resources/box/box.obj（-0.5～+0.5 の立方体。白1px テクスチャ）
/// ・bossBox : box と同じ OBJ を別のモデルとして読んだもの（マテリアルが別。ボスだけ陰影を弱めて色を明るく出す。値は Boss の調整項目）
/// ・spike : Resources/spike/spike.obj（四角すい。先端が +Y）
/// ・sphere / sphereLow : Model::CreateSphere（半径1。分割数違い。sphereLow はエフェクトの粒用）
/// ・glowSphere : Model::CreateSphere のマテリアルを「環境光1・拡散0・鏡面0」にした光る球（ライトの陰影が付かず、色がそのまま出る。爆発・火花の加算用）
/// どのモデルもテクスチャ差し替え版の Draw は使わない（マテリアルが全描画で共有されるため）。色は ObjectColor で付ける
/// </summary>
struct GameModels {
	KamataEngine::Model* box = nullptr;
	KamataEngine::Model* bossBox = nullptr;
	KamataEngine::Model* spike = nullptr;
	KamataEngine::Model* sphere = nullptr;
	KamataEngine::Model* sphereLow = nullptr;
	KamataEngine::Model* glowSphere = nullptr;
	// 全モデル共通のライト（筒の中を照らすため、カメラに付いて動く点光源を足してある。設定は Field の調整項目）
	KamataEngine::LightGroup* lightGroup = nullptr;

	// モデルとライトを作る（OBJ が見つからないときは球で代用する＝エンジンが止まらないように）
	void Create();
	// 作った物を破棄する
	void Destroy();

	// モデルのマテリアルの環境光・拡散光の強さを変える（鏡面は0。そのモデルを使う全描画に効く＝専用のモデルにだけ使う）
	static void SetMaterialLighting(KamataEngine::Model* model, float ambient, float diffuse);
};
