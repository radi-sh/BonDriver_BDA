BonDriver_BDA改

【これは何？】
BDA互換チューナー用のGeneric BonDriverです。

【動作環境】
Windows XP/Vista/7/8/8.1/10 (x86/x64)
※ OSによりサポートされていない機能があります。

【対応チューナー】
チューナーのデバイスドライバとのインターフェースがBDA互換なチューナー一般で下記種類のものに対応しています。
・DVB-Sチューナー
  所謂スカパー!プレミアムが視聴できるチューナーです。
・ISDB-T/ISDB-Sチューナー
  地デジ/BS/CS110が視聴できるチューナーです。
・ISDB-Cチューナー
  トランスモジュレーション方式に対応したチューナーです。
・その他

  BDA互換なインターフェース以外の操作を必要とするチューナーの場合、対応するBDASpecialプラグインが必要となります。

【対応しているBDASpecialプラグイン】(リリース時現在)
・このBonDriver専用のBDASpecialプラグイン
  -PlexPX.dll（Plex PX-Q3PE/Plex PX-W3PE/Plex PX-W3PE V2/Plex PX-W3U3/Plex PX-W3U3 V2/Plex PX-W3U2/Plex PX-S3U2/Plex PX-S3U）
  -IT35.dll（Plex PX-MLT5PE/Plex PX-Q3PE4/Plex PX-W3PE4/Plex PX-Q3U4/Plex PX-W3U4/e-Better DTV02-5T-P/e-Better DTV02-1T1S-U）
  -DD.dll（Digital Devices GmbH社製の各種チューナー）
  -DVBWorld.dll（現在バイナリでの配布は行っていません。ソースコードからビルドしてください）
  -TBS.dll（現在バイナリでの配布は行っていません。ソースコードからビルドしてください）
  ※ 各BDASpecialプラグイン共、20190202より前のバージョンは動作しませんのでご注意ください。
・「Bon_SPHD_BDA_PATCH_2」相当のBDASpecialプラグイン
  -DVBWorld.dll
  -TBS.dll

【使い方】
1. 使用するファイル（32ビット用/64ビット用、Windows XP用/Windows Vista以降用、通常版/ランタイム内蔵版）の選択
・32ビット用/64ビット用の選択
  x86フォルダ以下にあるものが32ビット用、x64フォルダ以下にあるものが64ビット用です。
  使用しているOSがどちらであるかに関係なく、このBonDriverを読込むアプリがx86ビルドされたものであればx86、x64ビルドされたものであればx64を使用してください。
  この選択が間違っているとBonDriverの読み込みに失敗します。
・Windows XP用/とWindows Vista以降用の選択
  Release-XP/Release-XP_Static等、"XP"のついた名称のフォルダにあるものはWindows XP用、それ以外のものはWindows Vista/7/8/8.1/10用です。
・通常版/ランタイム内蔵版の選択
  Release-_Static/Release-XP_Static等、"Static"のついた名称のフォルダにあるものはランタイム内蔵版、それ以外のものは通常版です。
  通常版のほうがリソースの消費量も少なくお勧めですが、ランタイム内蔵版を使用すると、下記の「Visual C++ 再頒布可能パッケージ」のインストールを省略することが出来ます。
  よくわからない場合は、通常版を使用してください。
  通常版でうまくいかない方は、ランタイム内蔵版もお試しください。

2. Visual C++ 再頒布可能パッケージのインストール
「Visual Studio 2017 の Microsoft Visual C++ 再頒布可能パッケージ」の最新版を下記URLよりダウンロードし、インストールしてください。
https://visualstudio.microsoft.com/ja/downloads/?q=#other-ja
x86/x64の2種類がありますので注意してください。よくわからない場合は両方インストールしても大丈夫です。

当然ですが、アプリで別のバージョンのVisual C++ 再頒布可能パッケージを必要とする場合はそれのインストールも別途必要です。
「Bon_SPHD_BDA_PATCH_2」相当のBDASpecialプラグインを使用する場合も、対応したVisual C++ 再頒布可能パッケージのインストールが別途必要です。

3. BonDriverとiniファイル等の配置
・使用するアプリのBonDriver配置フォルダに、BonDriver_BDA.dllをリネームしたコピーを配置
  通常、ファイル名が"BonDriver_"から始まる必要がありますのでご注意ください。
・用意したdllと同じ名前のiniファイルを配置
  使用するチューナーの種類に応じて下記のサンプルiniファイルを基に作成してください。
    -BonDriver_BDA_SPHD-sample.ini
    -BonDriver_BDA-UHF-sample.ini
    -BonDriver_BDA-BSCS-sample.ini
  BDASpecialプラグインにサンプルiniファイルが付属する場合はそちらを基に作成してください。
  iniファイルに設定できる各項目の説明はBonDriver_BDA.sample.iniを参照してください。
・BDASpecialプラグインを使用する場合は該当dllファイルを配置
  BDASpecialプラグインのdllファイルは通常リネームせずに1つだけ配置すればOKです。

(配置例)
  BonDriver_SPHD_0.dll
  BonDriver_SPHD_0.ini
  BonDriver_SPHD_1.dll
  BonDriver_SPHD_1.ini

※「Bon_SPHD_BDA_PATCH_2」相当のBDASpecialプラグインを使用する場合で、同一のプロセスから複数のBonDriverの読込が行われる場合は下記のようにBDASpecialプラグインも複数用意し、iniファイルでUseSpecial="DVBWorld_0"のように個別にファイル名を指定してください。
  BonDriver_SPHD_0.dll
  BonDriver_SPHD_0.ini
  BonDriver_SPHD_1.dll
  BonDriver_SPHD_1.ini
  DVBWorld_0.dll
  DVBWorld_1.dll

【サポートとか】
・最新バージョンとソースファイルの配布場所
https://github.com/radi-sh/BonDriver_BDA/releases

・不具合報告等
専用のサポート場所はありません。
5chのDTV板で「スカパー!プレミアムをPCで視聴」スレを探して書込むとそのうち何か反応があるかもしれません。
作者は多忙を言い訳にあまりスレを見ていない傾向に有りますがご容赦ください。

【免責事項】
BonDriver_BDAや付属するもの、ドキュメントの記載事項などに起因して発生する損害事項等の責任はすべて使用者に依存し、作者・関係者は一切の責任を負いません。

【謝辞みたいなの】
・このBonDriver_BDAは「Bon_SPHD_BDA_PATCH_2」を基に改変したものです。
・TSパケット処理周りは「BonDriver_HDUS_14.4」を参考にさせていただきました。
・改変内容のいくつかは、旧2ch/5chのスカパー!プレミアムをPCで視聴スレの書込みを参考にさせていただきました。
・CoInitialize処理関連に関しては、旧2chのBonDriver共有ツール総合スレのご助言や関係公開物を参考にさせていただきました。
・上記すべての作者様、その他参考にさせていただいたDTV関係の作者様、ご助言いただいた方、不具合報告・使用レポートをいただいた方、全ての使用していただいた方々に深く感謝いたします。

