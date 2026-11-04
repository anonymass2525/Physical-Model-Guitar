# Physical Model Guitar 0.4 — Acoustic Realism II

0.3と別のプラグインID (`PmG4`)、別のVST3パッケージ名を使用します。0.3は上書きしません。

## 0.4の主な改善
- スチール、ナイロン、エレキ各モデルに固有のピック接触剛性と接触時間
- 非線形な弦の振幅と張力の相互作用（微小なアタック初期のピッチ変動）
- 2つの振動方向を結ぶ非常に小さな偏波間の交換
- 2方向の損失係数の独立化とモデルごとの共鳴胴Q値
- 同一MIDI、同一ベロシティの単音レンダリングCLI
- 実録音WAVとの時系列倍音・アタック・音程の比較ツール
- Inno Setup 6 によるWindows x64 VST3 インストーラー自動作成

## ビルド・インストール
GitHub Actionsの `Physical Model Guitar 0.4 - Windows VST3 Installer` を実行し、
`PhysicalModelGuitar-0.4-Windows-x64-Setup` Artifact内のEXEを実行してください。
管理者権限が必要です。ファイルは `C:\Program Files\Common Files\VST3\Physical Model Guitar 0.4.vst3` に自動配置され、Windowsのアプリ管理から削除できます。

## 検証状況
Linux/g++によるDSP単体CTest5件合格。Windows用JUCE VST3とEXEはGitHub Actionsでの実ビルドと試験が必要。
音色が実ギター録音とどれだけ一致するかは現時点で未検証。測定方法は `REAL_RECORDING_CALIBRATION_JA.md` を参照してください。
