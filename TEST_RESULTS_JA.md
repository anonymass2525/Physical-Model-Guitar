# Physical Model Guitar 0.4 — ローカル検証

環境: Linux g++ / CMake, JUCEを除くDSP単体。

- PMG04_basic : PASS（44.1/48/96kHz、全モデル）
- PMG04_physics : PASS
- PMG04_interaction : PASS
- PMG04_stability : PASS
- PMG04_realtime : PASS（処理中newゼロの検査）
- 48kHzのElectric/Steel/Nylon単音WAVを生成。
- `CompareRealGuitar.py`の同一ファイル照合セルフテストは差分0で正常終了。
- 過去v0.3と同じフレーズで同一モデルの差分波形が存在することを確認。

**未実施:** 実録音の同条件比較、Windows JUCEビルド、インストーラーの導入・削除、Studio One、pluginval。Windows CI実行後に追記すること。
