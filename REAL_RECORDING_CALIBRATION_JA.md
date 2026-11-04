# Physical Model Guitar 0.4 — 実録音比較・校正手順

## 大前提

0.4は **実録音に対する物理パラメーターの最適化を完了したものではありません**。
0.3コードから、モデル固有の接触剛性・弦の二方向減衰・振幅依存張力・微小な偏波間エネルギー交換を実装し、ローカルDSPのテストに合格した段階です。従って「実ギターと聴き分けられない」という保証はありません。

### 参考音声の候補
- Electric：EGFxSet の clean/DI（https://egfxset.github.io/ ）
- Steel/Nylon：Meta Open Sound の実ギター録音（https://github.com/facebookincubator/Meta-open-sound ）
- Meta Open Sound のギター録音はCC0。録音条件、弦番号、ノート、ベロシティを合わせる。
- これらの録音データは本プラグインのバイナリには含めない。

### 48kHzモノ音声を比較する

    # 生ギター同一音程を WAV に用意する（例: E3, MIDI 52）
    cmake -S . -B build -DPMG_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
    cmake --build build --config Release
    ./build/PMGOfflineRender --single 2 52 0.75 PMG04_Steel_E3.wav
    python -m pip install numpy scipy soundfile
    python tools/CompareRealGuitar.py --reference Real_Steel_E3.wav --synthetic PMG04_Steel_E3.wav --midi 52 --kind steel --out steel_E3_report.json

`--single` のmodel番号は Electric=1、Steel=2、Nylon=3。録音の準備ができていれば同じ方法で比較可能。

### 比較指標
1. 発音後0–10、10–30、30–60、60–100、100–200 msの音量差（RMS一致後）
2. 発音直後のスペクトル重心差
3. 基音と2–16倍音のレベル、および各倍音の減衰傾斜 (dB/sec)
4. 初期音程偏差 (cent) と最大1秒までの時間変化
5. 倍音レベルの滑らかな指数減衰からの偏差（揺れの測度）

異なるDI、アンプ、マイク位置、録音室、EQ、ピックアップ設定では数値が大きく変わる。発音源の物理性と録音経路の違いを混同しない。特にElectricは必ず非加工DIを使用する。
複数回弾いた参照音の分布からバラツキを比較する。単発録音への最適化は過学習になりうる。

## まだ未完了
- 実録音ファイルを読み込んだ同条件での校正結果
- 実録音に基づくモデル別パラメーターフィッティング
- ブラインド聴取評価、Studio Oneでの実機試験、pluginval
- Inno Setup生成後のWindowsでのインストール/アンインストール動作
