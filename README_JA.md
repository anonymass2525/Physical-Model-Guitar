# Physical Model Guitar 0.6 — Physical Realism III

物理ギター研究版。各弦の2直交偏波による横振動に、1個の有効縦振動自由度とピック接線摩擦を追加。縦方向のモデルは完全な波動方程式の離散化ではない。固有振動数は実測同定されていない。

- VST3製品名: Physical Model Guitar 0.6、Plugin Code: PmG6 (0.5と共存)
- Physics 0.6 / 0.5 Blind A/B は初期ON、OFFで従来の計算経路。
- Real Recording Calibration は独立トグルで初期OFF。0.5の限定的な採用候補を維持。
- Windows x64 EXEインストーラーは Inno Setup 6。
- 同一ギターでの複数テイク、ピッキング位置と速度、弦ゲージ、録音回路を揃えない限り、実物の物性は同定できない。
- Bridge mobility は force input F(t) と同時計測速度 v(t) から Y(f)=V(f)/F(f) として求める。マイク音圧だけで同定した値は等価音響モデルと呼ぶ。
- ブラインドA/Bは音量一致の合成音同士。実ギターとの識別テストではなく、聴取前の勝敗もない。
- GitHub Actionsでは実ギター音源の特徴量を計算して改善・悪化を両方報告する。参照録音はVST3に含めない。

研究: https://www.dafx.de/paper-archive/details/KZ0Ws85vmVK5-Tceo9RZ1w
測定: https://pmc.ncbi.nlm.nih.gov/articles/PMC12609871/
ピック軌道: https://vibsys.put.poznan.pl/article/the-effect-of-micro-changes-in-the-pluck-trajectory-on-the-sound-of-an-acoustic-guitar/
