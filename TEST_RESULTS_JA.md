# Physical Model Guitar 0.3 Acoustic Realism — 実際の検査結果

検査日: 2026-11-05 / Linux x86_64 / C++17 / g++ / CMake / Ninja

## コードの変更
- 元ユーザー提供物「PhysicalModelGuitar-0.2-FourModels-Windows-x64.zip」を参考に別ディレクトリで開発。元のZIPには書込みなし。
- ギターの4モデルを継承しつつ、DSP本体は6弦×2偏極系＋接触演算＋10モード共鳴胴へ刷新。
- 旧版プラグインとの同時共存を想定し、VST3識別子と表示名を別途設定。

## 成功した検査（JUCE非依存のDSPのみ）
| ケース | 結果 | チェック内容 |
|---|---|---|
| PMG03_basic | PASS | 4モデル、44.1/48/96kHz、初期無音、出力、有限性、最大音量、6弦上限、Note Off減衰 |
| PMG03_physics | PASS | ピック角度/硬さ、弦材・2偏極、10モードの音色変化、ブリッジ運動、フレット衝突数、電磁ピックアップの可変応答 |
| PMG03_interaction | PASS | 未演奏弦への共鳴エネルギー移動、ピッチベンド、モデル切替40ms |
| PMG03_stability | PASS | 96kHz × 4モデル、6弦同時、最大の減衰と結合、ピッチ変化、3秒間の有限性 |
| PMG03_realtime | PASS | 1500ブロック×256サンプル、モデル変更、Note Off、pitch wheel、音声コールバック中のC++ new/new[] 0回 |

最終 Release CTest: **5/5 PASS**。オフライン処理時間は0.239秒／音声長8秒（比0.0299）。これはこのLinux環境の特定条件下での壁時計時間でありWindowsやDAWのピークCPU値ではありません。

AddressSanitizer + UndefinedBehaviorSanitizer構成で5つのテストを個別実行し全件PASS。検査中に報告された違反なし。サニタイザで一括実行した際はランタイム制限により中断されたため、残りは個別実行しています。

## 48kHz・9秒・16-bit stereo 試聴音源

`Audio/Preview`に4本収録。
- Hybrid Guitar: peak 0.069458、RMS 0.012633
- Electric: peak 0.420807、RMS 0.05136
- Steel Acoustic: peak 0.060425、RMS 0.010222
- Classical Nylon: peak 0.059784、RMS 0.011017

`Audio/Preview`は未加工です。`Audio/Matched`に、波形へ固定ゲインのみ適用しRMSを-22 dBFSで合わせた比較用WAVを追加しました。素材音の自然さを客観評価するには実ギターDI/マイク録音との計測較正、同音量での聴感比較が必要です。

## 未完了・未検証
- **Windows x64 VST3のビルド、pluginval、Studio One読込、プラグインエディタの実動作は未実施**。ソース／Windowsビルドバッチ／GitHub Actions定義は用意。
- ピック・フレット・ボディモデルは工学的な低次近似。現物の材質/幾何/周波数応答の実測パラメーターや陰的エネルギー保存接触ソルバーは未実装。
- 通常MIDIノートは6弦用に自動割当。最適運指推定、継続するスライド・ビブラート、独立弦ピッチベンドは未実装。
- 実機と同じリアリティの達成、音色の好みは、現段階では保証しません。
