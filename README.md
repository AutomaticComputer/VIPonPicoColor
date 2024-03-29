# VIP/Pico with Color

[English](README_en.md)

Raspberry Pi Pico を用いた、COSMAC VIP のエミュレータ。
以前から公開していた [VIP/Pico](https://github.com/AutomaticComputer/VIPonPico)に
VP-590 Color Board 相当の機能を加え、
カラーの NTSC ビデオ信号を出力するようにしました(「大体 NTSC」と言うべきかも)。
追加部品は(ビデオ周りでは)抵抗のみです。

現代において NTSC で出力する意味はあまり無いかもしれませんが、昔のマイコンな感じが出ているかと思います。
だからといって、RF モジュレータを通すことまでは考えていませんが。
(世の中には、モノクロですが[電波までマイコンで生成している](https://hackaday.com/2015/02/26/attiny85-does-over-the-air-ntsc/)
人もいますね…)

![RoundUp](doc/roundup_color.jpg)

![RoundUp](doc/roundup_color_78750.jpg)


実行可能ファイルは 2 種類あります(「カラー信号の発生について」参照)。

- [128MHz版](build/pico_vip.uf2). 
少しにじみますが、割合自然な映像になります(上の写真)。
背景が少し不安定な気もします。
TV によってはカラーが出ない可能性もあるかもしれません。
- [78.75MHz版](build/pico_vip_78750.uf2). 
比較的鮮明な画像になりますが、一部縞模様やゴースト(?)が出ます(下の写真)。
実際の VIP + VP-590 に近いと思います。

導入法、操作法等についてはこちらを。

[VIP/Pico](https://github.com/AutomaticComputer/VIPonPico)

[COSMAC VIP 上のソフトウェア](doc/software.md)


## 言い訳

カラー版も、いろいろ怪しいところがあります。
特に、ハードウェア部分に関しては、接続先を壊さないとは限りません。
(詳しくはVIP/Pico の README.md をご覧ください。)


## ハードウェア

![回路図](doc/vip_pico_color_schematic.png)

VIP/Pico に比べると、抵抗が 5 本増えています。


## カラー信号の発生について

Raspberry Pi Pico の PIO(programmable I/O)を 1 つ(state machine を 4 つ)使い、
またCPUのエミュレーションに主に core 0, ビデオ出力に主に core 1 を使っています。

ビデオ信号のうち V-SYNC, H-SYNC については、
[Sagar さんの記事](https://sagargv.blogspot.com/2014/07/ntsc-demystified-color-demo-with.html)
にも書いてある通り、
多少規格から外れていても(安定していれば)あまり問題無く表示されるようですが、
色副搬送波の方はかなり正確に 315/88=3.579545454... MHz
でなければならないようです。
規格では±10 Hz となっていますが、さすがに実際のテレビではそこまで厳密ではないようです。

- システムクロック128 MHz版では、
クロックを 17.8793651 分の一に分割して PIO を約 7.16 MHz で動かし、約 3.58 MHz の方形波を作っています。
実際には clkdiv の小数部分は 1 バイトなので、clkdiv=17.87890625 になっており、
副搬送波は 3.579637 MHz ほどで、100 Hz ほどずれています…が、何とかなっています。
テレビによっては、画像が乱れたりカラーにならなかったりするかもしれません。
クロック分割を半分にして、PIO の 4 クロックを 3.58 MHz とすることも試みましたが、
これだとうまく表示されませんでした。
誤差が大きすぎるか、小数部分による揺らぎが大きすぎるためだと思われます。
色ずれがそこそこありますが、方形波のせいなのか、
あるいはもう少し工夫の余地があるのか…
また、色が少し波打つ感じがあるのは、
色副搬送波の周波数が正確でないためだと推測しています。

- 78.75 MHz 版では、22 分周して正確な 3.579545... MHz を作っていますが、縞模様が出てしまいます。
([このあたりにある理由](https://sagargv.blogspot.com/2011/04/ntsc-demystified-nuances-and-numbers.html)
によるものと思われます。)
1802 のクロックはその半分で、実際の VIP + VP-590 と大体同じ構成になっていると思います。
アナログ部分を真似れば改善するのかもしれません。


PIO の 3 つの state machine を、少し時間を空けて開始する(clkdiv カウンターをリスタートする)ことにより
3 つの相の方形波を作っています。
もう一つの state machine を使って、これらのカラー信号と
もう 2 つの DC 信号(輝度)を on/off し、これらの出力を抵抗で組み合わせたものが
最終的な信号となっています。


## 謝辞

カラーの NTSC ビデオ信号については、以下のようなページを参考にしました。

ChaN さんの[RS-170A NTSCビデオ信号タイミング規格の概要](http://elm-chan.org/docs/rs170a/spec_j.html)

nekosan さんの [ＣＰＬＤでコンポジットビデオ　”ネコ８”](http://picavr.uunyan.com/vhdl_composite.html)

ケンケンさんの [PICマイコンによるカラーコンポジットビデオ出力実験](http://www.ze.em-net.ne.jp/~kenken/composite/index.html)

Sagar さんの [All Digital NTSC Color](https://www.sagargv.com/proj/ntsc/), 
[https://sagargv.blogspot.com/2014/07/ntsc-demystified-color-demo-with.html](https://sagargv.blogspot.com/2014/07/ntsc-demystified-color-demo-with.html)
