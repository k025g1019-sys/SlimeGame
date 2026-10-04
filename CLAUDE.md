# スライム（仮題）開発メモ

3人チーム開発の**3D**アクションのプロトタイプ（KamataEngine / DirectX 12 / C++20 / 1920×1080）。
緑のスライムが円形のフィールド（筒の内側の輪）に張り付いて進み続け、**スペースで平らになって敵や弾を乗せ、離して取り込む**／
**通常状態の弾力で爆弾を跳ね返してボスに当てる**。フィールドの敵を全滅させると奥の輪（次の層）へ進むフェーズ制で、最後のボスを倒すとクリア。
ゲーム仕様・アーキテクチャ・TODOは [README.md](README.md) を参照。

## ビルド・実行

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" "DirectXGame\DirectXGame.sln" /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

- 構成は Debug（ImGuiあり・ゲームシーン直行＝フェーズ1から・最適化なし）/ Develop（ImGuiあり・タイトルから。**最適化は無効**＝`/MT`と全体最適化だけ）/ Release（ImGuiなし・タイトルから・サイズ優先の最適化）
- 実行ファイル: `Generated\Outputs\<構成>\DirectXGame.exe`。**作業ディレクトリを `DirectXGame\` にして起動**（Resources を相対パスで読むため）
- W4 ＋ 警告はエラー扱い（`/WX`）・`/utf-8`。ソースはBOMなしUTF-8（日本語を含む PowerShell の .ps1 だけは **BOM付き** UTF-8。PowerShell 5.1 が BOM 無しを ANSI として読むため）
- 新しいプロジェクトはテンプレート（C:\KamataEngineTemplate）の `Tools\NewProject\new_project.ps1 -Destination <フォルダ> -Title <ウィンドウタイトル> [-GitInit]` で作る
  （このプロジェクトには置いていない。ゲームのフォルダから実行するとゲームごと複製されるため）

## コード規約

- タブインデント・日本語コメント（.clang-format 準拠、ColumnLimit 200）
- メンバ変数は末尾 `_`、調整可能な定数は `static inline` の `k` 接頭辞
- 調整項目は GlobalVariables パターン: 各クラスに `RegisterGlobalVariables()`（main起動時に登録）と
  `ApplyGlobalVariables()`（ゲーム中のクラスは `GameScene::ApplyAllGlobalVariables` が毎フレーム呼ぶ→ImGui編集が即反映。
  Title/GameOver/GameClearの各シーンは自分のUpdate冒頭で自分のApplyを呼び、
  編集ウィンドウ自体はmain.cppがGameScene以外のシーンで `GlobalVariables::Update()` を呼んで出す）を実装。
  **ImGui のドラッグで壊れる値（0除算・上下限の逆転・負の個数・拡縮0・線の本数の上限超え）は Apply で制限する**
- シーンを増やすときは main.cpp の `Scene` 列挙子・シーンのポインタ・`Start～()`・`ChangeScene`・`UpdateScene`・`DrawScene`・
  `DeleteAllScenes`（終了時とシーンジャンプの一括破棄。足し忘れるとそのシーンで終了したときにモデルやスプライトが解放されない）・
  Scene Debug の表示名とボタン・`ApplyRequestedScene` に1つずつ足す
- 入力のアクションは `GameInput.h` の `Action`・`kActionCount`・`kDefaultBindings` と `GameInput.cpp` の `kActionNames` を同じ順で足す
  （数が合わないと static_assert）。割り当ての変更は `DirectXGame/Resources/GameData/Input.json`。
  ゲームの操作は `kFlatten`（平ら。SPACE / パッドA。**決定と同じキー**なので、シーン開始時・ポーズ解除時は一度離すまで効かない）と、速度調整の `GetMove().x`（A/D）
- サウンドは**wav専用**（エンジンのAudioがmp3不可）。mp3素材は `Tools/ConvertSounds/convert_mp3_to_wav.ps1`
  で変換してから `Resources/sounds/` に置く。音量はサウンド個別（`Sound > Volume～`、Sounds.cpp の sSoundEntries）。
  読み込みは `LoadWaveIfExists`（ファイルが無ければスキップ）
- 新しい .cpp/.h を作ったら `DirectXGame.vcxproj` と `.filters` に手で追加する
- 時間は固定 1/60 秒（`GameConfig::kDeltaTime`）。速さの調整項目は「/秒」、時間は「秒」で書く

### ゲーム固有の取り決め

- **フェーズ構成は `DirectXGame/Resources/GameData/Stage/phases.json`**（PhaseData が読む。`//` コメント可）。
  配列の順にフェーズが進み、フェーズ k は層 k の輪で戦う。読み込みは「全部検証してから差し替え」で、壊れていても今の構成のまま動く
  （エラーは Game Debug に赤字。知らない項目名・範囲外・無限大になる数・長すぎる名前もエラー）。ファイルが無い・壊れているときの既定は
  `PhaseData::MakeDefault`（ファイルの初期内容と同じに保つこと）。実行中は Game Debug の **Reload Phases** / **Jump** で確認できる。
  **どのフェーズもスライムは開始角度（-90＝下）から始まる**（層の移動中に寄せる。Jump も同じ）ので、angleDeg・delay はそれを前提に書ける。
  出現の瞬間にスライムの近く（Enemy > SpawnSafeAngleDeg 以内）だった敵はその角度までずらして出す
- **座標は極座標のヘルパー（`Field` の static 関数）を使う**: θ（0=右、π/2=上、カメラから見て反時計回り）と h（輪の内面から軸へ向かう高さ）。
  `PolarToWorld(θ, h, z)`・`SurfaceRotationZ(θ)`（rotation_.z。ローカル+Yが「上」＝軸の方、+Xが θ の増える向き）・`WrapAngle`・
  `ArcDistance`・`DirectionTo`・`LayerZ(k)`。当たり判定は 3D の距離ではなく **`Footprint`（弧の半幅×高さの範囲）と `Field::Overlaps` / `CircleOverlaps`**
- **仮モデルは自作の OBJ（`Resources/box/`・`Resources/spike/`）と `Model::CreateSphere`**。色はすべて ObjectColor。
  OBJ を足すときは `Resources/<名前>/<名前>.obj`＋`.mtl`（Ka 0.3 / Kd 0.7、`map_Kd white1x1.png`、その白1pxを同じフォルダに置く）、
  面は必ず `v/vt/vn`。**x・z 対称な形にしておく**（読み込み時の左右反転・巻き順に依存しない。不透明パスはカリング無しで描いている）。
  共有モデルは `GameModels`（GameScene が作って各クラスへポインタで渡す）。光る球 `glowSphere` は環境光だけのマテリアル（加算エフェクト用）
- **仮の効果音は `Tools/PlaceholderSounds/generate_placeholder_sounds.ps1` で合成**（PowerShell 5.1 だけで PCM16bit/モノラル/44.1kHz の wav を書く。
  既存ファイルは上書きしない。作り直しは `-Force`）。本番の素材は同じファイル名の wav で上書きするだけ。BGM（bgm_main / bgm_boss）はまだ無い（置けば鳴る）
- **3Dの線は `LineDrawer`（LineDrawer.h）経由で描く**（1フレームの本数を数え、4000本で打ち切る。見積もりは LineDrawer.h の冒頭で、
  同じ計算を GameScene.cpp の static_assert で確かめている。線の本数が変わる調整項目（セグメント数・描く層の数・縦線の本数）は Apply で、
  予告マーカーの数は `GameScene::kMaxMarkersPerFrame` で上限を付けている。プールや上限の定数を増やしたら見積もりも直す）
- **敵・弾・エフェクトは上限付きのプール**（必要な分だけ作って使い回す。ゲーム中に delete しない。上限は `GameScene::kMaxEnemies` / `kMaxProjectiles`・`Effects::kMaxParticles`）。
  敵・ボスの攻撃は `ProjectileSpawn` の要求を積むだけで、GameScene が更新ループの後でまとめて弾にする（ループ中にコンテナを触らない）
- GameScene は実装を3ファイルに分けている: `GameScene.cpp`（初期化・フェーズ進行・描画）／`GameSceneInteraction.cpp`（当たり判定＝乗る・取り込む・跳ね返す・爆発）／
  `GameSceneDebug.cpp`（ImGui の Game Debug・出来事のログ・判定の可視化）
- テスト用のフック（入力の自動化・特定フェーズからの開始など）を本番のソースに残さない（必要ならプロジェクトをコピーして足す）。
  デザイナーが使う機能（Jump・Reload Phases など）は Game Debug に置く

## エンジンの注意点

### 初期化・ループ（main.cpp。テンプレートで対策済み）

- **エンジン初期化は main.cpp で手動**（`KamataEngine::Initialize` は1280×720固定で、エンジンのInputまで初期化するため不使用）。
  初期化順: CoInitializeEx → WinApp(1920×1080) → DirectXCommon → ImGuiManager → TextureManager →
  Sprite::StaticInitialize(1920,1080) → `TextureManager::Load("white1x1.png")` → `ModelCommon::Initialize` →
  `PrimitiveDrawer::Initialize` → DebugText → Audio。ImGuiManager はテクスチャより先（フォントがデスクリプタヒープの先頭を使う）
- 終了は シーンの delete → PrimitiveDrawer / ModelCommon の Terminate → `ImGuiManager::Terminate()`（DirectXCommonより前。Finalizeは呼ばない）
  → Audio → TextureManager → DirectXCommon → WinApp → CoUninitialize。Model・WorldTransform・Camera・ObjectColor・LightGroup など
  GPUリソースを持つ物は DirectXCommon::Terminate より前に破棄する（static やグローバルに置かない）
- **エンジンのInput（DirectInput）は不使用**（DISCL_NOWINKEYでWinキーが無効化されるため）。
  入力はGameInputが GetAsyncKeyState / XInputGetState で自前実装（**ゲームのウィンドウが前面のときだけ**キーを受け付ける）。
  ループも `KamataEngine::Update()` ではなく自前の ProcessAllMessages ＋ `Sprite::ResetInstanceCount()` ＋
  `PrimitiveDrawer::Reset()` ＋ `Audio::Update()` を直接呼ぶ
  （KamataEngine::Update は未初期化のInputを触るため使えない。WinApp::ProcessMessage は
  1フレーム1件しか処理せずImGui操作と×ボタンが遅延するため、全件処理の自前版を使う）
- **ImGuiのカーソル位置ずれ補正が main.cpp に2点ある**（①ProcessAllMessages でマウス座標を
  クライアント→1920×1080へスケール、②Begin直後に DisplaySize とメインビューポートを1920×1080へ上書き）。
  バックバッファ（常に1920×1080）とウィンドウ（既定1600×900）のサイズ差が原因。片方だけ消すと再発する
- `WinApp::kWindowWidth/Height` は1280×720のまま。画面サイズは必ず `GameConfig::kScreenWidth/kScreenHeight` を使う
- **AxisIndicator は使えない**（描画位置が1280幅前提の (1180,0) に固定で、ビューポートも戻さない）→ 軸は PrimitiveDrawer の線で描く。
  **DebugCamera も使えない**（エンジンのInputのマウスを読むため、不使用の環境では動かない）→ `GameCamera` の ImGui デバッグカメラ（Game Debug の Debug Camera (orbit)）を使う

### シェーダー（External/KamataEngine/shaders。エンジン更新時に消さないこと）

- **SpritePS.hlsl に NonUniformResourceIndex 修正済み**。外すと複数テクスチャ混在時にスプライト境界へ他テクスチャの欠片が出る
- **ObjPS.hlsl に透明テクセルの discard を追加済み**（`texcolor.a < 0.5/255` を捨てる）。外すと透明部分まで深度が書き込まれ、
  後から描く奥の物体がくり抜かれる
- シェーダーはビルド後イベントで `DirectXGame/Resources/EngineShaders/` へコピーされ、実行時にコンパイルされる（直すのは External 側）

### 3D

- **WorldTransform に UpdateMatrix は無い**。`MathUtil.h` の `UpdateWorldTransform(wt)`（アフィン行列→親を掛ける→TransferMatrix）を毎フレーム呼ぶ。
  親子は親を先に更新する。数学関数は `MathUtil.h` / `Matrix4x4.h`（KamataEngine 直下）。
  **`KamataEngine::MathUtility` 名前空間に自前定義しない**（エンジンのlibと重複して LNK2005）。前作の global な `Vector3.h` は持ち込まない
- **WorldTransform / Camera / ObjectColor の定数バッファはGPUの実行時に読まれる**。1つを使い回して値を変えながら複数回描くと、
  全部が最後の値になる → **描く物体1つにつき WorldTransform を1つ**、ObjectColor も色が違うなら1つずつ持つ
  （同じ色の物どうしは1つの ObjectColor を共有してよい。輪のセグメントの交互の2色・敵の脚などがその例）
- **WorldTransform・Camera はコピー不可**（ムーブも不可）→ `std::vector<WorldTransform>` は伸ばせない。
  `std::vector<std::unique_ptr<…>>` / `std::list` / `std::array` で持つ。使う前に必ず `Initialize()`（ObjectColor の SetColor も Initialize 後）
- **テクスチャ差し替え版の `Model::Draw(wt, camera, textureHandle, color)` はモデルのマテリアルの定数バッファを書き換える**
  → 同じモデルを1フレームに別のテクスチャで描くと全部最後のテクスチャになり、以後の通常の Draw にも残る（そのマテリアルの `Update()` を呼ぶまで）。
  このゲームでは使っていない（テクスチャを変えたいときはモデルを別に作る）
- `Model::Draw` は `Model::PreDraw()`～`Model::PostDraw()` の間で呼ぶ。**`Model::PreDraw` はコマンドリストを取らない**
  （引数はカリング・ブレンド・深度のモード）。半透明は `PreDraw(kBack, kNormal, kReadOnly)`、加算は `PreDraw(kBack, kAdd, kReadOnly)`
  で不透明の後に描く。内側から見る面（筒の内側など）は `CullingMode::kFront` か `kNone`（kNone の半透明は表裏が二重に混ざる）
- `ObjectColor` はライティング×テクスチャの結果に掛け算される（アルファも）。色を付けたい単色の物は白テクスチャのモデルで描く。
  `Model::SetAlpha` はそのモデルを使う全員に効くので、個別の半透明は ObjectColor で行う
- ライトは `GameModels::lightGroup`（`Model::SetLightGroup` で全モデル共通）。値は Field の調整項目から毎フレーム入れる。
  `SetDirLightDir` は「光の進む向き」。筒の内側は面の向きがばらばらなので、カメラに付いた点光源（ヘッドライト）を足している
- **拡縮（scale_）を厳密に0にしない**（ライティング用の逆行列が壊れて NaN になる）。潰す演出は 0.01 などで止める
- **.obj / .wav が見つからないとエンジンがメッセージボックスを出して終了する**。任意の素材は `std::filesystem::exists` で確認してから読む
  （Sounds の `LoadWaveIfExists`・GameModels の `CreateModelOrSphere` がその例）。モデルは `Resources/<名前>/<名前>.obj` の形（サブフォルダ不可）
- **PrimitiveDrawer（3Dの線）**: 毎フレーム `Reset()` が必須（main で呼んでいる）。**1フレーム4096本まで**で上限チェックが無い（超えるとメモリを壊す）。
  1本ごとに描画コマンドが出る。**Model の PreDraw～PostDraw の外**で、**先に `SetCamera(&camera)`**（未設定だと落ちる）。破棄するカメラを指したままにしない。
  ゲームでは `LineDrawer` を通す
- **カメラ**: 左手系・+Yが上・既定は+Z（奥）を向く。`rotation_` の x=見下ろす角度・y=左右（ラジアン）。**z（ロール）は `Camera::UpdateMatrix` が無視**し、
  真上・真下（±90°）を向くと計算が壊れる。位置や向きを変えたら `UpdateMatrix()`。farZ はエンジン既定1000（このゲームは Camera > FarZ で2000）
- **描画先は sRGB**。Model・ObjectColor・Sprite の色の数値は線形として扱われ、画面では明るく見える（0.18→約0.46、0.5→約0.73、0.02→約0.15）。テクスチャの色はそのまま出る
- Model / Sprite（`Create` で作った物）は作った側が delete する

### 2D・その他

- スプライトの座標はピクセル・左上原点・下+y。`Vector2`/`Vector3` の演算子・`Normalize` などは `MathUtil.h` で自前定義
- スプライトは `Sprite::PreDraw(cmdList)` ～ `PostDraw()` でまとめて描かれる（1フレーム1024個まで。DebugText の1文字も1個に数える）。
  加算合成は `Sprite::PreDraw(cmdList, Sprite::BlendMode::kAdd)` の別パスで。3Dより奥に板を敷くときは描いた後に `DirectXCommon::ClearDepthBuffer()`
- `DebugText` はASCIIのみ。`Printf` は `SetPos` 依存なので `Print(std::format(...), x, y, 倍率)` を使う。
  `DrawAll()` はスプライトパスの中で何度呼んでもよく、呼んだ時点までの文字がその重なり順で描かれる
- `std::min/max/clamp` を使う .cpp は先頭に `#define NOMINMAX`（Windows.hのマクロ対策）。`std::clamp` は下限＞上限だと Debug で止まる
- Debugビルドは起動に数秒かかる（デバッグ版のPNGデコードが遅い。白画面のまま少し待つ）
- 1フレームは固定で 1/60 秒として扱っている（`GameConfig::kDeltaTime`・Fade など）

## 由来

KamataEngine テンプレート（2026年10月作成。C:\KamataEngineTemplate）から `new_project.ps1` で作成し、
テンプレートの3Dサンプル（GameScene・天球・UVチェッカー）をゲーム本体に置き換えた。
テンプレートはバブルキャッチャー（C:\KamataEngine20260622、2D横スクロールSTG）の汎用部分（手動初期化・GameInput・GlobalVariables・
Fade・Easing・Sounds・シーン管理・ImGui対策）に、前作（C:\KamataEngine20260409）の3Dの知見を足したもの。GlobalVariables は bool 型対応済み。
