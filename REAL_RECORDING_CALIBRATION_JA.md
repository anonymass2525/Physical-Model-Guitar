# 実ギター録音と物理モデルの比較校正（研究版）

対象: Electric / Steel Acoustic / Classical Nylon。実録音をプラグインに搭載するのではなく、**DSPの外側で特徴量を計算して校正**します。音源は一貫して物理演算による合成です。

参照資料:
- Electric: `collegefishiesd/guitar-fretboard-notes` (`source=ele`、Stratocaster DI。CC BY-SA 4.0) https://huggingface.co/datasets/collegefishiesd/guitar-fretboard-notes
- Steel / Nylon: Meta Open Sound, 各弦・音高・強弱が識別できる録音。CC0 1.0 https://github.com/facebookincubator/Meta-open-sound
- Metaの本リポジトリにあるOGGは**プレビュー用非可逆圧縮**です。ここでの結果は予備校正であり、最終校正には48kHz/24bitの正式WAV、適切なDIまたはマイク条件が必要です。

## 手順

```
python -m pip install numpy scipy soundfile
cmake -S . -B build -DPMG_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build build --config Release
python tools/CalibratePhysicalModel.py --render build/PMGOfflineRender --out calibration-results
```

`tools/OfflineRender.cpp`に任意の校正用パラメータ (`--brightness`, `--hardness`, `--decay`, etc.) を追加し、DSP内部の発音条件を変えて比較します。

## 比較内容

- **学習**: E2 / A2 / D3（弦とフレットを明示して突き合わせる）。
- **検証**: G3 / B3 / E4（学習に使わない音高）。
- 0-15 / 15-40 / 40-80 / 80-140 / 140-280 / 280-550ms のレベル曲線。
- 1〜8倍音の時間依存減衰と初期相対強度。
- 初期から後期にかけた相対音高変化。
- 重み付け距離は**測定用の代理指標**。実ギターとの聴感等価性を表さない。
- 元の音量・絶対的なEQ特性を単純に一致させるとマイクやピックアップ条件に過剰適合するため、相対特徴量を優先。
- 学習と保留ノートで両方改善しなければ`accepted_by_holdout_gate=false`。改善判定されても実機での聴き比べを経るまではプラグインの既定値に自動で反映しない。

参照ファイルはCI実行中のみ取り込み、Artifactにアップロードしません。必要なライセンス表記と元URL、解析値のみを保存します。
