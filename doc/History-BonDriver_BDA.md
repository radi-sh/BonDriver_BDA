# Release Note

## 2014-03-14

- BonDriver_BDA改として公開

## 2014-03-19

- `BpnDriver`解放時に一般保護違反を起こす不具合を修正

## 2014-03-23

- TS取得用のフィルタグラフとして`Infinite Pin Tee`の使用をやめ、軽量なものに書き直した
- フィルタグラフの解放をキャプチャ→チューナの順序からチューナ→キャプチャに変更（メモリリークを起こすことがあるため）
- チューナのオープンに失敗した後オープン出来なくなる問題を修正
- その他、細かい修正

## 2015-08-07

- iniファイルでキャプチャデバイスの指定ができるようにした（`CaptureGuid`・`CaptureFriendlyName`）
- iniファイルでの`ChannelLock`関係の項目を追加（`ChannelLockWaitDelay`・`ChannelLockWaitRetry`・`ChannelLockTwice`・`ChannelLockTwiceDelay`）
- サンプルiniファイルでデフォルト値を明示指定するように変更
- サンプルiniファイルのCH設定を最新のものに更新
- 多くの環境で初期化に失敗する不具合を修正
- 2つ目以降のチューナが開けない不具合を修正
- チューナー・キャプチャの組合せを検索するロジックを変更
- その他、細かい修正
- 開発環境をVisual Studio Community 2013に変更

## 2015-09-21

- iniファイルに`Modulation`セクションを新設し、変調方式パラメータの設定ができるようにした
    - 上記変更に伴い、`put_Modulation()`のデフォルト値に`BDA_MOD_NBC_8PSK`/`BDA_MOD_NBC_QPSK`を使用するようにしました
    - 問題が発生する場合はiniファイル中の下記2項目を変更してみてください
    - `ModulationType0Modulation=20`
    - `ModulationType1Modulation=27`
- CH設定に左旋円偏波・右旋円偏波の指定ができるようにした
- `CoInitialize`/`CoUnInitialize`を含むすべてのCOM処理を独立した別スレッドで行うようにした
- 開発環境をVisual Studio Community 2015に変更
    - 通常版の実行には「Visual Studio 2015 の Visual C++ 再頒布可能パッケージ」のインストールが必要です

## 2015-10-07

（`BonDriver_BDA_with_PX-Q3W3S3`にバイナリ同梱）

- チャンネル空間が複数設定できるようになりました
    - `CH000`～のチャンネル定義は`TuningSpace00`～のセクション毎での定義になりました
    - `TuningSpaceName`も`TuningSpace00`～のセクション毎に設定してください
- `DebugLog`をファイルに記録するオプションを追加
- 地デジ/衛星チューナー用の設定を追加
- 同一のプロセスから複数`Create`されても大丈夫かもしれない感じにしてみた
- ↑の関係が原因だったと思われる、チューナデバイス検索時に使用中だったら`Release`しないルールを廃止
- チューナ固有関数を拡張し、チューナ初期化・デコード・選局時の追加処理等を行えるようにした
- `LoadTuningSpace`の処理を廃止し、`CreateTuningSpace`で作成した`TuningSpace`をシステムに登録しないように変更
- その他、細かい修正

## 2015-10-09

- ソースのコメントを整理
- 修正履歴・サンプルiniファイルを更新
- 一部コードの最適化

## 2015-10-10

不具合修正版

- `DefaultNetwork`に1(SPHD)/2(BS/CS110)を指定した時の衛星設定デフォルト値が間違っていた点を修正
- チューニングスペース中にCH定義が一つもされてなかった場合に落ちる不具合修正
- `IDVBTuningSpace`の代わりに`IDVBTuningSpace2`を使用するようにした
- CH定義が存在しなかった場合の動作を従来通り行うよう修正

## 2015-10-12

- `GetCurSpace()`が常に-1を返す不具合修正
- チューナデバイスとキャプチャデバイスのデバイスインスタンスパスをチェックし、間違った組み合わせでの`Connect`を行わないようにした
    - 問題が発生する場合はiniファイルの`[Tuner]`セクションに
    - `CheckDeviceInstancePath=0`
    - を追加することにより従来通りの動作（チェックを行わない）になります
- `Connect`してもよい`Capture`かどうか`BDASpceials`側に問い合わせる関数を追加
    - `BDASpecial-PlexPX`の2015-10-12版以後を使用する場合は`BonDriver_BDA`のこのバージョン以後と組み合わせて使用してください

## 2015-11-23

- iniファイルの`[TuningSpaceNN]`セクションに`ChannelLockTwiceTarget`を追加
    - `ChannelLockTwice`の対象となるCH番号を指定することが出来ます。
- `doc/Readme-BonDriver_BDA.txt`追加
- サンプルiniファイルの誤記修正
- `DLL_PROCESS_DETACH`時にデバッグログファイルのクローズを行うようにした

## 2015-12-23

- 開発環境をVisual Studio Community 2015 Update 1に変更
    - 通常版の実行には「Visual Studio 2015 Update 1 の Visual C++ 再頒布可能パッケージ」のインストールが必要です
    - 「Visual Studio 2015 の Visual C++ 再頒布可能パッケージ」を既にインストールしている場合でも別途インストールが必要ですので注意してください
- iniファイルの`SignalLevelCalcType`（`SignalLevel`算出方法）にビットレート値を指定できるようにした
- `CBonTuner::GetSignalState`周りのコード見直し
- iniファイルの`[Tuner]`セクションに下記3項目を追加し、受信異常時に自動での再チューニング/再オープンを行えるようにした
    - `WatchDogSignalLocked`
    - `WatchDogBitRate`
    - `ReOpenWhenGiveUpReLock`
- iniファイル`[Tuner]`セクションの`GUID`/`FriendlyName`/`CaptureGUID`/`CaptureFriendlyName`の仕様変更
    - 複数指定する場合に指定できる数値を0～9の10個から0～99の100個に変更
    - 下記の順序で検索するようにした
        1. `Guid0` / `FriendlyName0` / `CaptureGuid0` / `CaptureFriendlyName0` の組合せ
        1. `Guid1` / `FriendlyName1` / `CaptureGuid1` / `CaptureFriendlyName1` の組合せ
        1. `Guid2` / `FriendlyName2` / `CaptureGuid2` / `CaptureFriendlyName2` の組合せ
- 複数プロセスから同じ`BonDriver`を開いている場合でも`DebugLog`オプションがそれぞれで有効になるようにした

## 2016-01-10

- チューナのオープンに失敗した場合に不正終了してしまうことがある不具合を修正
- キャプチャデバイスを使用しないタイプのチューナに対応するオプションを追加
    - 詳細は`BonDriver_BDA.sample.ini`の`NotExistCaptureDevice`の項目を参照してください
- iniファイルで`ModulationTypeXBandWidth`を設定しても固定で6になっていた点を修正
- 衛星受信パラメータの仕様変更
    - `HighOscillator` / `LowOscillator` / `LNBSwitch` の組合せでの指定を可能にした
    - 機種固有API無しでの`DiSEqC`の指定を可能にした
- `SignalLevel`算出方法に、`ITuner::get_SignalStrength`を使用する設定を追加
    - 詳細は`BonDriver_BDA.sample.ini`の`SignalLevelCalcType`の項目を参照してください 
- `SignalLocked`の判定方法の設定を追加
    - 詳細は`BonDriver_BDA.sample.ini`の`SignalLockedJudgeType`の項目を参照してください
- iniファイルの`DVBSystemType`で指定できる種類に`DVB-C`/`ISDB-S`/`ISDB-T`/`ATSC`/`Digital-Cable`等を追加
    - 詳細は`BonDriver_BDA.sample.ini`の`DVBSystemType`の項目を参照してください
- iniファイルでの`Network Provider`指定を追加
    - 詳細は`BonDriver_BDA.sample.ini`の`NetworkProvider`の項目を参照してください
- `ATSC` / `Digital-Cable`用のCh定義設定を追加
    - 詳細は`BonDriver_BDA.sample.ini`のCH設定の項目を参照してください
- iniファイルの`BuffSize`に0を設定した場合に、上流より受取ったストリームデータサイズそのままで扱うようにした

## 2016-12-30

- 開発環境をVisual Studio Community 2015 Update 3に変更
- iniファイルで`StrengthBias`／`QualityBias`を指定可能にした
    - `SignalLevel`算出時に一定の値を加減算します
    - 詳細は`BonDriver_BDA.sample.ini`の`SignalLevelCalcType`の項目を参照してください
- iniファイルで`AlwaysAnswerLocked`を指定できるようにした
    - `TVTest`でチャンネルスキャンを行った時のお知らせダイアログを抑制できます
    - 詳細は`BonDriver_BDA.sample.ini`の`AlwaysAnswerLocked`の項目を参照してください
- iniファイルで`BackgroundChannelLock`を指定できるようにした
    - 詳細は`BonDriver_BDA.sample.ini`の`BackgroundChannelLock`の項目を参照してください
- iniファイルで`TryAnotherTuner`を指定できるようにした
    - 詳細は`BonDriver_BDA.sample.ini`の`TryAnotherTuner`の項目を参照してください

## 2017-01-22

- ini ファイルの `SignalLevelCalcType` に 3と13を追加
- `ITuner::get_SignalStrength`の`Strength` 値が正しく取得できない問題を修正
- `IBDA_SignalStatistics::get_SignalStrength` で取得する値を 16 bit mask するようにした

## 2017-02-06

- 一部の環境において、`BonDriver`の解放時にクラッシュしてしまう問題を修正

## 2018-07-16

- プラットフォームツールセットをVisual Studio 2017 - Windows XPに変更
- `CoInitialize` したスレッドでメッセージのディスパッチ処理を行うようにした
- iniファイルからのCH設定読込速度を改善
- TSバッファ排他処理の不具合を修正
- スカパー!SDの`ModulationType0Modulation`のデフォルト値を`20`(BDA_MOD_QPSK)に修正
- iniファイルの衛星設定を最大9つまで設定できるように変更
- iniファイルの変調方式設定を最大10個まで設定できるように変更
- `UHF`/`CATV`の周波数を正確な中心周波数に修正
- `UHF` 53ch～62chを削除
- `CATV` C24ch～C27chの周波数を変更
- `BonDriver_BDA-UHF-sample.ini`のチューニングスペース `"CATV"` をコメントアウト
- `BonDriver_BDA-UHF-sample.ini`のコメント内容変更
- `BonDriver_BDA-BSCS-sample.ini`のCH設定を最新のものに更新
- `BonDriver_BDA_SPHD-sample.ini`の`JCSAT3A-TP21`を有効にした
- `BonDriver_BDA_SPHD-sample.ini`の`JCSAT4A`の表記を`JCSAT4B`に修正

## 2018-08-18

- チューニング空間ごとに周波数オフセットを指定できるようにした
- `ThreadPriority`関係のiniファイルオプションを追加
- iniファイルの記述方法を数値形式だけではなく文字列での指定もできるように変更
    - 従来のiniファイル書式のままでも認識可能です
- `NotExistCaptureDevice=YES`なチューナーだけが存在する環境で`BonDriver`の初期化に失敗することがある不具合を修正
- iniファイルで`DefaultNetwork="Dual"`の設定を可能にした
- iniファイルの`UseSpecial`のデフォルトを`"AUTO"`から`""`(使用しない)に変更
- 一部の機種で`ITuner::get_SignalStrength()`の値が不正になる不具合を修正

## 2019-02-02

- Logに`"Format Error in readIniFile; Wrong Polarization."`が不正に記録されてしまう不具合を修正
- iniファイルで指定できる項目 `Satellite1SettingsAuto` ～ `Satellite9SettingsAuto` を追加
- iniファイルで指定できる項目 `ModulationType0SettingsAuto` ～ `ModulationType9SettingsAuto` を追加
- iniファイルで指定できる項目 `ChannelSettingsAuto` の内容の変更、関連オプションの追加
- iniファイルで指定できる項目 `ReserveUnusedCh` をチューニング空間ごとに設定できるようにした
- iniファイルで指定できる項目 `ToneSignalWait` に0が設定されている時はチューニング動作を2度行わないようにした
- iniファイルで指定できる項目 `DVBSystemType` に設定できる値に`"ISDB-C"`を追加
- iniファイルのCH定義 `SID`/`TSID`/`ONID` 等に `0` が設定されている時は `0` として認識するよう変更
- iniファイルで指定できる項目 `TSMFMode` を追加
- iniファイルで指定できる項目 `PeriodicTimer` を追加
    - **※ 20190202より前のバージョンの`BDASpecial`プラグインは動作しませんのでご注意ください。**
    - (`Bon_SPHD_BDA_PATCH_2`相当の`BDASpecial`プラグインを除く)

