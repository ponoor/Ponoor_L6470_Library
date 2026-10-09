# Ponoor_L6470_Library v1.2.0 修正タスク

対象リポジトリ: https://github.com/ponoor/Ponoor_L6470_Library （現行 v1.1.0）
参照データシート: STMicroelectronics L6470 DocID16737 Rev 7（以下「DS」）

このファイルは、変換係数のバグ修正、デイジーチェーンのパック送信機能の追加、ドキュメントとライブラリメタデータの整備をまとめたタスク指示です。作業は新しいブランチ（例: `release/1.2.0`）で行い、下記のタスク単位でコミットを分けてください。

## 基本方針

- 既存の公開API（`AutoDriver` クラスのメソッド名、引数、定数名）は互換性を保つこと。挙動の変更はバグ修正に限る。
- SparkFunのbeerwareライセンス表記と帰属表示は残すこと。
- コード中のコメント、README、library.properties、新規ドキュメントはすべて英語で書くこと。
- 文中の数値はDSに基づいている。疑わしい箇所はDSで確認し、判断に迷う場合は実装せずにTODOとして報告すること。

---

## Task 1: ACC/DEC 換算の修正（`src/Ponoor_L6470Support.cpp`）

DS 9.1.5 / 9.1.6 では `step/s² = ACC × 2^-40 / tick²`（tick = 250 ns）と定義されている。1 LSB は 14.551915 step/s²、逆数は 0.068719477。

### 1-1. `accParse()` / `decParse()` の係数が誤っている

現在は `15.258789F` を使っているが、これは MAX_SPEED / FS_SPD 用の係数（1/0.065536）を誤って流用したもの。この影響で `getAcc()` / `getDec()` の戻り値が約4.9%大きくなっている。

```cpp
float AutoDriver::accParse(unsigned long stepsPerSecPerSec)
{
  return (float)(stepsPerSecPerSec & 0x00000FFF) * 14.551915F;
}
// decParse も同じ係数にする
```

### 1-2. `accCalc()` / `decCalc()` のクランプを揃える

- DS 9.1.5 に「0xFFF の値は予約されており使用してはならない」とある。また DS 6章では、加減速の範囲を 2^-40 〜 (2^12 − 2)·2^-40 としている。
- `accCalc()` はすでに `>= 0xFFF → 0xFFE` になっているので、`decCalc()` も同じにする。
- 下限は 1 LSB（14.55 step/s²）なので、計算結果が0になる場合は1にクランプする（両関数とも）。
- 係数の表記は `0.068719477F` に統一する。

### 1-3. `getDec()` の修正（`src/Ponoor_L6470Config.cpp`）

現在の `getDec()` は `accParse()` を呼んでいる。計算式は同じなので結果は変わらないが、`decParse()` を呼ぶように直す。

### 1-4. 古いコメントの更新

次のコメントを現在のコードとDSに合わせて書き換える。

- `Ponoor_L6470Support.cpp` の ACC 部分: "Multiply desired steps/s/s by .137438" → 0.068719477。
- `Ponoor_L6470Support.cpp` の INT_SPD 部分: "Multiply desired steps/s by 4.1943" → 16.777216（式はすでに 2^-26 に修正済み）。
- `Ponoor_L6470Support.cpp` の RUN 速度部分: "Multiply desired steps/s by 67.106" → 67.108864。
- `paramHandler()` の ACC/DEC コメント: "Set ACC to 0xFFF to get infinite acceleration" は旧データシート（Rev 5以前）の記述。0xFFF は予約値で使用不可、という内容に書き換える。
- `Ponoor_L6470Config.cpp` の `setAcc()` コメント: "Any value larger than 29802 will disable acceleration, putting the chip in infinite acceleration mode" を、範囲は 14.55〜59590 step/s² で、範囲外の値はクランプされる、という内容に書き換える。

---

## Task 2: SAMD の割り込み禁止範囲の修正

現在は `getStatus()` と `xferParam()` の中で `__disable_irq()` / `__enable_irq()` を呼んでいる。しかし `setParam()` / `getParam()` では、コマンドバイトの送信（`SPIXfer(param | CMD_...)`）がこの保護範囲の外にある。

- PRIMASK を保存して復元するヘルパー（例: `uint32_t _irqSave()` / `void _irqRestore(uint32_t)`。`__get_PRIMASK()` を使う）を用意する。ネストしても安全な形にすること。
- `setParam()` / `getParam()` / `getStatus()` / 各モーションコマンド（コマンドバイトとデータバイトの組）について、トランザクション全体を保護する。
- `xferParam()` 内の個別の禁止/許可は、ヘルパーに置き換えるか削除する（二重に保護して早く許可されてしまうのを防ぐため）。
- `#if defined(ARDUINO_ARCH_SAMD)` 以外のアーキテクチャでは、ヘルパーは何もしない。

---

## Task 3: ヘッダとインクルードガード

- `src/Ponoor_L6470Constants.h` のガード `_dspin_constants_h_` は、Ponoor_PowerSTEP01_Library の定数ヘッダと同じ名前になっている。両方を同じ翻訳単位でインクルードすると、後に読んだほうの定数が黙ってスキップされる。`PONOOR_L6470_CONSTANTS_H` に変更する。
- `src/Ponoor_L6470Library.h` のガード `AutoDriver_h` を `PONOOR_L6470_LIBRARY_H` に変更する。
- ヘッダ内でタブとスペースが混在しているインデントを、スペース2つに統一する。

---

## Task 4: パック送信（Prepare / Perform）機能の追加

### 背景

現在の `SPIXfer()` は、1バイト送るたびにチェーン全体（`_numBoards` バイト）を送信し、対象の1台以外には NOP（0x00）を送っている。そのため N 台に1コマンドずつ送ると、N×N バイトの転送が必要になる。

実例として、step-series-universal-firmware のサーボモードでは、STEP800（L6470×8）で10msごとに全台の `getPos()` + `run()` を実行している。これは `SPIXfer` 64回（512バイト）になり、推定で約1.4msかかる。ステータスのポーリングを含めると約1.9ms/10msになる。

デイジーチェーンでは、1フレームの各スロットに台ごとに異なるコマンドを入れられる。この性質を使って、全台のコマンドを最大4フレーム（コマンドの最大長）で送るようにする。STの X-NUCLEO-IHM02A1 ライブラリにある Prepare → Perform 方式と同じ考え方。

### 仕様

**インスタンスの登録**

- コンストラクタで、各インスタンスを static な配列 `_instances[]` に登録する。最大数はマクロ `L6470_MAX_DEVICES`（デフォルト 16、`#ifndef` で上書き可能）で決める。
- 既存の `_numBoards` との整合を保つこと。

**準備バッファ**

- 各インスタンスに `_prepTx[4]`、`_prepRx[4]`、`_prepLen`、`_prepBitLen`、`_prepType` を持たせる。

**prepare 系メソッド（public）**

- prepare 系メソッドはバッファに積むだけで、SPI 通信は行わない。各台に同時に積めるコマンドは1つで、再度呼んだ場合は上書きする。
- 用意するメソッド:
  - `prepareGetParam(byte param)`
  - `prepareSetParam(byte param, unsigned long value)`
  - `prepareGetStatus()`
  - `prepareGetPos()`
  - `prepareRun(byte dir, float stepsPerSec)` / `prepareRunRaw(byte dir, unsigned long integerSpeed)`
  - `prepareMove(byte dir, unsigned long numSteps)`
  - `prepareGoTo(long pos)` / `prepareGoToDir(byte dir, long pos)`
  - `prepareSoftStop()` / `prepareHardStop()` / `prepareSoftHiZ()` / `prepareHardHiZ()`
  - `prepareNop()`（準備を取り消すため）
- バイト列の組み立てやクランプ、速度の換算は、既存の即時実行版と同じロジックを共通関数に切り出して共有する。重複実装は避けること。
- レジスタのビット長は `paramHandler()` と同じ表を参照する。表を関数（例: `static byte paramBitLen(byte param)`）として切り出し、`paramHandler()` 側もそれを使うようにリファクタリングしてよい。

**一括送信（static）: `static void performPrepared()`**

- フレーム数は、全インスタンスの `_prepLen` の最大値（最大4）とする。
- 各フレーム f について、`_numBoards` バイトのパケットを組み立てる。各インスタンスの `_position` には、`f < _prepLen` なら `_prepTx[f]`、それ以外なら 0x00（NOP）を入れる。
- 準備のないインスタンスのスロットも 0x00 で埋める。0x00 が NOP であることは DS のコマンド表で定義されており、既存の `SPIXfer()` も同じ前提で動いている。
- CS を LOW にし、`beginTransaction` → `transfer` → `endTransaction` を行ってから CS を HIGH に戻す。この流れは既存の `SPIXfer()` と同じにする。受信したパケットの `_position` の値を `_prepRx[f]` に格納する。
- DS では、各バイトの後に CS を tdisCS（800 ns）以上 HIGH に保つ必要がある。フレーム間の CS HIGH 時間がこれを下回らないことを確認する（`digitalWrite` のオーバーヘッドで満たされる見込みだが、コメントで明記する）。
- Task 2 と同じく、全フレームを割り込み禁止区間で保護する。
- 終了後、`_prepLen` を 0 に戻す。結果は次の prepare を呼ぶまで保持する。
- 前提として、全インスタンスが同じ CS ピンと SPI ポートを共有していること。異なる CS / SPI ポートのインスタンスが混在している場合は何も送らず、エラーを返す。戻り値を `bool` にするか、エラー用の getter を用意する。この制約はヘッダコメントと README に明記する。

**結果の取得**

- `unsigned long preparedResult()`: 応答バイト（`_prepRx[1..]`）を組み立て、`_prepBitLen` でマスクした値を返す。
- `long preparedPos()`: ABS_POS の 22bit 2の補数を、`getPos()` と同じ方法で符号拡張して返す。
- `int preparedStatus()`: GetStatus の結果（16bit）を返す。

**互換性**

- 既存の即時実行 API はそのまま残し、挙動を変えない。

### 新規サンプル `examples/PackedCommands/PackedCommands.ino`

- 8台のチェーンを想定する（台数は先頭の定数で変えられるようにする）。
- 即時実行版（ループで `getPos()` + `run()`）とパック版（`prepareGetPos` → `performPrepared` → `prepareRun` → `performPrepared`）の所要時間を `micros()` で測り、Serial に出力する。
- サーボ制御の1周期を模した簡単な P 制御の例を含める。
- 既存の gantry サンプルと同じピン設定の書き方に揃える。

---

## Task 5: オプション改善（低優先度。実装した場合は報告すること）

- `SPISettings` のクロックを固定の 4 MHz から変更できるようにする（`setSPIClock(uint32_t)`）。デフォルトは 4 MHz のままにし、DS の最大値 5 MHz を超える値はクランプする。
- `SPIXfer()` 内の VLA（`byte dataPacket[_numBoards]`）を、`L6470_MAX_DEVICES` サイズの固定配列に置き換える。

---

## Task 6: `keywords.txt`

- 既存の内容と、ヘッダの公開メソッドおよび定数を照合し、欠けているものを追加する。少なくとも以下は必要:
  - KEYWORD2: `getSpeed`、各 `*Raw` 関数、`getElPos` / `setElPos`、Task 4 の prepare 系メソッド、`performPrepared`、`preparedResult`、`preparedPos`、`preparedStatus`（Task 5 を実装した場合は `setSPIClock` も）。
  - LITERAL1: `CMD_*` 系の定数、`REG_STATUS`。
- 旧名（`GET_STATUS`、`STATUS` など、すでに存在しない定数）が残っていれば削除する。
- 区切りは必ず単一のタブにする（Arduino IDE の仕様）。

## Task 7: `library.properties`

- `version=1.2.0`
- `sentence` と `paragraph` を自然な英語に書き直す。案は以下の通り。

```
sentence=Arduino library for the STMicroelectronics L6470 (dSPIN) stepper motor driver, with daisy-chain support.
paragraph=Based on the SparkFun AutoDriver library. Adds packed daisy-chain transfers, raw register accessors, EL_POS functions, corrected unit conversions, and SAMD (ARM Cortex-M0+) support.
```

- その他のフィールド（`name`、`url`、`architectures=*`、`includes`）は現状のままでよい。

## Task 8: README.md（英語で全面改訂）

- 冒頭: L6470 用であること、SparkFun AutoDriver からのフォークであること、Ponoor STEP800 などで使われていることを説明する。
- **Daisy chain** の節:
  - `position` 引数の意味と、全インスタンスで CS を共有すること。
  - 通常の API では1コマンドごとにチェーン全体を転送するため、転送量が N×N で増えること。
  - パック送信の使い方（短いコード例を含める）と制約（単一チェーン、CS / SPI ポートの共有）。
- **Differences from the original library** の節を、実際の差分に合わせて全面的に書き直す:
  - Added `getSpeed()`.
  - Added raw register accessors (`set/get*Raw()`, `runRaw()`, `goUntilRaw()`).
  - Added `getElPos()` / `setElPos()`.
  - Added packed daisy-chain transfers (`prepare*()` / `performPrepared()`).
  - Fixed unit conversion factors for ACC, DEC, INT_SPD, and the RUN speed according to the datasheet.
  - Fixed the bit shift of the `STATUS_MOT_STATUS_*` constants.
  - `goUntil()` / `releaseSw()` accept any non-zero `action` as COPY.
  - Interrupts are disabled during SPI transactions on SAMD to prevent corrupted return values.
  - Renamed command and status constants (`CMD_*`, `REG_STATUS`) to avoid conflicts with other libraries.
  - Unique include guards.
- **Examples** の節: 各サンプルの1行説明。
- **License** の節: SparkFun の beerware 表記をそのまま残す。現在の "Your friends at SparkFun." の後に空行を入れる程度の整形はしてよい。
- 英語の誤り（"Changed some constants name" → "Renamed some constants"、"Added functions related with" → "Added functions related to" など）を修正する。

## Task 9: CHANGELOG.md（新規作成）

- v1.2.0 の Fixed / Added / Changed を記載する。
- 過去の経緯も簡潔に記載する: v1.0.0（2021、フォーク）、v1.1.0（2024-09、換算係数の修正）。
- **Breaking-ish** の注記を入れる: `getAcc()` / `getDec()` の戻り値が約4.9%小さくなる（正しい値になる）こと。

---

## 検証

- `arduino-cli` が使える場合は、全サンプルを以下のボードでコンパイルする:
  - `arduino:samd:arduino_zero_native`（STEP シリーズの実機相当）
  - `arduino:avr:uno`（`__disable_irq` 周りの `#if` 分岐の確認用）
- 換算関数の往復誤差を検証する。ホスト上で動く簡単なテスト（例: `test/conversion_test.cpp` を g++ でビルド）を用意し、以下を確認する。テストファイルはリポジトリに含めてよいが、Arduino のビルド対象（`src/`、`examples/`）には入れない。
  - `accCalc(accParse(n)) == n`（n = 1..0xFFE）
  - `accParse(68) ≈ 989.5`
  - `accCalc(1e6) == 0xFFE`、`accCalc(0) == 1`
- パック送信のフレーム組み立ては、SPI をモック化したホスト上のテストで確認できれば理想。難しい場合は、`performPrepared()` のパケット組み立て部分を純粋関数に切り出して、そこだけテストする。

## 作業完了時に報告してほしいこと

- 変更ファイルの一覧と、各タスクの実施状況。
- 実装を保留した点や、DS と照合して判断に迷った点。
- タグ `v1.2.0` の作成とリリースは人間側で行うので、作業不要。

## 範囲外（別タスク）

以下は今回のタスクに含めない。

- step-series-universal-firmware 側の対応（`lib_deps` のタグ更新、`updateServo()` / `checkStatus()` のパック送信化）。
- Ponoor_PowerSTEP01_Library の修正。同様の修正に加え、`SR_980V_us` の IGATE、`getSlewRate()`、FS_SPD の 11bit（BOOST_MODE）の問題があり、別途指示する。
