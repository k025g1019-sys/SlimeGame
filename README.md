# スライム（仮題）

筒の内側に並んだ円形のフィールドを、緑のスライムが張り付いて進み続ける**3Dアクション**（プロトタイプ）。
**スペースを押している間は平べったくなって敵や弾を上に乗せ、離した瞬間に元に戻って取り込む**。
爆弾だけは取り込むとダメージ——**通常状態のまま当たれば弾力で跳ね返せる**ので、跳ね返した爆弾をボスにぶつけて倒す。
フィールドの敵を全滅させると奥の輪（次の層）へ進み、最後の層のボスを倒すとクリア。

- チーム: （3人。名前を書く）
- エンジン: KamataEngine（DirectX 12）／解像度 1920×1080（F11でフルスクリーン切替）
- 開発環境: Visual Studio 2026 / `DirectXGame/DirectXGame.sln` を開いて x64 でビルド
- 状態: **プロトタイプ**。モデルは箱・四角すい・球の組み合わせ、効果音は合成した仮の音、BGM は無し。
  ゲームの手ざわりに関わる数値（速さ・大きさ・時間・攻撃・色・ライトなど）は ImGui（Global Variables）で調整できる。
  演出の細かい値（爆発の大きさの倍率・火花の数・HUD の文字の倍率など）はコード内の名前付きの定数

## 遊び方（操作）

| 操作 | キーボード | Xboxコントローラー |
| --- | --- | --- |
| 速度の調整（D で加速・A で減速。止まる・戻ることはできない） | A / D | Lスティック左右 / 十字キー左右 |
| 平らになる（押している間）→ 離すと元に戻って取り込む | SPACE（押しっぱなし） | A（押しっぱなし） |
| 決定（タイトル・メニュー） | SPACE | A |
| ポーズ | ESC | メニューボタン（START） |
| フルスクリーン切替 | F11 | — |

割り当ては `DirectXGame/Resources/GameData/Input.json` で変えられる（アクション名 `Menu` / `Decide` / `Flatten`）。
「平らになる」と「決定」は同じキーなので、ゲーム開始時・ポーズから戻ったときは**一度キーを離すまで平らにならない**。

---

## ゲームのルール

### フィールドとスライム

- フィールドは**円形の輪**。輪が奥へ層になって並んでいる（同心円）。カメラは筒の軸上から奥を向いて固定で、
  **次の層へ進むときだけ**前へ動く
- スライムは輪の内面に張り付き、**何もしなくても反時計回り（画面の下では右）へ進み続ける**。A/D で少しだけ速さを変えられる
- **通常（まるい）**と**平ら**の2つの状態がある。スペースを押している間が平ら、離した瞬間に通常へ戻る（ぷるんと揺れる）
- 体力は5（HUD 左上の緑の四角）。被弾すると1減って少しの間無敵（点滅）。0でゲームオーバー

### 接触の早見表（このゲームの核）

| 相手 | 通常状態のスライムに当たる | 平らなスライムに重なる → 離した瞬間 |
| --- | --- | --- |
| 雑魚敵（紫の虫） | **被弾**（歩く敵は弾き飛ばされて向きを反転） | 上に**乗る** → **取り込んで倒す** |
| トゲ弾（オレンジの三角） | **被弾**（弾は消える） | 上に**乗る** → **食べる**（回復量は調整項目。既定0） |
| 爆弾（黒い球・オレンジの縁取り。導火線が短くなると赤く点滅） | **跳ね返す**（無敵中でも） | 上に**乗る** → **体内で爆発してダメージ**（乗せたまま導火線が尽きても、その場で爆発して同じダメージ） |

- 乗った物はスライムと一緒に動き、離した瞬間に乗っている物と足元に重なっている物を**全部**取り込む（0.2秒ほどで吸い込まれる）
- **跳ね返した爆弾は味方の弾**（緑がかった色になる）
  - ボスが攻撃中のボスフェーズなら、**ボスへ一直線に飛んで1ダメージ**
  - それ以外は輪に沿って逆向きに速く転がり、**敵に当たると爆発**して爆風の中の敵を倒す（スライムにはダメージ無し）
- 敵の爆弾が導火線切れで爆発したとき、爆風の中にスライムがいると被弾する
- 爆弾にとっての正解は「通常状態のまま受けて跳ね返す」だけ（HUD 下の操作説明にも `BOMB: STAY ROUND TO BOUNCE` と出している）

### 敵

| 種類 | 見た目 | 動き |
| --- | --- | --- |
| walker | 紫の虫 | 輪に沿って歩く（既定はスライムと向かい合う時計回り）。攻撃はせず、接触でダメージ |
| shooter | 赤紫の虫・頭にトゲ | その場にいて、一定間隔でスライムに近い向きへトゲ弾を撃つ（撃つ前に膨らむ） |
| bomber | 青い虫・背中に大きな爆弾 | その場にいて、一定間隔でスライムに近い向きへ爆弾を転がす |
| ボス | オレンジの箱のゴーレム | 筒の中心の少し奥に浮かび、空中弾（トゲ・爆弾）を投げる。着弾点には予告の円（爆弾=赤、トゲ=黄。二重線で、内側の円が着弾までに縮む）が出る |

- 雑魚は出現してから少しの間（白く光って膨らむ間）は無害。体力は1。
  出現の瞬間にスライムから35度以内の位置だった敵は、35度の位置までずらして出す（出た瞬間に重なって当たる理不尽を防ぐ。Enemy > SpawnSafeAngleDeg、0で無効）
- **ボス（仮）**：レベル1～3。攻撃の間隔はレベルが上がるほど短く、1回に投げる弾はレベルの数。着弾点はスライムの少し先（予測位置）を中心に
  5（弧の長さ。Boss > VolleySpread）ずつ離す。空中から着地したトゲは0.3秒その場で止まってから滑り出す（Projectile > AirSpikeLandPause）。
  → 真ん中の爆弾を通常状態で跳ね返してから、隣のトゲが来るまでに平らになって食べる余裕がある（約0.45秒）。
  **2回の攻撃に1回は必ず爆弾を混ぜる**（倒す手段が爆弾の跳ね返しのため）。体力が0になると CLEAR! の間はのけぞったまま見えていて、
  次の層への移動と同時に**奥へ逃げ**、次のボスフェーズの層で待つ（奥に小さく見える）。最後のフェーズのボスは爆発の連鎖で撃破される

### フェーズの流れ

`PHASE n（ボスなら WARNING）のバナー` → `戦闘` → `CLEAR!` → `カメラとスライムが次の層へ移動（ボスは同時に奥へ逃げる）` → 次のフェーズ …

- 雑魚フェーズ：そのフェーズの敵が全員出現し、全員倒したらクリア
- ボスフェーズ：ボスの体力を0にしたらクリア（残った雑魚はまとめて消える）
- 吸い込み中の弾（飲み込んだ爆弾・トゲ）があるうちはクリアにしない（0.2秒ほど待つ。最後の敵と一緒に飲み込んだ爆弾のダメージを消さないため）
- クリアすると画面上の敵の弾は消える。最後のフェーズをクリア → `GAME CLEAR` → クリア画面。体力0 → つぶれる演出 → ゲームオーバー画面（リトライ可）
- **どのフェーズもスライムは開始角度（-90＝下）から始まる**（層の移動中に、短い向きで下へ移っていく）。Game Debug の Jump と同じ状態になる
- ボスフェーズの WARNING / CLEAR! / GAME CLEAR のバナーは、画面中央のボスに重ならないよう上寄せ・小さめ・薄い帯（Hud > BossBanner～）。
  最後のボスの GAME CLEAR は、撃破演出（爆発の連鎖）が終わってから出る
- 既定の構成は 雑魚 → ボス1 → 雑魚 → ボス2 → 雑魚 → ボス3 の6フェーズ（下の phases.json で自由に変えられる）

---

## フェーズ構成（phases.json）

**`DirectXGame/Resources/GameData/Stage/phases.json`** で、フェーズの数・順番・敵の配置を決める（`//` コメントを書ける JSON）。

```jsonc
{
	"phases": [
		{
			"type": "wave", "name": "WAVE 1",
			"enemies": [
				{ "type": "walker", "angleDeg": 0, "delay": 1.0 },
				{ "type": "shooter", "angleDeg": 90, "delay": 1.0 },
				{ "type": "bomber", "angleDeg": 120, "delay": 3.0 }
			]
		},
		{ "type": "boss", "name": "BOSS 1", "bossLevel": 1, "hp": 3 }
	]
}
```

| 項目 | 意味 |
| --- | --- |
| `type` | `"wave"`（雑魚を全滅でクリア）／`"boss"`（ボスの体力0でクリア） |
| `name` | 表示名（**英数字のみ・24文字まで**。日本語は画面に出せず `?` になる）。省略すると `WAVE n` / `BOSS n` |
| `bossLevel` | ボスの強さ 1～3（boss のみ。省略=1） |
| `hp` | ボスの体力 1～99（boss のみ。跳ね返した爆弾1発で1減る。省略=3） |
| `enemies` | 雑魚の配置（wave は1体以上必要。boss にも書ける） |
| `enemies[].type` | `"walker"` / `"shooter"` / `"bomber"` |
| `enemies[].angleDeg` | 出現する角度（度。-3600～3600）。**0=右、90=上、180=左、270(-90)=下**。カメラから見て反時計回りに増える。どのフェーズもスライムは -90（下）から反時計回りに進む（約26度/秒） |
| `enemies[].delay` | フェーズ開始から出現までの秒数（0～3600。省略=0） |
| `enemies[].dir` | walker の歩く向き。-1=時計回り（スライムと向かい合う）、1=反時計回り（省略=-1） |

- **追加・削除・入れ替え**は配列の要素を足す・消す・並べ替えるだけ。フェーズ k は層 k（奥へ k 番目の輪）で戦い、層の数はフェーズ数で自動で決まる。
  最後のフェーズがクリア条件（ボスで終わるならボス撃破でクリア）。ボスフェーズを途中に何回入れてもよい（ボスは逃げて次のボスフェーズで待つ）
- 上限：フェーズ32個・1フェーズの敵32体
- 実行中は Game Debug の **Reload Phases** で読み直せる（今のフェーズの番号からやり直す）。**Jump** で任意のフェーズから始められる
- 読み込みは「全部検証してから差し替え」。書き間違い（`"walkr"`・**知らない項目名（`"dealy"` など）**・数字の範囲外・カンマ抜けなど）があると**今の構成のまま動き、Game Debug に赤字でどこが悪いか出る**。
  起動時にファイルが無い・壊れているときは組み込みの既定構成（`PhaseData.cpp` の `MakeDefault`。ファイルの初期内容と同じ）で動く
- 敵の攻撃のタイミングは乱数を使わず決定的（フェーズ内の並び順で少しずつずれる）なので、同じ操作なら毎回同じ展開になる

---

## 調整項目（Global Variables）

Debug / Develop で実行すると出る **Global Variables** ウィンドウでグループを開き、値をドラッグして即反映 → **Save** で
`DirectXGame/Resources/GameData/GlobalVariables/<グループ名>.json` に保存（Gitで共有する）。

| グループ | 主な項目 |
| --- | --- |
| Field | 輪の半径・層の間隔・帯の幅と厚み・セグメント数（8～64）・描く層の数（1～8）・縦線の本数（0～32）・輪/縁/縦線の色・奥の層を暗くする割合・背景色・ライト（環境光・主光源・補助光・カメラに付いた点光源） |
| Camera | 層からの距離・軸からのずれ・視野角・farZ・次の層への移動時間（AdvanceDuration）・画面揺れ（振幅0で無効・時間・速さ・被弾/ボス命中のときの強さ） |
| Player | 開始角度・半径・通常/平らの形（高さ・横幅の倍率・平らの判定の上面 FlatTop・乗った物を置ける幅 RiderHalfWidthScale）・速さ（基本・A/Dの調整幅・下限・追従・平らの倍率）・バネ（硬さ・減衰・跳ね返し/被弾のインパルス）・最大HP・無敵時間・フェーズクリアの回復・トゲを食べた回復・爆弾を取り込んだダメージ・敵/トゲが平らに乗るか・色・不透明度・目の大きさ |
| Enemy | 半径・出現演出の時間・出現位置の安全範囲（SpawnSafeAngleDeg）・walker の速さ・弾き飛ばされる距離と時間・shooter/bomber の攻撃間隔・最初の攻撃までの割合・予備動作・色 |
| Projectile | トゲ（速さ・高さ・半径・寿命・色）・爆弾（半径・転がる速さ・導火線・点滅し始める残り時間・爆風の半径・色・縁取りの大きさと色・火花の大きさ）・跳ね返し（速さの倍率・ボスへ飛ぶ時間）・空中弾の飛ぶ時間（5秒まで）・予告マーカーの半径・着地したトゲが止まる時間・吸い込みの時間 |
| Boss | 大きさ・層からの奥行き・高さ・登場/逃走の時間と距離・攻撃間隔（レベル1）とレベルごとの倍率・1レベルあたりの弾数・着弾点の間隔・爆弾を含める攻撃の間隔（N回に1回）・最初の攻撃までの時間・投げる前の予備動作・被弾ののけぞり/光る時間・撃破演出・揺れ・色・ボス専用の箱のマテリアル（環境光・拡散光の強さ） |
| Phase | フェード時間・各状態の時間（PHASE n バナー・CLEAR!・ゲームクリア・敗北） |
| Hud | HP の四角の位置・大きさ・色、フェーズ表示、ボスのHPバー、バナーの大きさ・帯（ボスフェーズ用の位置・大きさ・帯も別にある）、操作説明の文字の大きさ |
| Effect | 爆発の時間・色、取り込みの粒の数・速さ・時間・大きさ・減速（/秒）、火花の時間・大きさ・重さ |
| Sound | MasterVolume と、サウンドごとの音量（Volume～） |
| Title / GameOver / Clear / Pause | 各画面の背景色・文字の位置と大きさ（タイトルの遊び方の説明の大きさ・行間も） など |

0 や負にすると壊れる値（時間・間隔・半径など）・線の本数が増える値（セグメント数・空中弾の飛ぶ時間など）は、各クラスの `ApplyGlobalVariables` で範囲を制限している。

---

## 仮素材と差し替え方

### モデル（すべて仮。色は ObjectColor）

| 物 | 作り | 差し替えるとき |
| --- | --- | --- |
| 輪のセグメント・敵の脚・火花・爆弾の導火線 | `Resources/box/box.obj`（-0.5～+0.5 の立方体） | 輪の見た目を変えるなら Field.cpp の `Draw` |
| ボス | `bossBox`（box.obj を別のモデルとして読んだもの。マテリアルが別で、陰影を弱めて色を明るく出す） | Boss.cpp の部品の配置表（`layouts`）と `Draw` |
| トゲ弾・shooter の角 | `Resources/spike/spike.obj`（四角すい。先端が +Y） | Projectile.cpp の `UpdateVisual` / `Draw` |
| スライム・敵の体・目・爆弾 | `Model::CreateSphere` | Player.cpp / Enemy.cpp の `Draw` |
| 爆発 | `glowSphere`（環境光だけのマテリアルの球。加算） | Effects.cpp |
| 爆弾の火花・縁取り | `glowSphere`（火花は加算、縁取りは少し大きい球を表面カリングで描いて体のまわりに輪郭を出す） | Projectile.cpp の `DrawAdditive` / `DrawOutline` |

- 本番のモデルは `Resources/<名前>/<名前>.obj`（＋.mtl・テクスチャ。サブフォルダ不可）に置き、`GameModels.cpp` で読み込んで各クラスの `Draw` で使う。
  OBJ の面は `v/vt/vn` の形（vt が無いとエンジンが止まる）。ファイルが無いとエンジンが止まるので、`GameModels` の `CreateModelOrSphere` のように存在を確認する
- 自作の OBJ は面の向き（巻き順）に頼らないよう、不透明パスを**カリング無し**で描いている。テクスチャ付きの本番モデルに替えたら必要に応じて `GameScene::Draw` のカリングを戻す

### サウンド（`DirectXGame/Resources/sounds/`）

| ファイル | 鳴る場面 | 今の中身 |
| --- | --- | --- |
| se_decide.wav | 決定（タイトルなど） | エンジン付属の木魚 |
| se_fanfare.wav | ゲームクリア画面 | エンジン付属のファンファーレ |
| se_flatten.wav / se_restore.wav | 平らになる／元に戻る | 合成（下がる音／上がる音） |
| se_eat.wav / se_absorb_enemy.wav | トゲを食べた／敵を取り込んだ | 合成（ぽぽっ／ごくん） |
| se_bounce.wav | 爆弾を跳ね返した | 合成（ぼよーん） |
| se_explosion.wav | 爆発 | 合成（ノイズ＋低音） |
| se_damage.wav | スライムの被弾 | 合成 |
| se_boss_hit.wav | ボスに爆弾が当たった | 合成（金属音） |
| se_phase_clear.wav / se_boss_warning.wav | フェーズクリア／ボスフェーズ開始 | 合成（ドミソド／警報） |
| bgm_main.wav / bgm_boss.wav | タイトル・雑魚フェーズ／ボスフェーズ | **まだ無い**（置けば鳴る。bgm_boss が無いときはボスフェーズも bgm_main のまま） |

- 合成音は `Tools/PlaceholderSounds/generate_placeholder_sounds.ps1` で作った（PowerShell だけで PCM16bit/モノラル/44.1kHz の wav を書く）。
  `powershell -ExecutionPolicy Bypass -File Tools\PlaceholderSounds\generate_placeholder_sounds.ps1`（既存ファイルは上書きしない。作り直しは `-Force`）
- **本番の音に替えるときは、同じファイル名の wav を置くだけ**（mp3 は `Tools/ConvertSounds/convert_mp3_to_wav.ps1` で変換してから）。音量は Global Variables の Sound グループ

---

## デバッグ機能（Debug / Develop のみ）

**Game Debug** ウィンドウ（右側）：

- 状態の表示（フレーム・フェーズ・状態・スライムの角度/速さ/HP・ボスの状態・敵/弾/粒の数・**今フレームの線の本数**）
- **Pause / Step 1 Frame**（一時停止・コマ送り）・**Invincible**（無敵）・**Show Hitboxes**（足元の判定を線で表示。スライム=緑/平ら=水色、敵=桃、乗っている物=白、弾=黄、味方の弾=緑）
- **Kill All Enemies**（まだ出ていない敵も含めて全滅＝雑魚フェーズはクリアになる）・**Boss Damage +1**（本物の当たりと同じく、ボスが攻撃中・のけぞり中のときだけ効く）
- **フェーズ一覧と Jump**（そのフェーズの層から HP 満タンでやり直す）・**Reload Phases**（phases.json の再読み込み。エラーは赤字）
- **Reload Input**（Input.json の再読み込み）・**Debug Camera (orbit)**（筒を横から見る。右ドラッグで回転・ホイールで距離）
- **Force Clear / Force GameOver / To Title**（シーンの結果の確認）
- **Event log**（取り込んだ・跳ね返した・ボスに当たった・被弾の原因 などの最近の出来事）

ほかに **Global Variables**（全調整項目）と **Scene Debug**（どのシーンへも直接飛べる）。Debug ビルドはタイトルを飛ばしてフェーズ1から始まる。

---

## コード構成

| ファイル | 役割 |
| --- | --- |
| `main.cpp` | エンジンの手動初期化・メインループ・シーン管理・調整項目の登録（テンプレートのまま＋ゲームのクラスの登録） |
| `GameScene.h/.cpp` | ゲーム本体：フェーズの状態遷移（Intro/Battle/Cleared/Advance/Victory/Defeat）・描画のパス・HUDへの受け渡し |
| `GameSceneInteraction.cpp` | GameScene の当たり判定：乗る・取り込む・接触・跳ね返す・爆発・弾の生成要求の処理・プール |
| `GameSceneDebug.cpp` | GameScene の ImGui（Game Debug）・出来事のログ・判定の可視化 |
| `Field.h/.cpp` | 輪の描画（箱のセグメント・縁と縦線）と極座標のヘルパー（`PolarToWorld`・`SurfaceRotationZ`・`ArcDistance`・`Footprint`/`Overlaps` など）・ライトの設定 |
| `GameCamera.h/.cpp` | 固定カメラ・層の移動の補間・画面揺れ・デバッグカメラ |
| `Player.h/.cpp` | スライム：速度・平ら/通常・バネの見た目・目・HP・無敵・足元の判定 |
| `Enemy.h/.cpp` | 雑魚（walker/shooter/bomber）：出現・歩く・撃つ（弾の生成要求）・乗せられる・吸い込まれる |
| `Projectile.h/.cpp` | 弾（トゲ/爆弾）：地上・空中（予告マーカー）・乗せられる・吸い込まれる・跳ね返し（転がる/ボスへ飛ぶ）・導火線 |
| `Boss.h/.cpp` | ボス（箱のゴーレム）：登場・攻撃（空中弾の投げ方）・被弾・逃走・待機・撃破演出 |
| `PhaseData.h/.cpp` | phases.json の読み込み・検証・既定構成 |
| `Effects.h/.cpp` | 爆発・取り込みの輪・火花（上限付きのプール） |
| `Hud.h/.cpp` | HP・フェーズ表示・ボスのHPバー・バナー・操作説明（white1x1 の板＋DebugText） |
| `GameModels.h/.cpp` | 共有の仮モデル（box・ボス専用の box・spike・球・光る球）とライト |
| `LineDrawer.h` | PrimitiveDrawer の線を本数を数えながら描く（上限 4000 本。見積もりは冒頭のコメント＝最大 3274 本。GameScene.cpp の static_assert で確かめている） |
| `GameInput` / `GlobalVariables` / `Fade` / `Easing` / `MathUtil` / `Matrix4x4` / `Sounds` / `Resources` / `PauseMenu` | テンプレートの基盤（GameInput に `kFlatten`、Sounds にゲームの音、Easing に `EaseOutBack` を追加） |
| `TitleScene` / `GameOverScene` / `GameClearScene` | テンプレートの仮画面（タイトルの文字だけゲーム用に変更） |

**1フレームの流れ（GameScene::UpdatePlay）**：フェーズの時間経過 → 雑魚の出現 → スライム・ボス・雑魚・弾の更新（攻撃は要求を積むだけ。弾の爆発などはその場で処理）
→ 当たり判定（乗っている物の追従 → 平らなら乗せる → 離した瞬間に取り込む → 通常の接触 → 味方の爆弾と敵 → 吸い込み）→ 弾の生成 → エフェクト・カメラ → クリア/敗北の判定。
見た目（行列・色）は一時停止中も毎フレーム `UpdateVisuals` で更新する。

---

## 未決事項・TODO（チームで決める）

- **ボスの攻撃と倒し方**：今は「空中弾を投げる → 爆弾を跳ね返して当てる」の仮実装。本番の攻撃パターン（レベルごとの違い・HP の段階での変化など）と倒し方を決める（Boss.cpp の `Throw` と状態を差し替える）
- **モデル**：スライム・虫・ボス・弾・フィールドの本番モデル（今は箱と球）。スライムの「平ら」の見た目（今は楕円体を潰しているだけ）
- **BGM**：bgm_main / bgm_boss を用意する（置けば鳴る）。効果音も本番の音に差し替える
- **タイトル・ゲームオーバー・クリア画面**：今はテンプレートの仮画面（文字だけ）
- **バランス**：スライムの速さ・敵の数と配置（phases.json）・ボスの体力・無敵時間など。何も操作しないと最初の雑魚フェーズで十数秒で倒れる程度の難しさ
- **ルールの確認**：通常状態で敵に当たったとき（今は被弾＋walker を弾き飛ばす）／トゲを食べたときの回復（今は0）／フェーズクリアの回復（今は0。
  HP5 で6フェーズは初めての人には厳しめ。Player > PhaseClearHeal を1にすると楽になる）／平らのときの移動速度（今は通常と同じ）／
  平らのまま押しっぱなしで進み続ける戦法（今は制限なし。敵は乗せている間は攻撃しないので、雑魚の少ないウェーブはまとめて取り込めてしまう。
  爆弾を乗せると導火線切れ・飲み込みでダメージになるのが歯止め。必要なら平らでいられる時間の上限などを足す）／空中弾の着弾点の予測の強さ
- **演出**：層を移るときのスライム（今は輪の間を浮いて移動する）・ボスの登場・HUD の本番デザイン・日本語の文字（DebugText は英数字のみ。画像にする）
- **見た目の調整**：輪の色・ライト・背景（Field グループで調整できる）

---

## チームの手引き（テンプレート由来）

3人チームの授業制作で何度も使ってきた KamataEngine の手動初期化（1920×1080）と、ハマりどころの対策をまとめたテンプレート（C:\KamataEngineTemplate）から作っている。
エンジンの注意点は **CLAUDE.md の「エンジンの注意点」** にまとめてある。

### ビルド・実行

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" "DirectXGame\DirectXGame.sln" /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

| 構成 | ImGui | 最初のシーン | 最適化 | 用途 |
| --- | --- | --- | --- | --- |
| Debug | あり | **ゲームシーン直行**（フェーズ1） | なし | 普段の開発 |
| Develop | あり | タイトル | **なし**（`/MT`・全体最適化のみ） | タイトルから通しで確認 |
| Release | なし | タイトル | サイズ優先 | 提出・配布 |

- 実行ファイルは `Generated/Outputs/<構成>/DirectXGame.exe`。**作業フォルダを `DirectXGame/` にして起動**する（Resources を相対パスで読むため。VSから実行すれば既定でそうなる）。
  配布時は exe の隣に `Resources/` を置けば、どこから起動しても動く（main.cpp が exe の隣の Resources を見つけると作業フォルダを切り替える）
- ビルド後イベントで `dxcompiler.dll` `dxil.dll` が exe の隣へ、エンジンのシェーダーが `DirectXGame/Resources/EngineShaders/` へコピーされる（実行時にコンパイルされる）
- **Debug は起動に数秒かかる**（デバッグ版のPNG読み込みが遅い。白い画面のまま少し待つ）
- 警告レベル W4 ＋ 警告をエラー扱い（`/WX`）・`/utf-8`。ソースは BOM なし UTF-8・タブインデント（日本語を含む .ps1 だけは BOM 付き）

### フォルダ構成

```
（ルート）
├─ DirectXGame/            … ゲームのプロジェクト（sln / vcxproj / ソース）
│   ├─ main.cpp            … 初期化・メインループ・シーン管理
│   ├─ GameConfig.h        … ウィンドウタイトル・画面サイズ・1フレームの時間
│   ├─ *Scene.h/.cpp       … 各シーン（GameScene は GameSceneInteraction.cpp / GameSceneDebug.cpp にも分かれている）
│   ├─ （ゲームのソース）   … Field / Player / Enemy / Projectile / Boss / Effects / Hud / PhaseData / GameCamera / GameModels / LineDrawer
│   ├─ （基盤のソース）     … GameInput / GlobalVariables / Fade / Easing / MathUtil / Matrix4x4 / Sounds / Resources / PauseMenu
│   └─ Resources/          … 実行時に読む素材（作業フォルダ基準の相対パス）
│       ├─ GameData/       … Input.json、Stage/phases.json、GlobalVariables/<グループ>.json（Saveで作られる）
│       ├─ sounds/         … wav（SE・BGM）
│       ├─ box/ spike/     … 仮モデル（Resources/<名前>/<名前>.obj の形）
│       ├─ cube/           … Model::Create() の既定モデル（エンジンが使う。消さない）
│       ├─ white1x1.png debugfont.png … エンジンが使う（消さない）
│       └─ EngineShaders/  … ビルド時に自動生成（Git管理外）
├─ External/               … エンジンと外部ライブラリ（シェーダー修正入り。下記）
├─ Tools/                  … ConvertSounds（mp3→wav）・PlaceholderSounds（仮の効果音の合成）
│                             （新しいプロジェクトはテンプレート側の C:\KamataEngineTemplate\Tools\NewProject で作る。ゲームのプロジェクトには置かない）
└─ Generated/              … ビルド成果物（Git管理外）
```

### シーンの流れ

`Title →（決定）→ Game →（Result）→ GameOver（リトライ→Game / タイトルへ）・GameClear（→Title）`。
各シーンは `IsFinished()`（と `GetResult()`）で終了を知らせ、`main.cpp` の `ChangeScene()` が次のシーンを作る。
**Scene Debug ウィンドウ**（Debug/Develop）でどのシーンへも直接飛べる。

シーンを増やすときは、`main.cpp` の次の場所に1つずつ足す。

- `Scene` 列挙子・シーンのポインタ・`Start～()`
- `ChangeScene` / `UpdateScene` / `DrawScene`
- `DeleteAllScenes`（終了時とシーンジャンプで使う一括破棄。足し忘れると、そのシーンで終了したときにモデルやスプライトが解放されない）
- Scene Debug の表示名とボタン・`ApplyRequestedScene`

Global Variables ウィンドウと Sound の音量の反映は、`main.cpp` が GameScene 以外のシーンで自動で行う
（GameScene のように自分で `GlobalVariables::Update()` を呼ぶシーンを作ったときだけ、`main.cpp` のその条件から除外する）。

### ファイルを追加するとき

新しい .cpp / .h を作ったら **`DirectXGame.vcxproj` と `DirectXGame.vcxproj.filters` に手で追加**する
（VSの「追加」から作れば自動で入る。エディタやAIで作ったときは忘れがち。filters は 基盤 / シーン / オブジェクト / 演出・UI に分けてある）。

### 調整項目の足し方（GlobalVariables）

```cpp
// .h：調整できる定数は static inline の k 接頭辞
static inline float kMoveSpeed = 0.15f;
// .cpp：RegisterGlobalVariables（既定値の登録。main起動時に呼ばれる）
globalVariables->AddItem(kGroupName, "MoveSpeed", kMoveSpeed);
// .cpp：ApplyGlobalVariables（値の反映。ゲーム中のクラスは GameScene::ApplyAllGlobalVariables が毎フレーム呼ぶ）
kMoveSpeed = std::max(globalVariables->GetFloatValue(kGroupName, "MoveSpeed"), 0.0f); // 壊れる値は制限する
```

- 型は int32_t / float / Vector3 / bool（Vector2 は無いので Vector3 の z を使わずに入れる）
- 新しいクラスを作ったら `main.cpp` の Register / Apply の並びと、`GameScene::ApplyAllGlobalVariables` に1行ずつ足す
- JSON を手で書き換えてもよい。小数の項目に `1` と書いても登録した型（float）にそろえて読む。型が合わない値は使われない（既定値になる）

### 入力（GameInput / Input.json）

- エンジンの Input（DirectInput）は使わない（Windowsキーが無効化されるため）。`GameInput` がキーボード（GetAsyncKeyState）とパッド（XInput）を直接読む（ウィンドウが前面のときだけ）
- ボタンの割り当ては **`DirectXGame/Resources/GameData/Input.json`**（書ける値はファイル内の `_readme`）。実行中は Game Debug の **Reload Input** で反映。書き間違いは赤字で表示され、既定の割り当てで動き続ける
- 移動軸は `GetMove()`（WASD・Lスティック・十字キー。各軸 -1～+1）。このゲームは x（A/D）だけを速度の調整に使う。メニューの上下は `GetUiMoveY()`
- **アクションの増やし方**：`GameInput.h` の `Action` 列挙子・`kActionCount`・`kDefaultBindings`、`GameInput.cpp` の `kActionNames` に同じ順で1つずつ足す（数が合わないと static_assert で止まる）。必要なら Input.json にも書く

### サウンド（Sounds）

- **wav のみ**（エンジンの Audio は mp3 不可）。ファイル名は英語のみ
- 増やし方：`Sounds.h` に extern、`Sounds.cpp` に実体・`sSoundEntries` の行（個別音量）・`LoadAudio` の `LoadWaveIfExists` を1行ずつ足す。鳴らすのは `PlaySe(h)` / `PlayBgm(h)` / `StopBgm()`
- **ファイルが無いサウンドはスキップされ、鳴らないだけで起動できる**

### 画像・文字（Sprite / DebugText）

- スプライトは `Sprite::Create(ハンドル, 位置, 色, アンカー)` で作り、**作った側が delete する**。座標はピクセル・左上原点・下が+y。1フレーム1024個まで（DebugText の1文字も1個）
- 単色の板は `white1x1.png` を `SetSize` で引き伸ばして `SetColor` する（フェード・ポーズ・HUD の板はこれ）
- `DebugText` は **ASCIIのみ**（日本語は画像にする）。`Print(std::format(...), x, y, 倍率)` で予約し、スプライトパスの中の `DrawAll()` で描く。中央揃えは `PrintDebugTextCentered`

### 3D描画の順番（GameScene::Draw）

```cpp
Sprite::PreDraw(cmd); background->Draw(); Sprite::PostDraw(); dxCommon->ClearDepthBuffer(); // ① 背景の板（3Dより奥）
Model::PreDraw(Model::CullingMode::kNone);  /* 輪・ボス・敵・弾・スライムの目・火花 */ Model::PostDraw();      // ② 不透明
Model::PreDraw(Model::CullingMode::kFront); /* 爆弾の縁取り（光る球の裏側） */ Model::PostDraw();             // ③ 縁取り
LineDrawer::BeginFrame(&camera); /* 輪の縁・縦線 */                                                          // ④ 3Dの線（Pre/Post の外）
Model::PreDraw(kBack, kNormal, kReadOnly);  /* スライムの体・取り込みの粒 */ Model::PostDraw();              // ⑤ 半透明
/* LineDrawer：着弾予告マーカー・判定の可視化（スライムの体に隠れないよう半透明の後） */                       // ⑥ 3Dの線
Model::PreDraw(kBack, kAdd, kReadOnly);     /* 爆発・爆弾の火花（glowSphere） */ Model::PostDraw();          // ⑦ 加算
Sprite::PreDraw(cmd); hud.Draw(); pauseMenu.Draw(); DebugText::DrawAll(); Sprite::PostDraw();               // ⑧ HUD
fade.Draw();                                                                                                 // ⑨ フェード
```

- 物は `WorldTransform`（描く物1つにつき1つ。親子は親を先に `UpdateWorldTransform`）と `ObjectColor`（色ごとに1つ）を持ち、Initialize してから使う
- 輪の内面に立つ物は `Field::PolarToWorld(θ, h, z)` の位置に置き、`rotation_.z = Field::SurfaceRotationZ(θ)`（ローカル+Y＝上、+X＝進む向き、−Z＝カメラ側）

### エンジンに入れてある修正（エンジンを更新したら消さないこと）

- `External/KamataEngine/shaders/SpritePS.hlsl`：テクスチャ番号に `NonUniformResourceIndex` を追加（無いと複数テクスチャのスプライトの境界に別テクスチャの欠片が出る）
- `External/KamataEngine/shaders/ObjPS.hlsl`：完全に透明なテクセルを `discard`（無いと透明部分まで深度が書き込まれ、奥の物がその形にくり抜かれる）
