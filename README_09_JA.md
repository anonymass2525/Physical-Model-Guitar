# Physical Model Guitar 0.9 — Passive Two-Port Bridge Coupling

## 実装範囲

v0.8を独立プラグインとして残す。v0.9で **6個の弦側波ポートから2つの橋位置（低音側・高音側）に力を伝える受動的な散乱境界** を新規実装した。新規モジュールは `DSP/TwoPortBridge.h/.cpp`。

弦側ポートは6本で、インピーダンス Zi>0 と正規化入射速度波 ai を持つ。2地点への投影 W は弦番号別の決定論的重み。2地点の受動アドミタンスは、質量 m>0、Q>0 のモード10個、方向ベクトル d による `Y(ω) = Σ d d^T * (jω)/(m(ω0² - ω² + j γ ω))` で表現する。周波数応答は対称/相反・正実。ブリッジの時間更新は陰的中点法。6弦の波散乱とボディは各サンプルで1回の2x2直接解法で結ぶ。

一方で従来の弦波導はディスプレースメント差分履歴の擬似波導であり、端点変数も SI 単位への実測校正を終えていない。v0.9の厳密な収支式の**検証対象は新設した6波ポート＋2地点ブリッジに限る**。旧波導の物理的総エネルギーと「完全一致した」と解釈しないこと。弦の力波・速度波を完全に定式化し直すのが次段階。

## 電力収支

各弦 i の入射速度波 a_i、反射速度波 b_i、波インピーダンス Z_i、結合後端点速度 v_i:

- `b_i = v_i - a_i`
- `f_i = Z_i(2*a_i - v_i)`
- `sum_i Z_i(a_i² - b_i²) = sum_i f_i*v_i`
- `V=[v_bass,v_treble], F_bridge=W*f, v=W^T*V`
- `E_wave_in-E_wave_out = E_body_stored + E_body_dissipated`（初期エネルギー0の場合、丸め誤差まで）

受動性・相反性は `Y` のPSDモード剰余行列と正のZにより保証。実音響放射はモード速度の観測信号であり、マイクで観測される音響エネルギーまで包含したものではない。

## 検査

- `ctest -j 6`: 10件のネイティブDSPテスト。専用テストで44.1/48/96kHz、6弦結合、相互伝達、反射電力、蓄積/散逸の恒等式。
- `--physics09=0`: v0.8に戻す。`--physics08=1 --physics09=1`: 新2ポート境界。
- `python tools/BlindAB09.py --render build/PMGOfflineRender --output ab09` 音量一致12組比較。
- `python tools/Physics09ReferenceBenchmark.py --render build/PMGOfflineRender --out benchmark09` 実録音の未同定資料との特徴量誤差。検証の結果悪化する場合も報告する。

## 実測橋データとの関係

Zenodo 4604577（65台の記録）には2地点の加速度と力の記録があり、以前の0.8で低音側・高音側および相互伝達を個別に解析した。これらは2つの*空間位置*であって、弦の直交2平面ではない。0.9では物理的に許容される2x2行列実装を優先し、未同定の自由度を勝手に「実測値」と呼ばない。現プリセットは初期の受動的推定値。計測値でモデル同定を改めて行う必要がある。

## インストール

Windows x64で `PhysicalModelGuitar-0.9-Windows-x64-Setup.exe` を実行。既存0.8とVST3の名前・IDが異なるため共存。Studio Oneで `BRIDGE 0.9 TWO-PORT / 0.8: A/B` を比較。主機構の比較時は他の A/B を固定する。

## 参考

- R. Mores, Zenodo 4604577: https://zenodo.org/records/4604577
- E. Maestre et al., Joint Modeling of Bridge Admittance and Body Radiativity ... (2017), doi:10.1109/TASLP.2017.2689241
- DAFx 2026 Measurement-Informed Nonlinear Modal Synthesis of 65 Classical Guitars: https://www.dafx.de/paper-archive/details/KZ0Ws85vmVK5-Tceo9RZ1w
