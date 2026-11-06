# Physical Model Guitar 1.4 Research — Magnetic Attack & Nylon Treble

## 研究対象

- **Electric**: 弦の横速度を有限幅の磁束結合場で積分し、近似電磁起電力を構成。高速／低速包絡線に応じたコイル負荷と高域バイパスで立ち上がりと高次倍音を調整する。
- **Classical Nylon**: 既存の2地点受動ブリッジから読み取った速度を低域・中域・高域に分割し、高音域の音圧放射係数を別に制御。空洞共鳴と弦直接音は区別。
- **Steel**: 今回の新観測器では一切変えない負の対照。

1.4は弦・ブリッジ波導そのものへフィードバックしない**観測専用の物理近似**であり、測定した磁束密度、電気インピーダンス、スピーカー／空間を含む絶対的なPa/N伝達関数ではありません。特にアコースティックでの音響放射によるエネルギー損失の反作用は未同定です。

## 比較

- `1.2 UNIFIED / 1.1 HYBRID`をON。
- Electricで`1.3 MAGNETIC COIL`をON、`1.4 MAGNETIC ATTACK / 1.3`を切替。
- Nylonで`1.3 NYLON RADIATION`をON、`1.4 NYLON HIGH BAND / 1.3`を切替。
- 1.4の独立調整`1.4 Magnetic Transient Detail`、`1.4 Nylon High-Register Radiation`はホストのパラメータ一覧から0..1で調整可能。
- 1.4の新機構はデフォルトOFF。1.3のDSPや初期音を保存。

## 検証

- CTestにて3種類・44.1/48/96kHz・非干渉性・高音域・パラメータ有効性・1.3経路の回帰を検証。
- 外部録音は参考特徴量。Electric DIはHFのCC BY-SA参照（再配布なし）、NylonはMeta Open SoundのCC0 OGGプレビュー。**ギター・マイク・強弱・ピッキングが統一されていない**。
- オンラインの盲検聴取は未実施。実録音との誤差が下がっても人間の「リアルさ」は保証されない。

## 参考文献

- IEEE pickup modelling: https://ieeexplore.ieee.org/document/4292302
- JAES magnetic pickup measured response: https://aes.org/publications/elibrary-page/?id=19201
- Soundboard high-frequency radiation: https://onlinelibrary.wiley.com/doi/10.1155/2016/6084230
