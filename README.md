◆概要

　Shigezoneが販売する「フルカラーLEDパネル用ESP32制御ボードキット」を使ってRSSからトピックスをダウンロードしてフルカラーLEDパネルへ表示するプログラム。

　RSSの提供元から「提供するRSSは個人利用にのみ使用することができます。」という制限が設けられています。利用するさいにはこの制限を守て利用ください。

　使用するピンは以下を想定しています。

　　#define R1PIN     25

　　#define G1PIN     26

　　#define B1PIN     27

　　#define R2PIN     21

　　#define G2PIN     22

　　#define B2PIN     23

　　#define APIN      12

　　#define BPIN      16

　　#define CPIN      17

　　#define DPIN      18

　　#define EPIN       4

　　#define CLKPIN    15

　　#define LATPIN    32

　　#define OEPIN     33

◆必須設定

１）matled_6464_rss.ino ファイルの28行目付近の以下の行へアクセスするWi-Fiの SSIDとパスフレーズを設定する必要があります。

　　const char* ssid     = "xxxxxxxxxx";

　　const char* password = "xxxxxxxxxx";

２）初期状態でIPアドレスは DHCPからの取得モードになっています。固定IPアドレスとする場合、19行目付近のDEF_STATIC_IPの定義を「1」にします。

　IPアドレスは 33行目付近のコードと DEF_STATIC_IPで判定している部分を確認ください。

　　/* 静的IPアドレス使用の設定  0:DHCP  1:静的IP */

　　#define  DEF_STATIC_IP     0

◆免責

　著作者は，提供するプログラムを用いることで生じるいかなる損害に関して，一切の責任を負わないものとします．

　たとえ，著作者が損害の生じる可能性を知らされていた場合も同様に一切の責任を負わないものとします．
