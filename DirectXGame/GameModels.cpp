#include "GameModels.h"
#include <filesystem>
#include <string>

using namespace KamataEngine;

namespace {
// Resources/<名前>/<名前>.obj があれば読み込み、無ければ球で代用する
// （エンジンの CreateFromOBJ はファイルが無いとメッセージボックスを出して終了するため、先に確認する）
Model* CreateModelOrSphere(const std::string& name) {
	const std::string path = "Resources/" + name + "/" + name + ".obj";
	if (std::filesystem::exists(path)) {
		return Model::CreateFromOBJ(name, false);
	}
	return Model::CreateSphere(8, 8);
}
} // namespace

void GameModels::SetMaterialLighting(Model* model, float ambient, float diffuse) {
	if (!model) {
		return;
	}
	for (const auto& mesh : model->GetMeshes()) {
		Material* material = mesh->GetMaterial();
		if (material) {
			material->ambient_ = {ambient, ambient, ambient};
			material->diffuse_ = {diffuse, diffuse, diffuse};
			material->specular_ = {0.0f, 0.0f, 0.0f};
			material->Update();
		}
	}
}

void GameModels::Create() {
	box = CreateModelOrSphere("box");
	// ボス専用の箱（同じ OBJ を別のモデルとして読む＝マテリアルが別になるので、陰影の強さをボスだけ変えられる）
	bossBox = CreateModelOrSphere("box");
	spike = CreateModelOrSphere("spike");
	sphere = Model::CreateSphere(24, 24);
	sphereLow = Model::CreateSphere(10, 10);
	// 光る球：マテリアルを環境光だけにする（陰影が付かず ObjectColor の色がそのまま出る。マテリアルはこのモデル専用なので他に影響しない）
	glowSphere = Model::CreateSphere(16, 16);
	SetMaterialLighting(glowSphere, 1.0f, 0.0f);

	// ライト（既定のライトは上下左右から白く当たって色が飛ぶので、自前の設定にする。値は Field::UpdateLightGroup が毎フレーム入れる）
	lightGroup = LightGroup::Create();
	for (Model* model : {box, bossBox, spike, sphere, sphereLow, glowSphere}) {
		model->SetLightGroup(lightGroup);
	}
}

void GameModels::Destroy() {
	delete box;
	delete bossBox;
	delete spike;
	delete sphere;
	delete sphereLow;
	delete glowSphere;
	delete lightGroup;
	box = nullptr;
	bossBox = nullptr;
	spike = nullptr;
	sphere = nullptr;
	sphereLow = nullptr;
	glowSphere = nullptr;
	lightGroup = nullptr;
}
