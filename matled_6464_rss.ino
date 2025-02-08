/*
 *  Copyright(C) 2022 by Yukiya Ishioka
 */

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>     /* RTOS task related API prototypes. */
#include <freertos/timers.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>


/* 静的IPアドレス使用の設定  0:DHCP  1:静的IP */
#define  DEF_STATIC_IP     0

/* 受信データのBODYのモニタへの表示設定  0:表示せず  1:表示 */
#define  DEF_PRINT_BODY    0

/******************************************************/
/* Wi-Fiアクセスポイントの SSID とパスフレーズ        */
/* 利用する環境のSSIDとパスフレーズを設定してください */
/******************************************************/
const char* ssid     = "xxxxxxxxxx";
const char* password = "xxxxxxxxxx";

/******************************************************/

#if DEF_STATIC_IP
/******************************************************/
/* 静的IPアドレスの情報                               */
/* 利用する環境に合わせて設定してください             */
/******************************************************/
IPAddress local_IP(192, 168,   0, 120);
IPAddress gateway( 192, 168,   0,   1);
IPAddress subnet(  255, 255, 255,   0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);
/******************************************************/
#endif

/* 表示メッセージの情報元のURLリスト */
const char *url[] = {
  //"https://rss-weather.yahoo.co.jp/rss/days/13.xml",
  //"https://rss-weather.yahoo.co.jp/rss/days/4610.xml",
  /* https://news.yahoo.co.jp/rss */
  "https://news.yahoo.co.jp/rss/topics/top-picks.xml",
  "https://news.yahoo.co.jp/rss/topics/domestic.xml",
  "https://news.yahoo.co.jp/rss/topics/world.xml",
  "https://news.yahoo.co.jp/rss/topics/business.xml",
  "https://news.yahoo.co.jp/rss/topics/entertainment.xml",
  "https://news.yahoo.co.jp/rss/topics/sports.xml",
  "https://news.yahoo.co.jp/rss/topics/it.xml",
  "https://news.yahoo.co.jp/rss/topics/science.xml",
  "https://news.yahoo.co.jp/rss/topics/local.xml",
  NULL  /* 終了マーク */
};

/* マトリクスLEDのピン設定 */
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

/* 表示メッセージ抽出用のキーワード */
const char  *chks = "<title>";
const char  *chke = "</title>";
const char  *chkS = "<TITLE>";
const char  *chkE = "</TITLE>";

/* 起動時の表示メッセージ */
const char  *msg_init = "データアクセス準備中";

/* フォントの縦方向のドット数 */
#define DEF_FONT_SIZE        16

/* フォント配列の設定 */
extern  const unsigned char  fx_8x16rk_fnt[];
extern  const unsigned char  jiskan16_fnt[];
extern  const unsigned char  Utf8Sjis_tbl[];

#define DEF_FONT_A16_VAR     fx_8x16rk_fnt
#define DEF_FONT_K16_VAR     jiskan16_fnt

/* マトリクスLEDの横方向のドット数 */
#define DEF_DISP_WIDTH       128

/* マトリクスLED表示データの最大ドット数 */
#define DEF_OUTDATA_NUM      2900

/* 表示メッセージ文字列抽出時の最大半角文字数 */
#define DEF_INCHAR_NUM       1000

/* 変数、配列 */
unsigned char  dat_buffer[ DEF_FONT_SIZE ][ DEF_OUTDATA_NUM ];
unsigned char  msg_buffer[ DEF_INCHAR_NUM ];

TaskHandle_t  thand_mainTask;
TaskHandle_t  thand_slideTask;
TaskHandle_t  thand_wifiTask;
TimerHandle_t thand_timeMsg;
TimerHandle_t thand_timeBtn;
EventGroupHandle_t  ehandMsg;
EventGroupHandle_t  ehandSld;
SemaphoreHandle_t  shandMsg;

int  led_color;
int  led_pos;
int  led_msg_len ;


/*************************************************
 *  表示データ配列のクリア
 *************************************************/
void  clear_message( unsigned char *buff )
{
    int  i, j ;

    for( i=0 ; i<DEF_FONT_SIZE ; i++ ) {
        for( j=0 ; j<DEF_OUTDATA_NUM ; j++ ) {
            *buff++ = 0x00;
        }
    }
}


/*************************************************
 *  １文字分のフォントデータを表示データ配列の指定位置へセット
 *************************************************/
void  set_font( unsigned char *font, unsigned char *buff, int pos, int width )
{
    int  i, j, k;
    int  row;
    int  w = (width/8);   /* font width byte */
    unsigned char  pat;

    /* row */
    for( i=0 ; i<DEF_FONT_SIZE ; i++ ) {
        row = DEF_OUTDATA_NUM * i;
        /* col */
        for( j=0 ; j<w ; j++ ) {
            pat = 0x80;
            for( k=0 ; k<8 ; k++ ) {
                if( (font[ i * w + j ] & pat) != 0 ) {
                    /*         base up/low  offset */
                    buff[ row + pos + j*8 + k ] = 1;
                }
                pat >>= 1; /* bit shift */
            }
        }
    }
}


/*************************************************
 *  UTF-8コード から SJISコードへの変換
 *************************************************/
void UTF8_To_SJIS_cnv(unsigned char utf8_1, unsigned char utf8_2, unsigned char utf8_3, unsigned int* spiffs_addrs)
{
  unsigned int  UTF8uint = utf8_1*256*256 + utf8_2*256 + utf8_3;
   
  if(utf8_1>=0xC2 && utf8_1<=0xD1){
    *spiffs_addrs = ((utf8_1*256 + utf8_2)-0xC2A2)*2 + 0xB0;
  }else if(utf8_1==0xE2 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE28090)*2 + 0x1EEC;
  }else if(utf8_1==0xE3 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE38080)*2 + 0x9DCC;
  }else if(utf8_1==0xE4 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE4B880)*2 + 0x11CCC;
  }else if(utf8_1==0xE5 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE58085)*2 + 0x12BCC;
  }else if(utf8_1==0xE6 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE6808E)*2 + 0x1AAC2;
  }else if(utf8_1==0xE7 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE78081)*2 + 0x229A6;
  }else if(utf8_1==0xE8 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE88080)*2 + 0x2A8A4;
  }else if(utf8_1==0xE9 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE98080)*2 + 0x327A4;
  }else if(utf8_1>=0xEF && utf8_2>=0xBC){
    *spiffs_addrs = (UTF8uint-0xEFBC81)*2 + 0x3A6A4;
    if(utf8_1==0xEF && utf8_2==0xBD && utf8_3==0x9E){
      *spiffs_addrs = 0x3A8DE;
    }
  }
}


/*************************************************
 *  表示データ文字列（UTF-8コード）から SJISコードの取得
 *************************************************/
int  utf8_to_sjis( unsigned char *buff, unsigned char *code1, unsigned char *code2 )
{
  unsigned char  utf8_1, utf8_2, utf8_3;
  //unsigned char  sjis[2];
  unsigned int  sp_addres;
  int  pos = 0;
  int fnt_cnt = 0;
 
  if(buff[pos]>=0xC2 && buff[pos]<=0xD1){
    /* UTF8 2bytes */
    utf8_1 = buff[pos];
    utf8_2 = buff[pos+1];
    utf8_3 = 0x00;
    fnt_cnt = 2;
  }else if(buff[pos]>=0xE2 && buff[pos]<=0xEF){
    /* UTF8 3bytes */
    utf8_1 = buff[pos];
    utf8_2 = buff[pos+1];
    utf8_3 = buff[pos+2];
    fnt_cnt = 3;
  }else{
    utf8_1 = buff[pos];
    utf8_2 = 0x00;
    utf8_3 = 0x00;
    fnt_cnt = 1;
  }

  /* UTF8 to Sjis change table position */
  UTF8_To_SJIS_cnv( utf8_1, utf8_2, utf8_3, &sp_addres );
  *code1 = Utf8Sjis_tbl[ sp_addres   ];
  *code2 = Utf8Sjis_tbl[ sp_addres+1 ];

  return  fnt_cnt;
}


/*************************************************
 *  半角文字コードからフォントデータの先頭アドレス取得
 *************************************************/
unsigned char  *get_fontx2_a( unsigned char *font, unsigned int code )
{
    unsigned char  *address = NULL ;
    unsigned int  fontbyte ;

    fontbyte = (font[14] + 7) / 8 * font[15] ;
    address = &font[17] + fontbyte * code ;

    return  address ;
}


/*************************************************
 *  全角文字コードからフォントデータの先頭アドレス取得
 *************************************************/
unsigned char  *get_fontx2_k( unsigned char *font, unsigned int code )
{
    unsigned char  *address = NULL ;
    unsigned char  *tmp ;
    unsigned int  blknum, i, fontnum ;
    unsigned int  bstart, bend ;
    unsigned int  fontbyte ;

    fontbyte = (font[14] + 7) / 8 * font[15] ;
    fontnum = 0 ;

    blknum = (unsigned int)font[17] * 4 ;
    tmp = &font[18] ;
    for( i=0 ; i<blknum ; i+=4 ) {
        bstart = tmp[i]   + ((unsigned int)tmp[i+1] << 8) ;
        bend   = tmp[i+2] + ((unsigned int)tmp[i+3] << 8) ;
        if( code >= bstart && code <= bend ) {
            address = tmp + (fontnum + (code - bstart)) * fontbyte + blknum ;
            break ;
        }

        fontnum += (bend - bstart) + 1 ;
    }

    return  address ;
}

/*************************************************
 *  表示メッセージ文字列から表示データ配列の作成
 *************************************************/
int  make_message( unsigned char *strbuff, unsigned int size )
{
    int  pos, bitpos;
    int  num;
    unsigned char  *fontdata;
    unsigned int  code;
    unsigned char  code1, code2 ;

    clear_message( (unsigned char *)dat_buffer );

    pos = 0 ;
    /* 表示先頭のブランクのためのオフセット */
    bitpos = DEF_DISP_WIDTH ;
    while( pos < size ) {
        /* 表示メッセージ 1byteを取り出し */
        code = strbuff[ pos ] ;  /* get 1st byte */
        /* 半角か全角かの判定 */
        if( code < 0x80 ) {
            /* 半角の処理 */
            if( code == 0x0d || code == 0x0a ) {
                /* 改行コードはスペースへ変換 */
                code = ' ' ;
            }
            fontdata = get_fontx2_a( (unsigned char *)DEF_FONT_A16_VAR, code );
            set_font( fontdata, (unsigned char *)dat_buffer, bitpos, DEF_FONT_SIZE/2 );
            bitpos += DEF_FONT_SIZE/2 ;
            pos++ ;
        } else {
            /* 全角の処理 */
            num = utf8_to_sjis( &strbuff[ pos ], &code1, &code2 );
            code = (code1<<8) + code2 ;  /* get 2nd byte and marge */
            fontdata = get_fontx2_k( (unsigned char *)DEF_FONT_K16_VAR, code );
            set_font( fontdata, (unsigned char *)dat_buffer, bitpos, DEF_FONT_SIZE );
            bitpos += DEF_FONT_SIZE;
            pos += num;
        }

        if( bitpos >= (DEF_OUTDATA_NUM - DEF_FONT_SIZE - DEF_DISP_WIDTH) ) {
           /* 終端側のブランク部分に達したら作成処理終了 */
           break ;
        }
    }

    led_pos = 0 ;

    return  bitpos;
}


/*************************************************
 *  LED_SLIDタスクの入り口関数
 *************************************************/
void  led_slide( void *param )
{
  int  vol;

  Serial.println( "call slideTask()" );

  vol = 1000;

  while( 1 ) {
    vTaskDelay( vol>>6 );

    /* ボリューム値が2000未満ならスライド実行   */
    /* ボリューム値が2000以上ならスライドを停止 */
    if( vol < 2000 ) {
      led_pos++ ;
      if( led_pos >= led_msg_len ) {
        /* 表示データが終端に達したので表示メッセージ更新要求を送信*/
        xEventGroupSetBits( ehandMsg, 0x02 );
        led_pos = 0 ;

        /* 表示メッセージのスライド開始許可待ち */
        xEventGroupWaitBits( ehandSld, 0x01, pdTRUE, pdFALSE, portMAX_DELAY );
      }
    }
  }
}


/*************************************************
 *  Wi-Fi経由でURLリストのURLへアクセス
 *************************************************/
void  wifi_access( void )
{
  HTTPClient  http;
  int  httpCode;
  unsigned char  *src ;
  unsigned char  *dst ;
  unsigned char  *top;
  unsigned int  lens = strlen(chks);
  unsigned int  lene = strlen(chke);
  unsigned int  len;
  int  cnt;
  int  lim;
  int  i;
  int  mode;
  static int  url_num = 0;
  const char *url_pnt;

  /* URLリストからアクセスするURLを取得 */
  url_pnt = url[url_num];
  if( url_pnt == NULL ) {
    url_num = 0;
    url_pnt = url[url_num];
  }
  url_num++ ;

retry_loop:
  http.begin( url_pnt );  /* HTTPで URLへのアクセス */
  httpCode = http.GET();  /* HTTPのレスポンスコードを取得 */
  Serial.printf("URL: %s\n", url_pnt );
  Serial.printf("Response: %d", httpCode);
  Serial.println();

  if (httpCode == HTTP_CODE_OK) {
    /* HTTPサーバからのレスポンがOKの場合 */
    String body = http.getString();

#if DEF_PRINT_BODY
    /* 受信データのBODYの表示 */
    Serial.print("Response Body: ");
    Serial.println(body);
#endif

    src = (unsigned char *)body.c_str();
    dst = msg_buffer;
    mode = 0;
    cnt = 0;
    lim = 0;

    /* 受信データのBODYから表示メッセージの抽出 */
    while( cnt < DEF_INCHAR_NUM/2 && *src != 0x00 ) {
      if( ++lim > 10000 ) {
        /* 受信データが異常な場合、強制的に抽出終了 */
        Serial.printf("read data limit.");
        break;
      }

      /* <title> か <TITLE> の検索 */
      if( *src != '<' ) {
        src++;
        continue;
      }
      if( mode == 0 ) {
        if( strncmp( chks, (const char*)src, (size_t)lens ) == 0 
         || strncmp( chkS, (const char*)src, (size_t)lens ) == 0 ) {
          mode = 1;
          src += lens;
          top = src;
        } else {
          src++ ;
        }
        continue;
      }

      if( mode == 1 ) {  /* <title> か <TITLE> を発見後 */
        /* </title> か </TITLE> の検索 */
        if( strncmp( chke, (const char*)src, (size_t)lene ) == 0
         || strncmp( chkE, (const char*)src, (size_t)lene ) == 0) {

          /* <title> ～ </title>の間のデータを表示メッセージとして抽出 */
          len = (unsigned long)src - (unsigned long)top;
          for( i=0 ; i<len ; i++ ) {
            *dst++ = *top++;
          }
          *dst++ = ' '; /* set space */
          *dst++ = ' '; /* set space */
          src += lene;
          cnt += len + 2;
          mode = 0;
        }
      }
    }

    http.end();
    if( cnt == 0 ) {
      goto retry_loop;
    }

    /* 表示メッセージ文字列終端にヌル文字をセット */
    *dst = 0x00;

    if( i>0 ) {
      led_msg_len = make_message( msg_buffer, cnt );
      Serial.printf( "data byte  = %d\n", cnt );
      Serial.printf( "data width = %d\n", led_msg_len );
      Serial.printf( "%s\n", msg_buffer );
    }
  } else {
    /* HTTPサーバからのレスポンがOK以外の場合 */
    http.end();
  }
}


/*************************************************
 *  Wi-Fiの初期化
 *************************************************/
void  wifi_init( void )
{
  WiFi.disconnect(true);
  delay(1000);
  WiFiMulti wifiMulti;

#if DEF_STATIC_IP
  /* 静的IPアドレスの設定 */
  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
#endif

  /* Wi-Fiアクセスポイントへの接続 */
  wifiMulti.addAP(ssid, password);
  while(wifiMulti.run() != WL_CONNECTED) {
    vTaskDelay(100);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


/*************************************************
 *  LED_MAIタスクの入り口関数
 *************************************************/
void  led_main( void *param )
{
  EventBits_t  ret;

  Serial.println( "call mainTask()" );

  led_pos = 0;
  led_color = 0;

  /* 起動時の表示メッセージから表示データを作成 */
  led_msg_len = make_message( (unsigned char *)msg_init, strlen(msg_init) );
  Serial.printf( "data byte  = %d\n", strlen(msg_init) );
  Serial.printf( "data width = %d\n", led_msg_len );

  wifi_init();

  while( 1 ) {
    /* イベントグループによるメッセージ更新要求待ち */
    ret = xEventGroupWaitBits( ehandMsg, 0x03, pdTRUE, pdFALSE, portMAX_DELAY );

    /* 表示データ配列へのアクセス許可のためのセマフォの獲得待ち */
    xSemaphoreTake( shandMsg, portMAX_DELAY );

    /* TIM_MSGタイマのリセット */
    xTimerReset( thand_timeMsg, 0 );

    Serial.printf( "xEventGroupWaitBits() ret=0x%02x\n", ret );
    wifi_access();
    led_color++ ;
    xEventGroupSetBits( ehandSld, 0x01 );
    xSemaphoreGive( shandMsg );
  }
}


/*************************************************
 *  TIM_MSGタイマのハンドラ関数
 *************************************************/
void  timer_message( void *param )
{
  /* タイマによる表示メッセージ更新要求を送信 */
  xEventGroupSetBitsFromISR( ehandMsg, 0x01, NULL );
}


/*************************************************
 *  Arduinoの setup関数
 *************************************************/
void setup( void )
{
  int  col, row;
  int  ba, bb, bc, bd;

  /* シリアルの初期化 */
  Serial.begin(115200);
  Serial.println( "call setup()" );

  /* マトリクスLEDのピンの設定 */
  pinMode( R1PIN,  OUTPUT );
  pinMode( G1PIN,  OUTPUT );
  pinMode( B1PIN,  OUTPUT );
  pinMode( R2PIN,  OUTPUT );
  pinMode( G2PIN,  OUTPUT );
  pinMode( B2PIN,  OUTPUT );
  pinMode( APIN,   OUTPUT );
  pinMode( BPIN,   OUTPUT );
  pinMode( CPIN,   OUTPUT );
  pinMode( DPIN,   OUTPUT );
  pinMode( EPIN,   OUTPUT );
  pinMode( CLKPIN, OUTPUT );
  pinMode( LATPIN, OUTPUT );
  pinMode( OEPIN,  OUTPUT );
  digitalWrite( OEPIN, LOW );
  digitalWrite( CLKPIN, LOW );
  digitalWrite( LATPIN, LOW );
  digitalWrite( EPIN, 0 );

  /* マトリクスLEDの消灯 */
  for( row=0 ; row<16; row++ ) {
    for( col=0 ; col<DEF_DISP_WIDTH ; col++ ) {
      digitalWrite( R1PIN, LOW );
      digitalWrite( G1PIN, LOW );
      digitalWrite( B1PIN, LOW );
      digitalWrite( R2PIN, LOW );
      digitalWrite( G2PIN, LOW );
      digitalWrite( B2PIN, LOW );
      digitalWrite( CLKPIN, HIGH );
      digitalWrite( CLKPIN, LOW );
    }
    ba = bb = bc = bd = 0;
    if( row & 0x1 ) ba = 1;
    if( row & 0x2 ) bb = 1;
    if( row & 0x4 ) bc = 1;
    if( row & 0x8 ) bd = 1;
    digitalWrite( OEPIN, HIGH );
    digitalWrite( APIN, ba );
    digitalWrite( BPIN, bb );
    digitalWrite( CPIN, bc );
    digitalWrite( DPIN, bd );
    digitalWrite( LATPIN, HIGH );
    digitalWrite( LATPIN, LOW );
    digitalWrite( OEPIN, LOW );
  }

  /* イベントグループの生成 */
  ehandMsg = xEventGroupCreate();
  ehandSld = xEventGroupCreate();

  /* セマフォの生成 */
  shandMsg = xSemaphoreCreateBinary();
  xSemaphoreGive( shandMsg );

  /* タスクの生成 */
  xTaskCreatePinnedToCore( led_main,
                           "LED_MAIN",
                           0x2000,
                           NULL,
                           10,
                           &thand_mainTask,
                           0 );

  xTaskCreatePinnedToCore( led_slide,
                           "LED_SLID",
                           0x2000,
                           NULL,
                           configMAX_PRIORITIES - 1,
                           &thand_slideTask,
                           0 );

  /* タイマの生成 */
  thand_timeMsg = xTimerCreate( "TIM_MSG",
                                60*1000,
                                pdTRUE,  /* pdTURE:repeat, pdFALSE:one-shot */
                                NULL,
                                timer_message );
  /* タイマの開始 */
  xTimerStart( thand_timeMsg, 0 );
}


/*************************************************
 *  Arduinoの loop関数
 *************************************************/
void loop( void )
{
  int  ba, bb, bc, bd;
  int  r1, g1, b1;
  int  r2, g2, b2;
  int  row, col;
  int  i;
  int  l_led_pos;
  unsigned char *buff2 = (unsigned char *)dat_buffer ;
  static unsigned int  bright = 0;

  /* 表示データ配列へのアクセス許可のためのセマフォの獲得待ち */
  xSemaphoreTake( shandMsg, portMAX_DELAY );

  if( ++bright & 0x1 ) {  /* LEDの点灯を50%削減するための判定 */
    /* 表示カラーからRGBピンの 0/1を設定 */
    switch( led_color%7 ) {
    case 0:
      r1 = r2 = 1;
      g1 = g2 = 0;
      b1 = b2 = 0;
      break;
    case 1:
      r1 = r2 = 0;
      g1 = g2 = 1;
      b1 = b2 = 0;
      break;
    case 2:
      r1 = r2 = 0;
      g1 = g2 = 0;
      b1 = b2 = 1;
      break;
    case 3:
      r1 = r2 = 1;
      g1 = g2 = 1;
      b1 = b2 = 0;
      break;
    case 4:
      r1 = r2 = 1;
      g1 = g2 = 0;
      b1 = b2 = 1;
      break;
    case 5:
      r1 = r2 = 0;
      g1 = g2 = 1;
      b1 = b2 = 1;
      break;
    case 6:
      r1 = r2 = 1;
      g1 = g2 = 1;
      b1 = b2 = 1;
      break;
    }
  } else {
    r1 = r2 = 0;
    g1 = g2 = 0;
    b1 = b2 = 0;
  }

  l_led_pos = led_pos;
  for( i=0 ; i<16 ; i++ ) {
    row  = DEF_OUTDATA_NUM * i;

    for( col=l_led_pos ; col<l_led_pos+DEF_DISP_WIDTH ; col++ ) {

      /* RGBピンへの出力 */
#if 0
      /***** 上端 16ラインの表示処理、今回は未使用 *****/
      if( buff2[ row + col ] ) {
          digitalWrite( R1PIN, r1 );
          digitalWrite( G1PIN, g1 );
          digitalWrite( B1PIN, b1 );
      } else {
          digitalWrite( R1PIN, 0 );
          digitalWrite( G1PIN, 0 );
          digitalWrite( B1PIN, 0 );
      }
#endif

      /***** 下端 16ラインの表示処理 *****/
      if( buff2[ row + col ] ) {
          digitalWrite( R2PIN, r2 );
          digitalWrite( G2PIN, g2 );
          digitalWrite( B2PIN, b2 );
      } else {
          digitalWrite( R2PIN, 0 );
          digitalWrite( G2PIN, 0 );
          digitalWrite( B2PIN, 0 );
      }
      /* クロックの0/1を生成 */
      digitalWrite( CLKPIN, HIGH );
      digitalWrite( CLKPIN, LOW );
    }

    /* アクセスするライン用I/Oの変数をセット */
    ba = bb = bc = bd = 0;
    if( i & 0x1 ) ba = 1;
    if( i & 0x2 ) bb = 1;
    if( i & 0x4 ) bc = 1;
    if( i & 0x8 ) bd = 1;

    /* LATピン、OEピンの制御 */
    digitalWrite( OEPIN, HIGH );

    /* アクセスするライン用I/Oのピンの 0/1 をセット */
    digitalWrite( APIN, ba );
    digitalWrite( BPIN, bb );
    digitalWrite( CPIN, bc );
    digitalWrite( DPIN, bd );

    /* LATピン、OEピンの制御 */
    digitalWrite( LATPIN, HIGH );
    digitalWrite( LATPIN, LOW );
    digitalWrite( OEPIN, LOW );
    usleep(3);
  }

  for( col=0 ; col<DEF_DISP_WIDTH ; col++ ) {
#if 0
    digitalWrite( R1PIN, 0 );
    digitalWrite( G1PIN, 0 );
    digitalWrite( B1PIN, 0 );
#endif
    digitalWrite( R2PIN, 0 );
    digitalWrite( G2PIN, 0 );
    digitalWrite( B2PIN, 0 );

    digitalWrite( CLKPIN, HIGH );
    digitalWrite( CLKPIN, LOW );
  }

  digitalWrite( OEPIN, HIGH );

  digitalWrite( APIN, 0 );
  digitalWrite( BPIN, 0 );
  digitalWrite( CPIN, 0 );
  digitalWrite( DPIN, 0 );

  digitalWrite( LATPIN, HIGH );
  digitalWrite( LATPIN, LOW );
  digitalWrite( OEPIN, LOW );
  usleep(3);

  /* 表示データ配列へのアクセス許可セマフォの解放 */
  xSemaphoreGive( shandMsg );
}
