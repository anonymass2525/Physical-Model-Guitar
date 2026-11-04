# Physical Model Guitar 0.3 — Acoustic Realism (DSP研究プロトタイプ)

0.2とは**別製品識別子**で共存できる新物理モデリングギター。以前の0.2音色はこのソースにはありません。
現在は **Linux上のJUCE非依存C++ DSPコードを実際にビルド・テスト済み**です。
**Windows x64 VST3はビルド設定とソースまで用意していますが、Windows実機でのビルド・ロード・pluginval・Studio One検査はまだ行っていません。**

## 5つの物理モデル

1. **Pick/Finger Contact**：ピック先端の軌道、コンプライアンス（弾性）、相対速度から力を計算し、解放時にはスリップによって波動を励起。硬さ、位置、角度の変更が発音に反映。
2. **Dual-Polarization**：弦ごとに独立した2方向の巡回ディジタル導波路、異なる分散・損失・波速。Pick Angleで励振比率を変更。
3. **Shared Bridge + Body**：全6弦が同じ10モードの共鳴体を駆動。共鳴体の速度フィードバックが6弦へ戻る。鳴らしていない弦にも共鳴が発生（sympathetic vibration）。
4. **Fret/Finger**：弦変位がフレットクリアランスを超えた時の片側接触とバネ・ダンパー反力。発生条件はFret Clearance/Touchで変化。ノートオフでは押弦のダンピングをモデル化。
5. **Electric Pickup**：ピックアップ位置・有限幅の3点加重積分、2コイル模擬、変位の時間差分による誘導信号、負荷の2極近似。アンプシミュレーターは未使用。

これらは**物理現象に基づく簡略モデルを備えたという意味**であり、計測された実物と同じ波動・材料特性を再現できたことを意味しません。フレットの接触は簡略scattering、ボディは計測FEMではなく10モード近似、ピックアップの電気回路はRLCの完全な部品値ベースではなく2極の近似です。

## 4種類

- **Hybrid Guitar**：本プロトタイプのニュートラルな弦・ボディモデル。
- **Electric**：磁気ピックアップと負荷フィルターを通したDI音。
- **Steel Acoustic**：金属弦・高域減衰・ボディ放射。
- **Classical Nylon**：ナイロン弦・柔らかい励振・遅めの高域減衰。

全モデルで6弦・2方向・共通ブリッジを使用します。MIDI範囲は E2 (40) から E6 (88)、各弦0〜24フレット、最大6ノートで実物に近い運指制約です。同じ弦の後続ノートは前ノートを中断します。音色・運指の最適性、弦間共鳴の物理的精度は研究途中です。

## テストと試聴

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPMG_BUILD_PLUGIN=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
mkdir -p Audio/Preview
./build/PMGOfflineRender Audio/Preview/PMG03
```

`Audio/Preview` に48kHz、16bit、ステレオ、各9秒の**未加工**試聴用WAVが付属します。`Audio/Matched`には同じ音をRMS -22 dBFSに音量合わせした4本があります（ゲイン以外のエフェクト処理なし）。同一フレーズの聴感差の比較には`Audio/Matched`を推奨します。

## WindowsでVST3を生成

Windows 10/11、Visual Studio 2022（C++デスクトップ開発）、CMake、Git、ネット接続を準備して、`BUILD_WINDOWS.bat` を実行してください。初回はJUCE 9.0.4をGitHubから取得します。Linuxのこの環境ではJUCE取得ができないためVST3は未ビルドです。

GitHub Actions `windows-v03.yml` も同梱。成功時に90日保存のVST3アーティファクトを出力する構成（**未実行／未検証**）。

## 既知の限界

- 波動散乱と非線形接触の簡略化による、励振・うなり・余韻の誤差。
- 弦材ごとの物理パラメーターは実測キャリブレーション前。高精度ピックアップ磁束・正確な導電性／インダクタンス計測は未対応。
- 指とフレット間の完全な接触制約、押弦位置連続移動、接触のエネルギー保存型陰解法は未実装。Pitch Bendは遅延長を滑らかに変える近似。
- 16ボイスの汎用楽器ではなく6弦楽器。単一MIDIチャンネルの全弦一括Pitch Bendです。
- ボディ共鳴サイズ変更は45ms係数補間で状態を維持しますが、厳密な時変物理エネルギー保存は未証明です。
- 現時点ではギター実音との主観評価・ブラインドAB比較・DAW録音・ライブ入力試験が必要。

参照：
https://ieeexplore.ieee.org/document/5446590/
https://www.dsprelated.com/freebooks/pasp/Pluck_Modeling.html
https://www.ness.music.ed.ac.uk/archives/systems/guitarfretboard-interactions.html
