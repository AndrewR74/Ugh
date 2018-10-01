#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <MD_MAX72xx.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES  4
#define USE_PAROLA_HW   0
#define  DELAYTIME  100  // in milliseconds

#define CLK_PIN   4  // or SCK
#define DATA_PIN  16  // or MOSI
#define CS_PIN    14  // or SS

char *textPointer = '\0';
char currentText[100];
int currentTextI = 0;
unsigned long previousTextMillis = 0, previousConnectMillis = 0;
uint8_t currentCharWidth;
uint8_t cBuf[8];
int _overScrollI = 0;
bool _doneScrolling = true;

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

const byte DNS_PORT = 53;
IPAddress apIP(10, 168, 1, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);
  
unsigned long previousMillis = 0;
int mode = 0; // 0 = Init Mode, 1 = Ticker, 2 = Captive Portal Mode, 3 = Connecting to wifi

// const char* host = "api.coinmarketcap.com";
const int httpsPort = 443;

// const char StockApiHost[200] = "api.iextrading.com";

char SSID[32] = "", Password[32] = "", Currency[5] = "", Api[200] = "", ApiHost[200] = "", Stock_Api[200] = "", Stock_ApiHost[200] = "";
char Tickers[5][10], TickerIds[5][5];
bool IsStock[5];
unsigned int precision = 3, currentTicker = 0;

// https://jsfiddle.net/rh1uo9wq/37/

String responseHTML = ""
                      "<!DOCTYPE html><html><head><meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\"><title>Crypto Ticker</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><style>"
                      "body{margin:5%;color:#2C4770;font-size:19px}hr{display:block;height:1px;border:0;border-top:1px solid #2C4770;margin:2em 0;padding:0}ul{list-style:none;margin-bottom:40px;padding-left:10px}.crypt{margin-top:50px}label{display:block;font-weight:700}input,select{display:block;width:100%;padding:6px 10px;margin:5px 0;box-sizing:border-box;font-size:19px}button{background-color:#1d8d3f;font-weight:700;color:#fff;display:block;width:100%;margin-top:20px;border-radius:0;padding:10px;border:none;cursor:pointer}button:hover{opacity:.8}#retry{display:none}#retry{margin-bottom:20px}.isStck{width:100%}.isStck>label:first-child{float:left;width:auto}.isStck>input,.isStck>label{display:inline;width:auto;float:right}.isStck>label{margin-right:10px}"
                      "</style></head><body>"
                      "<label>Available Networks</label>"
                      "<ul>";

String responseHTML2 =  "</ul><label>Wi-Fi Network (SSID)</label><input type='text' id='ssid' placeholder='Select an available network one from above' onkeyup='ssid=event.target.value; setSave();'><br/ ><label>Wi-Fi Password</label><input type='text' onkeyup='pass=event.target.value; setSave();'><br/ ><hr/><div class='crypt'><div class=\"isStck\"><label>Symbol 1</label><input type=\"checkbox\" id=\"s1\" onclick='s1=this.checked; setSave();'/><label for=\"s1\"> Is Stock Symbol</label></div><input type='text' id='c1' placeholder='BTC' onkeyup='c1=event.target.value; setSave();'><br/ ><div class=\"isStck\"><label>Symbol 2</label><input type=\"checkbox\" id=\"s2\" onclick='s2=this.checked; setSave();'/><label for=\"s2\"> Is Stock Symbol</label></div><input type='text' id='c2' placeholder='AAPL' onkeyup='c2=event.target.value; setSave();'><br/ ><div class=\"isStck\"><label>Symbol 3</label><input type=\"checkbox\" id=\"s3\" onclick='s3=this.checked; setSave();'/><label for=\"s3\"> Is Stock Symbol</label></div><input type='text' id='c3' onkeyup='c3=event.target.value; setSave();'><br/ ><div class=\"isStck\"><label>Symbol 4</label><input type=\"checkbox\" id=\"s4\" onclick='s4=this.checked; setSave();'/><label for=\"s4\"> Is Stock Symbol</label></div><input type='text' id='c4' onkeyup='c4=event.target.value; setSave();'><br/ ><div class=\"isStck\"><label>Symbol 5</label><input type=\"checkbox\" id=\"s5\" onclick='s5=this.checked; setSave();'/><label for=\"s5\"> Is Stock Symbol</label></div><input type='text' id='c5' onkeyup='c5=event.target.value; setSave();'></div><div class='crypt'><label>Crypto Currency Conversion</label><select id='c' onclick='curr=event.target.value; setSave();' onchange='curr = event.target.value; setSave();'></select></div><button onclick='document.location.href=save;' id='save'>Save</button><hr><h4>Advanced Settings</h4><label>Change Crypto API Host</label><input type='text' id='apihost' onkeyup='cryptoHost=event.target.value; setSave();'><br/><label>Change Crypto API URL</label><input type='text' id='api' onkeyup='cryptoApi=event.target.value; setSave();'><hr/><label>Change Stock API Host</label><input type='text' id='STCKapihost' onkeyup='stockHost=event.target.value; setSave();'><br/><label>Change Stock API URL</label><input type='text' id='STCKapi' onkeyup='stockApi=event.target.value; setSave();'><div style='text-align: right'><a href='http://845.ddns.net/cryptohelp' target='blank'>Help</a></div><button onclick='reset()' style='background-color:#ec6060' id='reset'>Reset</button><div style='display: none' id='data'>";

String responseHTML3 =  ""
                        "</div>"
                        "<script type=\"text/javascript\" src='/scripts.js'></script>"
                        "</body></html>";

String JS = "var save='',ssid='',pass='',c1='',c2='',c3='',c4='',c5='',s1=!1,s2=!1,s3=!1,s4=!1,s5=!1,curr='',c=JSON.parse(\"['USD', 'AUD', 'BRL', 'CAD', 'CHF', 'CLP', 'CNY', 'CZK', 'DKK', 'EUR', 'GBP', 'HKD', 'HUF', 'IDR', 'ILS', 'INR', 'JPY', 'KRW', 'MXN', 'MYR', 'NOK', 'NZD', 'PHP', 'PKR', 'PLN', 'RUB', 'SEK', 'SGD', 'THB', 'TRY', 'TWD', 'ZAR', 'BTC', 'ETH', 'XRP', 'LTC', 'BCH']\".replace(/'/g,'\\\"'));var crpytos=JSON.parse(\"{\\\"BTC\\\":1,\\\"LTC\\\":2,\\\"XRP\\\":52,\\\"NXT\\\":66,\\\"DOGE\\\":74,\\\"DGB\\\":109,\\\"RDD\\\":118,\\\"DASH\\\":131,\\\"MONA\\\":213,\\\"MAID\\\":291,\\\"XMR\\\":328,\\\"BCN\\\":372,\\\"BTS\\\":463,\\\"XLM\\\":512,\\\"SYS\\\":541,\\\"EMC\\\":558,\\\"XVG\\\":693,\\\"USDT\\\":825,\\\"XEM\\\":873,\\\"ETH\\\":1027,\\\"SC\\\":1042,\\\"REP\\\":1104,\\\"DCR\\\":1168,\\\"PIVX\\\":1169,\\\"LSK\\\":1214,\\\"DGD\\\":1229,\\\"STEEM\\\":1230,\\\"WAVES\\\":1274,\\\"ARDR\\\":1320,\\\"ETC\\\":1321,\\\"STRAT\\\":1343,\\\"NEO\\\":1376,\\\"XZC\\\":1414,\\\"ZEC\\\":1437,\\\"GNT\\\":1455,\\\"MKR\\\":1518,\\\"KMD\\\":1521,\\\"NANO\\\":1567,\\\"ARK\\\":1586,\\\"QTUM\\\":1684,\\\"BAT\\\":1697,\\\"ZEN\\\":1698,\\\"AE\\\":1700,\\\"ETP\\\":1703,\\\"MIOTA\\\":1720,\\\"BNT\\\":1727,\\\"GXS\\\":1750,\\\"FUN\\\":1757,\\\"PAY\\\":1758,\\\"SNT\\\":1759,\\\"EOS\\\":1765,\\\"MCO\\\":1776,\\\"PPT\\\":1789,\\\"OMG\\\":1808,\\\"BCH\\\":1831,\\\"BNB\\\":1839,\\\"BTM\\\":1866,\\\"DCN\\\":1876,\\\"ZRX\\\":1896,\\\"HSR\\\":1903,\\\"NAS\\\":1908,\\\"WTC\\\":1925,\\\"LRC\\\":1934,\\\"TRX\\\":1958,\\\"MANA\\\":1966,\\\"LINK\\\":1975,\\\"KNC\\\":1982,\\\"KIN\\\":1993,\\\"ADA\\\":2010,\\\"XTZ\\\":2011,\\\"RHOC\\\":2021,\\\"CNX\\\":2027,\\\"AION\\\":2062,\\\"BTG\\\":2083,\\\"KCS\\\":2087,\\\"NULS\\\":2092,\\\"ICX\\\":2099,\\\"POWR\\\":2132,\\\"ETN\\\":2137,\\\"QASH\\\":2213,\\\"BCD\\\":2222,\\\"CMT\\\":2246,\\\"ELF\\\":2299,\\\"WAX\\\":2300,\\\"XIN\\\":2349,\\\"MOAC\\\":2403,\\\"IOST\\\":2405,\\\"ZIL\\\":2469,\\\"HT\\\":2502,\\\"TUSD\\\":2563,\\\"ONT\\\":2566,\\\"BTCP\\\":2575,\\\"CENNZ\\\":2585,\\\"DROP\\\":2591,\\\"NPXS\\\":2603,\\\"WAN\\\":2606,\\\"MITH\\\":2608,\\\"HOT\\\":2682,\\\"AOA\\\":2874,\\\"VET\\\":3077}\");var S_cryptoApi='/v2/ticker/{TickerId}/?convert={Currency}',S_cryptoHost='https://api.coinmarketcap.com',STCK_HOST='https://api.iextrading.com',STCK_API='/1.0/stock/{Symbol}/price';var cryptoApi=S_cryptoApi+\"\",cryptoHost=S_cryptoHost+\"\",stockApi=STCK_API+\"\",stockHost=STCK_HOST+\"\";var sbtn=document.getElementById('save'),rbtn=document.getElementById('reset'),cddl=document.getElementById('c'),api=document.getElementById('api'),apiHost=document.getElementById('apihost'),txt_stock=document.getElementById('STCKapi'),txt_stockHost=document.getElementById('STCKapihost');for(var i in c){var opt=c[i];var el=document.createElement('option');el.text=opt;el.value=opt;cddl.add(el)}"
            "api.value=cryptoApi;apiHost.value=cryptoHost;txt_stock.value=stockApi;txt_stockHost.value=stockHost;cddl.value='USD';var getCrypto=function(symb){if(!isNaN(symb))"
            "return symb;if(crpytos&&crpytos[symb.toUpperCase()]){return crpytos[symb.toUpperCase()]}"
            "return null};var setSave=function(){var a1=getCrypto(c1),a2=getCrypto(c2),a3=getCrypto(c3),a4=getCrypto(c4),a5=getCrypto(c5);save='/Save?'+'SSID='+encodeURI(ssid)+'&'+'Pass='+encodeURI(pass)+'&'+'C1='+encodeURI(c1)+'&'+'C2='+encodeURI(c2)+'&'+'C3='+encodeURI(c3)+'&'+'C4='+encodeURI(c4)+'&'+'C5='+encodeURI(c5)+'&'+'I1='+encodeURI(a1)+'&'+'I2='+encodeURI(a2)+'&'+'I3='+encodeURI(a3)+'&'+'I4='+encodeURI(a4)+'&'+'I5='+encodeURI(a5)+'&'+'S1='+encodeURI(s1)+'&'+'S2='+encodeURI(s2)+'&'+'S3='+encodeURI(s3)+'&'+'S4='+encodeURI(s4)+'&'+'S5='+encodeURI(s5)+'&'+'Api='+encodeURI(cryptoApi)+'&'+'Host='+encodeURI(cryptoHost)+'&'+'StockApi='+encodeURI(stockApi)+'&'+'StockHost='+encodeURI(stockHost)+'&'+'Curr='+encodeURI(curr)};var setSsid=function(s){document.getElementById('ssid').value=s;ssid=s;setSave();return!1};var reset=function(){document.getElementById('ssid').value='';cddl.value='USD';api.value=S_cryptoApi;apiHost.value=S_cryptoHost;txt_stock.value=STCK_API;txt_stockHost.value=STCK_HOST;document.getElementById('c1').value='BTC';document.getElementById('c2').value='';document.getElementById('c3').value='';document.getElementById('c4').value='';document.getElementById('c5').value='';document.getElementById('s1').checked=!1;document.getElementById('s2').checked=!1;document.getElementById('s3').checked=!1;document.getElementById('s4').checked=!1;document.getElementById('s5').checked=!1;ssid='',c1='BTC',c2='',c3='',c4='',c5='',s1=!1,s2=!1,s3=!1,s4=!1,s5=!1,curr='USD',cryptoApi=S_cryptoApi,crpytoHost=S_cryptoHost,stockApi=STCK_API,stockHost=STCK_HOST;setSave()};try{var d=JSON.parse(document.getElementById('data').innerHTML);document.getElementById('ssid').value=d.ssid||'';cddl.value=d.c||'';document.getElementById('c1').value=d.c1||'';document.getElementById('c2').value=d.c2||'';document.getElementById('c3').value=d.c3||'';document.getElementById('c4').value=d.c4||'';document.getElementById('c5').value=d.c5||'';document.getElementById('s1').checked=d.s1||!1;document.getElementById('s2').checked=d.s2||!1;document.getElementById('s3').checked=d.s3||!1;document.getElementById('s4').checked=d.s4||!1;document.getElementById('s5').checked=d.s5||!1;ssid=d.ssid,c1=d.c1,c2=d.c2,c3=d.c3,c4=d.c4,c5=d.c5,curr=d.c,cryptoApi=(d.api||S_cryptoApi),s1=(d.s1||!1),s2=(d.s2||!1),s3=(d.s3||!1),s4=(d.s4||!1),s5=(d.s5||!1),crpytoHost=(d.host||S_cryptoHost),stockApi=(d.StockApi||STCK_API),stockHost=(d.StockHost||STCK_HOST);api.value=cryptoApi;apiHost.value=crpytoHost;txt_stock.value=stockApi;txt_stockHost.value=stockHost}catch(e){}";


String networksHTML = "";

void loadCredentials() {

  Serial.println("");
  Serial.println("SSID: " + String(SSID) + " - " + sizeof(SSID));
  Serial.println("Password: " + String(Password) + " - " + sizeof(Password));
  for (int i = 0; i < (sizeof(Tickers) / sizeof(Tickers[0])); i++) {
    Serial.println("C" + String(i + 1) + ": " + String(Tickers[i]) + " - " + sizeof(Tickers[i]));
  }
  for (int i = 0; i < (sizeof(TickerIds) / sizeof(TickerIds[0])); i++) {
    Serial.println("I" + String(i + 1) + ".ID: " + String(TickerIds[i]) + " - " + sizeof(TickerIds[i]));
  }
  for (int i = 0; i < (sizeof(IsStock) / sizeof(IsStock[0])); i++) {
    Serial.println("S" + String(i + 1) + ": " + String(IsStock[i]));
  }
  Serial.println("Currency: " + String(Currency) + " - " + sizeof(Currency));


  EEPROM.begin(2048);

  int addr = 0;

  EEPROM.get(0, SSID);
  addr = addr + sizeof(SSID);

  EEPROM.get(addr, Currency);
  addr = addr + sizeof(Currency);

  for (int i = 0; i < (sizeof(Tickers) / sizeof(Tickers[0])); i++) {
    EEPROM.get(addr, Tickers[i]);
    addr = addr + sizeof(Tickers[i]);
  }

  for (int i = 0; i < (sizeof(TickerIds) / sizeof(TickerIds[0])); i++) {
    EEPROM.get(addr, TickerIds[i]);
    addr = addr + sizeof(TickerIds[i]);
  }

  EEPROM.get(addr, ApiHost);
  addr = addr + sizeof(ApiHost);

  EEPROM.get(addr, Api);
  addr = addr + sizeof(Api);

  EEPROM.get(addr, Password);
  addr = addr + sizeof(Password);

  EEPROM.get(addr, Stock_ApiHost);
  addr = addr + sizeof(Stock_ApiHost);

  EEPROM.get(addr, Stock_Api);
  addr = addr + sizeof(Stock_Api);

  for (int i = 0; i < (sizeof(IsStock) / sizeof(IsStock[0])); i++) {
    byte tval = 0x00;
    EEPROM.get(addr, tval);
    addr = addr + 1;
    IsStock[i] = (tval == 0x01);
  }

  EEPROM.end();

  Serial.println("");
  Serial.println("SSID: " + String(SSID));
  Serial.println("Password: " + String(Password));
  for (int i = 0; i < (sizeof(Tickers) / sizeof(Tickers[0])); i++) {
    Serial.println("C" + String(i + 1) + ": " + String(Tickers[i]));
  }
  for (int i = 0; i < (sizeof(TickerIds) / sizeof(TickerIds[0])); i++) {
    Serial.println("I" + String(i + 1) + ": " + String(TickerIds[i]));
  }
  for (int i = 0; i < (sizeof(IsStock) / sizeof(IsStock[0])); i++) {
    Serial.println("S" + String(i + 1) + ": " + String(IsStock[i]));
  }
  Serial.println("Currency: " + String(Currency));
  Serial.println("ApiHost: " + String(ApiHost));
  Serial.println("Api: " + String(Api));
  Serial.println("Stock_ApiHost: " + String(Stock_ApiHost));
  Serial.println("Stock_Api: " + String(Stock_Api));
}

void handleSave() {
  scrollText("Save");
  Serial.println("handle Save");

  bool _ssidChanged = true;
  char _tempSsid[32] = "", _tempPassword[32] = "";

  webServer.arg("SSID").toCharArray(_tempSsid, 32);
  webServer.arg("Pass").toCharArray(Password, 32);
  webServer.arg("Curr").toCharArray(Currency, 5);

  for (int i = 0; i < (sizeof(Tickers) / sizeof(Tickers[0])); i++)
    webServer.arg("C" + String(i + 1)).toCharArray(Tickers[i], 10);
  for (int i = 0; i < (sizeof(TickerIds) / sizeof(TickerIds[0])); i++)
    webServer.arg("I" + String(i + 1)).toCharArray(TickerIds[i], 5);
  for (int i = 0; i < (sizeof(IsStock) / sizeof(IsStock[0])); i++) {
    char tVal[5];
    webServer.arg("S" + String(i + 1)).toCharArray(tVal, 5);
    IsStock[i] = String(tVal) == "true";
  }

  webServer.arg("Api").toCharArray(Api, 200);
  webServer.arg("Host").toCharArray(ApiHost, 200);

  webServer.arg("StockApi").toCharArray(Stock_Api, 200);
  webServer.arg("StockHost").toCharArray(Stock_ApiHost, 200);

  _ssidChanged = String(_tempSsid) != String(SSID);
  strcpy( SSID, _tempSsid ); // Put the SSID to global scope

  // Only update the password if the SSID/Password changed or the Password changed
  //if (_ssidChanged || (String(_tempPassword).length() > 0 && String(_tempPassword) != String(Password)))
  //  strcpy( Password, _tempPassword );

  Serial.println("SSID: " + webServer.arg("SSID"));
  Serial.println("Password: " + webServer.arg("Pass"));
  for (int i = 0; i < (sizeof(Tickers) / sizeof(Tickers[0])); i++)
    Serial.println("C" + String(i + 1) + ": " + webServer.arg("C" + String(i + 1)));

  for (int i = 0; i < (sizeof(TickerIds) / sizeof(TickerIds[0])); i++)
    Serial.println("I" + String(i + 1) + ": " + webServer.arg("I" + String(i + 1)));

  for (int i = 0; i < (sizeof(IsStock) / sizeof(IsStock[0])); i++) {
    Serial.println("S" + String(i + 1) + ": " + String(IsStock[i]));
  }

  Serial.println("Currency: " + webServer.arg("Curr"));

  Serial.println("SSID: " + String(SSID) + " - " + sizeof(SSID));
  Serial.println("Password: " + String(Password) + " - " + sizeof(Password));
  for (int i = 0; i < (sizeof(Tickers) / sizeof(Tickers[0])); i++) {
    Serial.println("C" + String(i + 1) + ": " + String(Tickers[i]) + " - " + sizeof(Tickers[i]));
  }
  for (int i = 0; i < (sizeof(TickerIds) / sizeof(TickerIds[0])); i++) {
    Serial.println("I" + String(i + 1) + ": " + String(TickerIds[i]) + " - " + sizeof(TickerIds[i]));
  }
  Serial.println("Currency: " + String(Currency) + " - " + sizeof(Currency));

  Serial.println("ApiHost: " + String(ApiHost) + " - " + sizeof(ApiHost));
  Serial.println("Api: " + String(Api) + " - " + sizeof(Api));
  Serial.println("Stock_ApiHost: " + String(Stock_ApiHost) + " - " + sizeof(Stock_ApiHost));
  Serial.println("Stock_Api: " + String(Stock_Api) + " - " + sizeof(Stock_Api));

  int addr = 0;

  EEPROM.begin(2048);

  EEPROM.put(0, SSID);
  addr = addr + sizeof(SSID);

  EEPROM.put(addr, Currency);
  addr = addr + sizeof(Currency);

  for (int i = 0; i < (sizeof(Tickers) / sizeof(Tickers[0])); i++) {
    EEPROM.put(addr, Tickers[i]);
    addr = addr + sizeof(Tickers[i]);
  }

  for (int i = 0; i < (sizeof(TickerIds) / sizeof(TickerIds[0])); i++) {
    EEPROM.put(addr, TickerIds[i]);
    addr = addr + sizeof(TickerIds[i]);
  }

  EEPROM.put(addr, ApiHost);
  addr = addr + sizeof(ApiHost);

  EEPROM.put(addr, Api);
  addr = addr + sizeof(Api);

  EEPROM.put(addr, Password);
  addr = addr + sizeof(Password);

  EEPROM.put(addr, Stock_ApiHost);
  addr = addr + sizeof(Stock_ApiHost);

  EEPROM.put(addr, Stock_Api);
  addr = addr + sizeof(Stock_Api);

  for (int i = 0; i < (sizeof(IsStock) / sizeof(IsStock[0])); i++) {
    EEPROM.put(addr, (IsStock[i] ? 0x01 : 0x00));
    addr = addr + 1;
  }

  EEPROM.commit();
  EEPROM.end();

  webServer.send(200, "text/html", "<h2 style='color: #1d8d3f'>Saved!</h2>");

  for (int i = 0; i < 1000; i++)
    delay(1);

  mode = 1; // Ticker Mode

  connectToWifi();
}

void scan() {
  Serial.println("scan start");
  networksHTML = "";

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found

      networksHTML = networksHTML + "<li><a onclick='return setSsid(\"" + WiFi.SSID(i) + "\");' href='#'>" + WiFi.SSID(i) + "</a></li>";

      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  //Serial.println("");

  //Serial.println(networksHTML);
}

void connectToWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  WiFi.begin(SSID, Password);
  //WiFi.begin("SmartWifi", "pastelmint859");

  /*int t = 0;

    while (WiFi.status() != WL_CONNECTED && (t++) < 20) {
    delay(1000);
    Serial.println("Connecting... " + String(t) + "/20");
    }*/
}


void scrollText(char *p)
{
  textPointer = p;
  currentTextI = 0;
  previousTextMillis = 0;
  currentCharWidth = 0;
  _overScrollI = 0;
  _doneScrolling = false;
  mx.clear();
}

void runScrollText() {

  unsigned long currentMillis = millis();

  if (*textPointer != '\0' || currentTextI > 0) {

    if (currentMillis - previousTextMillis >= DELAYTIME) {
      if (currentTextI == 0) {

        if (*textPointer == '#' || *textPointer == '^') {
          currentCharWidth = 7;

          if (*textPointer == '#') {
            uint8_t up_arrow[7] =
            {
              0b00001000,
              0b00011000,
              0b00111000,
              0b01111000,
              0b00111000,
              0b00011000,
              0b00001000
            };

            memcpy(cBuf, up_arrow, 7);
          } else {
            uint8_t down_arrow[7] =
            {
              0b00010000,
              0b00011000,
              0b00011100,
              0b00011110,
              0b00011100,
              0b00011000,
              0b00010000
            };

            memcpy(cBuf, down_arrow, 7);
          }

          *textPointer++;
        } else {
          currentCharWidth = mx.getChar(*textPointer++, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);
        }
      }

      mx.transform(MD_MAX72XX::TSL);

      if (currentTextI < currentCharWidth) {
        mx.setColumn(0, cBuf[currentTextI]);
      }

      currentTextI++;

      if (currentTextI > currentCharWidth) {
        if (*textPointer == '\0') {
          if (_overScrollI++ == (8 * MAX_DEVICES)) {
            currentTextI = 0;
            _doneScrolling = true;
          }
        } else {
          currentTextI = 0;
        }
      }

      previousTextMillis = currentMillis;
    }
  }
}

void captivePortalSetup() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("Crypto Ticker");

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);

  // replay to all requests with same HTML
  webServer.onNotFound([]() {
    scrollText("Setup Mode");

    Serial.println("Setup Mode");

    String json = "{"
                  "\"ssid\": \"" + String(SSID) + "\","
                  "\"c\": \"" + String(Currency) + "\",";

    for (int i = 0; i < (sizeof(Tickers) / sizeof(Tickers[0])); i++)
      json += "\"c" + String(i + 1) + "\": \"" + String(Tickers[i]) + "\",";

    for (int i = 0; i < (sizeof(TickerIds) / sizeof(TickerIds[0])); i++)
      json += "\"i" + String(i + 1) + "\": \"" + String(TickerIds[i]) + "\",";

    for (int i = 0; i < (sizeof(IsStock) / sizeof(IsStock[0])); i++)
      json += "\"s" + String(i + 1) + "\": " + (IsStock[i] ? "true" : "false") + ",";

    json += "\"StockHost\": \"" + String(Stock_ApiHost) + "\",";

    json += "\"StockApi\": \"" + String(Stock_Api) + "\",";

    json += "\"api\": \"" + String(Api) + "\",";

    json += "\"host\": \"" + String(ApiHost) + "\"";

    json += "}";

    webServer.send(200, "text/html", responseHTML + networksHTML + responseHTML2 + json + responseHTML3);

    mode = 2; // Captive Portal Mode
  });
  webServer.on("/Save", handleSave);

  webServer.on("/scripts.js", []() {
    webServer.send(200, "text/javascript", JS);
  });

  webServer.begin();
}

String GetStockTickerData(char Symbol[5]) {
  
  char payload[50];
  bzero(payload, 50);
  int i = 0;
  
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    Serial.print("connecting to ");
    String LC = String(Stock_ApiHost);
    LC.toLowerCase();
    String HS = String(Stock_ApiHost).substring(LC.indexOf("://") + 3, sizeof(Stock_ApiHost) - 1);

    Serial.println(HS + ":" + (LC.indexOf("https") == 0 ? "443" : "80"));


    if (!client.connect(HS, (LC.indexOf("https") == 0 ? 443 : 80))) {
      Serial.println("connection failed");
      return "\0";
    }

    Serial.println("requesting URL: ");
    Serial.println(Stock_Api);
    Serial.println(Symbol);
    
    String url = String(Stock_Api);

    url.replace("{Symbol}", String(Symbol));

    Serial.println("requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + HS + "\r\n" +
                 "User-Agent: BuildFailureDetectorESP8266\r\n" +
                 "Connection: close\r\n\r\n");

    Serial.println("request sent");

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
      if (line == "\r") {
        Serial.println("headers received");
        break;
      }
    }

    while (client.available()) {
      char c = client.read();

      if(i < 48) {
        payload[i] = c;
        i++;
      }
    }

    //payload[i] = '\0'; // Terminate

    Serial.println("\n\n");
    Serial.println(payload);

    client.stop();
  }
  
  return String(payload);
}

String GetTickerData(char tickerId[5]) {
  char payload[1500];
  bzero(payload, 1500);
  int i = 0;
  
  Serial.println("Getting Ticker: " + String(tickerId));
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    Serial.print("Connecting to: ");
    String LC = String(ApiHost);
    LC.toLowerCase();
    String HS = String(ApiHost).substring(LC.indexOf("://") + 3, sizeof(ApiHost) - 1);

    Serial.print(HS + ":" + (LC.indexOf("https") == 0 ? "443" : "80"));

    if (!client.connect(HS, (LC.indexOf("https") == 0 ? 443 : 80))) {
      Serial.println("connection failed");
      return "\0";
    }
    
    Serial.println("Nope");
    
    String url = String(Api); //"/v2/ticker/" + String(*tickerId) + "/?convert=" + String(Currency);

    url.replace("{TickerId}", String(tickerId));
    url.replace("{Currency}", (String(Currency) == "\0" ? "USD" : String(Currency))); //(Currency == "\0" ? "USD" : String(Currency)));

    Serial.println("Requesting URL: ");
    Serial.print(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + HS + "\r\n" +
                 "User-Agent: BuildFailureDetectorESP8266\r\n" +
                 "Connection: close\r\n\r\n");

    Serial.println("request sent");

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
      if (line == "\r") {
        Serial.println("headers received");
        break;
      }
    }
    
    while (client.available()) {
      char c = client.read();

      if(i < 1498) {
        payload[i] = c;
        i++;
      }
    }

    Serial.println("\n\n");
    Serial.println(payload);

    client.stop();
  }
  
  return String(payload);
}

String GetTickerText(String p) {

  String result = "\0";

  DynamicJsonBuffer  jsonBuffer(500);

  JsonObject& root = jsonBuffer.parseObject(p);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    scrollText("API Error");
  } else {
    result = root["data"]["symbol"].as<String>() + (root["data"]["quotes"][(root.containsKey(String(Currency)) ? String(Currency) : "USD")]["percent_change_1h"].as<float>() >= 0 ? " ^ " : " # ") + String(root["data"]["quotes"]["USD"]["price"].as<float>(), precision);
    Serial.println(result);
  }

  return result;
}

void setup() {
  mx.begin();
  
  Serial.begin(9600);

  Serial.println("Heap: " + String(ESP.getFreeHeap()));

  loadCredentials();

  scan();

  captivePortalSetup();

  scrollText("Setup");
}

void loop() {
  unsigned long currentMillis = millis();

  runScrollText();

  //Serial.println("Heap: " + String(ESP.getFreeHeap()));

  // Init/Captive Portal Modes
  if (mode == 0 || mode == 2) {

    dnsServer.processNextRequest();
    webServer.handleClient();

    if (String(SSID).length() > 0 && mode == 0) {
      if (previousMillis == 0) {
        previousMillis = currentMillis;
        //scrollText("Starting");
      }

      // 20 seconds after no settings request go into ticker mode
      if (currentMillis - previousMillis >= 2000) {

        scrollText("Connecting");
        
        mode = 3; // WiFi Connect mode

        connectToWifi(); // Connect to wifi network

        previousMillis = 0; // Reset this timer so if we don't connect to the wifi this iteration, we'll have a chance to change networks before the next iteration
      }
    } else if (_doneScrolling && mode == 0) { // Not initalized - 1st time run or empty SSID
      scrollText("WiFi: Crypto Ticker");
    }
  }
  // Ticker Mode
  else if (mode == 1) {

    if (WiFi.status() == WL_CONNECTED) {
      if (currentMillis - previousMillis >= 10000 && _doneScrolling) {
        previousMillis = currentMillis;

        if (currentTicker == (sizeof(TickerIds) / sizeof(TickerIds[0])))
          currentTicker = 0;

        Serial.println("Current Ticker: " + String(currentTicker));
        Serial.println("TickerIds Size: " + String((sizeof(TickerIds) / sizeof(TickerIds[0]))));
        Serial.println("Ticker ID: " + String(TickerIds[currentTicker]));
        Serial.println("Ticker: " + String(Tickers[currentTicker]));
        Serial.println("Heap: " + String(ESP.getFreeHeap()));

        if ( (!IsStock[currentTicker] && TickerIds[currentTicker] != "\0" && String(TickerIds[currentTicker]).length() > 0) ||  (IsStock[currentTicker] && Tickers[currentTicker] != "\0" && String(Tickers[currentTicker]).length() > 0)) {

          String tickerText = "";
          String resultText = "";

          if (!IsStock[currentTicker]) {

              tickerText = GetTickerData(TickerIds[currentTicker]);

              if (tickerText != "" && tickerText != "\0")
                resultText = GetTickerText(tickerText);
          } else {
            // TODO: Test if numeric in a function

            //tickerText = GetStockTickerData(Tickers[currentTicker]);
            
            // tickerText =  String(GetStockTickerData(Tickers[currentTicker]));

            // tickerText = "AAPL: 225.667";

            if (tickerText != "" && tickerText != "\0")
              resultText = String(Tickers[currentTicker]) + "  " + tickerText;
          }

          if (resultText != "")
          {
            Serial.println("Ticker Text: ");
            Serial.println(tickerText);

            Serial.println("Result Text: ");
            Serial.println(resultText);

            //char fuckMe[50];
            
            //resultText.toCharArray(fuckMe, 50);
            scrollText("TEST");
          }

          // Get next ticker price and display it
        } else {
          previousMillis = 0; // Proceed to the next one without delay
        }

        currentTicker++;
      }
    } else {
      scrollText("Connecting");
    }
  }
  // Connect to Wi-Fi
  else if (mode == 3) {

    if (previousConnectMillis == 0)
      previousConnectMillis = currentMillis;

    if (WiFi.status() != WL_CONNECTED) {
      // Try to connect for 15 seconds
      if (currentMillis - previousConnectMillis >= 15000) {
        scrollText("No Connection");
        Serial.println("No Connection");
        mode = 0;
        previousConnectMillis = 0; // Reset this timer
        captivePortalSetup(); // Enable the Captive portal to allow user to adjust settings
      } else {
        Serial.print(".");
      }
    }
    else {
      Serial.println("\nWe're connected!");
      mode = 1; // Ticker Mode
    }
  }
}