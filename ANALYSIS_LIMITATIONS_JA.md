# Physical Model Guitar 0.5 — 実録音からの弦別・帯域別校正

## 実装した校正可能な物理量

- **各弦6本それぞれの接触剛性倍率** `contact0`～`contact5`：0.4～2.5倍。発音直後の物理的な弦の押し込み／離脱の応答に作用。
- **各弦の4帯域損失** `loss00`～`loss53`：各弦の基音寄り・低次・高次・超高次倍音の追加減衰を制御。制約 `loss0 <= loss1 <= loss2 <= loss3` と非負重み付きローパス混合によって高域散逸を設計（FFT/EQの後処理ではなく周回する波動にフィルタ）。**4帯域の近似**であり、各倍音1本ずつの減衰を厳密に完全同定したものではない。
- **各弦ピックアップの相対位置** `pickup-offset0`～`pickup-offset5`。実際の磁束の空間分布／ピックアップ高さまで特定できるわけではない。
- **10のボディ機械モード** `mode0-freq`、`mode0-q`、`mode0-gain` ～ `mode9-*`。周波数・Q・応答強度を独立に変更可能。正の範囲に制限し、変更時はモード状態を維持。
- **使用弦の明示** `--string=0..5`：低E弦から高E弦まで。MIDI 52を低E弦12フレットで発音する場合など、同じ音高を違う弦で弾く場合も比較可能。

## データと評価

- Steel: Meta Open Sound Steel Guitar、CC0 1.0、6弦・3強弱層、最大375録音。
- Nylon: Meta Open Sound Nylon Guitar、CC0 1.0、6弦・指弾き＋ピック、最大348録音。
- Electric: HuggingFace `collegefishiesd/guitar-fretboard-notes`、CC BY-SA 4.0、エレキのDI単音。録音者IDを混在させず、低E～高Eの弦とフレットを照合する。
- 今回の自動実験は、各弦**開放・3・7フレットを学習**、**5・9フレットと別の強弱・奏法を保留検証**。収集可能な最大数はSteel 30件、Nylon 30件、Electric 30件程度で、欠損を含み得る。
- 実録音と合成音をオンセット合わせし、アタック（6窓）、最大8次倍音の強度と減衰勾配、短時間ピッチ変化を比較。弦ごとに頑健な候補を探索、**保留スコア3%以上の改善＋アタック悪化12%以内**なら候補として記録。自動的にプラグイン初期値には適用しない。

## 研究上の注意

1. **構造的な同定可能性の限界：** 単一マイク／DIの収録波形のみでは、接触剛性の絶対値[N/m]、弦材の損失、ピックアップの空間応答、ボディの固有振動の寄与を一意には分離できない。これらは有界な**等価モデルパラメーター**であり、実物ギターの物性を正確に測った値と誤解しない。
2. 特にボディ10モードの周波数・Q・強度を測定値として確定するには、共通ブリッジへの既知の加振、複数位置のマイク／加速度計、インピーダンス測定などが望ましい。今回のプログラムでは**調整可能だが「実測から同定済み」としない**。
3. 単一テイクかつ圧縮OGGの録音を多数使っても、人間の演奏間変動・ピッキング角度・弦材や録音室の差を代表できない。可能ならマルチテイク24bit WAVで再校正。
4. 実録音は校正解析にのみ使い、VST3本体には取り込まない。試聴音声と参照音声を同一の著作物として再配布しない。
5. 推定の学習結果は**全弦一括適用せず、保留弦と音高で効果を検証**し、Studio Oneでの聴感A/Bを通過するまで採用しない。

## ビルド

```bash
cmake -S . -B build -DPMG_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python -m pip install numpy scipy soundfile
python tools/LargeCorpusCalibrator.py --render build/PMGOfflineRender --out expanded-calibration-results
```

## 結果の見方

`expanded-calibration-results/report.json` には使用した各弦の学習／保留録音の件数、パラメーター候補、候補採否、特徴量別の誤差が記録される。`SUMMARY.md` は集約表。録音の実ファイルはGitHub Artifactには入らない。

参考:
- https://github.com/facebookincubator/Meta-open-sound
- https://huggingface.co/datasets/collegefishiesd/guitar-fretboard-notes

## 機械共鳴の実測校正（追加）

`tools/FitBridgeAdmittance.py` はギターのブリッジ加振点で測定した **2チャンネルの同期WAV**（左=加振力[N]、右=ブリッジ速度[m/s]）から機械的アドミタンスを推定し、10モードの周波数・Q・強度の**候補**を出力する。

```bash
python tools/FitBridgeAdmittance.py measured_force_velocity.wav --model steel --out candidate-body.json
```

測定には加振器またはインパクトハンマー、力センサー、速度センサー／加速度計（速度へ積分）、同一サンプルクロックの収録が必要。一般的なギター単音の録音だけからは、これらの物理モードを一意に測ることはできない。現在は振幅ピークと半値幅からの候補生成であり、位相と多方向のアドミタンス行列まで含めた厳密な同定ではない。

参考：https://euphonics.org/wp-content/uploads/2022/03/Guitar_II.pdf
