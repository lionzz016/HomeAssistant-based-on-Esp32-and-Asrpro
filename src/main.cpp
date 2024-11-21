/*
A few colour codes:

code	color
0x0000	Black
0xFFFF	White
0xBDF7	Light Gray
0x7BEF	Dark Gray
0xF800	Red
0xFFE0	Yellow
0xFBE0	Orange
0x79E0	Brown
0x7E0	Green
0x7FF	Cyan
0x1F	Blue
0xF81F	Pink

 */

/**
 * ==============================include blocks==================================
 * declare including headers file
 * TFT-LCD,ST7735S,SPI
 * DHT11
 */
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <WiFi.h>

#include <PubSubClient.h>

#include <Ticker.h>

#include <img/weather.h>
#include <img/stat.h>

#include <HTTPClient.h>
#include <ArduinoJson.h>

/**============================================================================== */

/**
 * ==============================defination blocks===============================
 * declare definations here blow
 * 所有具有潜在修改需求的值采用宏定义或静态字符串
 */
// DHT测温模块的信息引脚和类型
#define DHTPIN 4
#define DHTTYPE DHT11

// WIFI通信需要连接的网络信息
#define WIFI_SSID "Palette"
#define WIFI_PASSWORD "cjp886116"

// mqtt的服务地址和端口
#define MQTT_SERVER "mqtts.heclouds.com"
#define MQTT_PORT (uint16_t)1883

// 定义onenet接入的产品和设备
// 产品ID
#define CLIENT_ID "monitor"
// 设备名称
#define USERNAME "Hn5sqb3tOh"
// 设备的鉴权信息
#define PASSWORD "version=2018-10-31&res=products%2FHn5sqb3tOh%2Fdevices%2Fmonitor&et=1762853395&method=md5&sign=KAP2tyZkwpBcfI1uMvLpXA%3D%3D"

// 定义mqtt的主题的订阅和下发同步命令
//  设备订阅主题，平台就能发布同步命令，从而下发数据、指令给设备了
#define MQTT_TOPIC_GET "$sys/" USERNAME "/" CLIENT_ID "/cmd/request/+"
// 发布主题
#define MQTT_TOPIC_POST "$sys/" USERNAME "/" CLIENT_ID "/dp/post/json"
// 数据源的头head
#define MQTT_POST_BODY_FORMAT "{\"id\":%d,\"dp\":%s}"

// 天气api的私钥
#define API_KEY "SPIRhXLh-dt5N2tga"
// 目标地区的信息
#define API_LOCATION "Guangzhou"
// 数据形式，en：英文数据，zh-CN：中文数据
#define API_LANGUAGE "en"
// c：公制单位，f：英制单位
#define API_UNIT "c"

// 网络实时时间
// 服务器地址
#define TIME_NTP_SERVER "ntp.tencent.com"
// 当前时区矫正值：东八区：北京时间UTC/GMT+8:00
#define TIME_OFFSET_SEC 8 * 3600
// 当前时区的夏令时
#define TIME_OFFSET_DAYLIGHT_SEC 0
/**============================================================================== */

/**
 * ===============================value blocks===================================
 * declare values here blow
 */
TFT_eSPI tft = TFT_eSPI(); // Invoke library, pins defined in User_Setup.h
byte omm = 99;
bool initial = 1;
byte xcolon = 0;
unsigned int colour = 0;
uint32_t post_id = 0;

DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

HTTPClient httpClient;

const char *city;
const char *weather;
const char *temp;

// 声明获取的实时时间的结构体变量
struct tm timestamp;
// 需要显示的月份和星期数组
String MONTH[] = {"Jan.", "Feb.", "Mar.", "Apr.", "May", "Jun.", "Jul.", "Aug.", "Sep.", "Oct.", "Nov.", "Dec."};
String WEEKDAY[] = {"Sun.", "Mon.", "Tues.", "Wed.", "Thur.", "Fri.", "Sat."};

// 串口回调函数接收AsrPro发送的十六进制数据的sta变量，采用全局变量是为了其他函数的使用
uint8_t sta = 0xFF;

bool wifi_stat = false;
bool light_stat = false;
bool fan_stat = false;

/**============================================================================= */

/**
 * =========================customerized functions blocks=======================
 * declare customerized functions here blow
 */

void setWiFi();
void setMqtt();

void Callback(char *topic, byte *payload, unsigned int length);
void sendData();
void getWeather();
void updateImg(const char *city, const char *weatherStat, const char *temp);
void getStat_ASRPRO();

Ticker tim1(sendData, 200);
Ticker tim2(getWeather, 5000);
Ticker tim3(setWiFi, 100);
Ticker tim4(setMqtt, 1000);

// 获取完整天气api的连接，设置参数是为了低耦合
String getURL(const char *key, const char *location, const char *lang, const char *unit)
{
  String s = "https://api.seniverse.com/v3/weather/now.json?key=" + String(key) + "&location=" + String(location) + "&language=" + String(lang) + "&unit=" + String(unit);
  return s;
}
/**============================================================================= */

/**
 * ============================initalized entry block===========================
 */
void setup(void)
{
  Serial.begin(115200);

  Serial2.begin(115200);
  Serial2.onReceive(getStat_ASRPRO);

  // 获取网络时间
  configTime(TIME_OFFSET_SEC, TIME_OFFSET_DAYLIGHT_SEC, TIME_NTP_SERVER);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);

  tft.fillSmoothRoundRect(0, 50, 63, 3, 2, 0xeff);
  tft.fillSmoothRoundRect(88, 50, 73, 3, 2, 0xeff);
  tft.drawSmoothArc(75, 68, 22, 20, 0, 360, 0xeff, TFT_TRANSPARENT);

  tft.setSwapBytes(true);
  tft.pushImage(146, 2, 13, 13, img_fan_off);
  tft.pushImage(144, 17, 16, 16, img_light_off);

  dht.begin();

  WiFi.mode(WIFI_STA);
  if (!WiFi.getAutoReconnect())
    WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  tim3.start();

  tim4.start();

  tim1.start();
  tim2.start();
}
/**============================================================================ */

/**
 * ============================program entry block=============================
 */
void loop()
{
  getLocalTime(&timestamp, 50);

  if (timestamp.tm_sec == 0 || initial)
  {
    initial = 0;
  }

  byte xpos = 6;
  byte ypos = 0;
  if (omm != timestamp.tm_min)
  {
    tft.setTextColor(0x39C4, TFT_BLACK);
    tft.drawString("88:88", xpos, ypos, 7);
    tft.setTextColor(0xFBE0);
    omm = timestamp.tm_min;

    if (timestamp.tm_hour < 10)
      xpos += tft.drawChar('0', xpos, ypos, 7);
    xpos += tft.drawNumber(timestamp.tm_hour, xpos, ypos, 7);
    xcolon = xpos;
    xpos += tft.drawChar(':', xpos, ypos, 7);
    if (timestamp.tm_min < 10)
      xpos += tft.drawChar('0', xpos, ypos, 7);
    tft.drawNumber(timestamp.tm_min, xpos, ypos, 7);
  }

  if (timestamp.tm_sec % 2)
  {
    tft.setTextColor(0x39C4, TFT_BLACK);
    xpos += tft.drawChar(':', xcolon, ypos, 7);
    tft.setTextColor(0xFBE0, TFT_BLACK);
  }
  else
  {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawChar(':', xcolon, ypos, 7);
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawNumber(timestamp.tm_sec / 10, 62, 58, 4);
  tft.drawNumber(timestamp.tm_sec % 10, 75, 58, 4);

  if (getLocalTime(&timestamp, 50))
  {
    tft.drawNumber(timestamp.tm_year + 1900, 2, 55, 1);
    tft.drawString(WEEKDAY[timestamp.tm_wday], 30, 55, 1);
    tft.drawString(MONTH[timestamp.tm_mon], 2, 63, 2);
    tft.drawNumber(timestamp.tm_mday, 34, 63, 2);
  }

  tft.fillSmoothRoundRect(0, 91, 12, 128 - 91, 2, TFT_GOLD);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawFloat(dht.readTemperature(), 1, 14, 91, 4);
  tft.drawString("Temperature", 14, 112, 2);
  tft.drawString("°C", 65, 91, 4);

  tft.fillSmoothRoundRect(96, 91, 12, 128 - 91, 2, TFT_DARKGREEN);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawFloat(dht.readHumidity(), 0, 110, 91, 4);
  tft.drawString("Humidity", 110, 112, 2);
  tft.drawString("%", 138, 91, 4);

  tim3.update();
  if (WiFi.status() == WL_DISCONNECTED || WiFi.status() == WL_CONNECTION_LOST)
    tim3.resume();
  else if (WiFi.status() == WL_CONNECTED)
    tim3.pause();

  tim4.update();
  if (!client.connected())
    tim4.resume();

  client.loop();
  tim1.update();
  tim2.update();

  tft.setSwapBytes(true);
  (WiFi.status() == WL_CONNECTED) ? tft.pushImage(144, 33, 16, 16, img_wifi_on)
                                  : tft.pushImage(144, 33, 16, 16, img_wifi_off);

  if (sta == 0x01)
  {
    tft.pushImage(144, 17, 16, 16, img_light_on);
    light_stat = true;
  }
  if (sta == 0x00)
  {
    tft.pushImage(144, 17, 16, 16, img_light_off);
    light_stat = false;
  }
  if (sta == 0x11)
  {
    tft.pushImage(146, 2, 13, 13, img_fan_on);
    fan_stat = true;
  }
  if (sta == 0x10)
  {
    tft.pushImage(146, 2, 13, 13, img_fan_off);
    fan_stat = false;
  }

  if (sta == 0xDF)
  {
    char s[15];
    Serial2.printf("%s,%s", weather, temp);
    sprintf(s, "%s,%s", weather, temp);
    Serial.println(s);
    // Serial.println(String(s).substring(0, (String(s).indexOf(",", 1))));
    // Serial.println(String(s).substring(String(s).indexOf(",", 1) + 1));
    sta = 0xFF;
  }

  if (sta == 0xBF)
  {
    Serial2.printf("%d-%d/%d.%d,%d:%d", timestamp.tm_year + 1900, timestamp.tm_mon + 1, timestamp.tm_mday, timestamp.tm_wday, timestamp.tm_hour, timestamp.tm_min);
    sta = 0xFF;
  }

  WiFi.status() == WL_CONNECTED ? wifi_stat = true : wifi_stat = false;
}

/**
 * 设置Wifi网络相关参数
 */
void setWiFi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Connecting...");
  }
  else
  {
    Serial.println("Wifi Connected.");

    Serial.print("Local IP Address:");
    Serial.println(WiFi.localIP());
    // Serial.print("Subnet Mask:");
    // Serial.println(WiFi.subnetMask());

    // Serial.print("Gateway IP:");
    // Serial.println(WiFi.gatewayIP());

    // Serial.print("DNS IP Address:");
    // Serial.println(WiFi.dnsIP());
    Serial.print("MAC Address:");
    Serial.println(WiFi.macAddress());

    Serial.print("Hostname:");
    Serial.println(WiFi.getHostname());
  }
}

/**
 * 连接OneNet，设置MQTT
 */
void setMqtt()
{
  if (!client.connected())
  {
    Serial.println("MQTT connected failure!");
    Serial.print("State Code:");
    Serial.println(client.state());
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.connect(CLIENT_ID, USERNAME, PASSWORD);
  }
  else
  {
    Serial.println("MQTT connected!");
    client.subscribe(MQTT_TOPIC_GET);
    client.setCallback(Callback);
    tim4.pause();
  }
}

// 订阅MQTT所发布的主题的回调函数
void Callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  // Handle incoming message here
  String message = "";
  const char *target = "";
  JsonDocument doc;
  for (int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }
  DeserializationError error = deserializeJson(doc, message);
  if (!error)
  {
    target = doc["command"];
    // Serial.println(target);
  }
  else
    Serial.print("invalid json format!");

  if (!strcmp("0x01", target))
  {
    sta = 0x01;
    light_stat = true;
  }
  else if (!strcmp("0x00", target))
  {
    sta = 0x00;
    light_stat = false;
  }
  else if (!strcmp("0x11", target))
  {
    sta = 0x11;
    fan_stat = true;
  }
  else if (!strcmp("0x10", target))
  {
    sta = 0x10;
    fan_stat = false;
  }

  else
    target = "error stat code!";
  Serial2.write(sta);
  Serial.print("Received message:");
  Serial.println(target);
  // Parse message as JSON
}

/**
 * 通过MQTT协议在OneNet上发布主题（上传温度、湿度灯数据）的函数，数据采用Json格式
 */
void sendData()
{
  if (client.connected())
  {

    // 先拼接出json字符串
    char param[169];
    char jsonBuf[252];

    sprintf(param,
            "{ \"temp\":[{\"v\": %.2f}], \"humi\":[{ \"v\": %.2f}], \"wifi\": [{ \"v\": %d}], \"light\": [{ \"v\": %d}], \"fan\": [{ \"v\": %d}] }",
            dht.readTemperature(), dht.readHumidity(), wifi_stat, light_stat, fan_stat); // 我们把要上传的数据写在param里
    post_id += 1;
    sprintf(jsonBuf, MQTT_POST_BODY_FORMAT, post_id, param);

    // client.publish("$dp", (uint8_t *)msg_buf, 3+strlen(msgJson));
    client.publish(MQTT_TOPIC_POST, jsonBuf);
    // 发送数据到主题
    // delay(20);
  }
  else
    tim4.resume();
}

/**
 * 实时获取天气相关状况函数
 */
void getWeather()
{
  JsonDocument doc;
  httpClient.begin(getURL(API_KEY, API_LOCATION, API_LANGUAGE, API_UNIT));
  if (httpClient.GET() > 0 && httpClient.GET() == HTTP_CODE_OK)
  {
    String json = httpClient.getString();
    DeserializationError error = deserializeJson(doc, json);
    if (!error)
    {
      city = doc["results"][0]["location"]["name"];
      weather = doc["results"][0]["now"]["text"];
      temp = doc["results"][0]["now"]["temperature"];
      updateImg(city, weather, temp);
      Serial.print(city);
      Serial.print(",");
      Serial.print(weather);
      Serial.print(",");
      Serial.println(temp);
    }
  }
  else
    Serial.println(httpClient.GET());
}

/**
 * 天气、地区和气温相关状态显示函数
 * @param city 地区名称，如：Guangzhou
 * @param weatherStat 天气状况，如：Cloudy
 * @param temp 气温，如：23
 */
void updateImg(const char *city, const char *weatherStat, const char *temp)
{
  String s(weatherStat);

  tft.setTextColor(TFT_PINK, TFT_BLACK);
  tft.drawString(city, 2, 80, 1);
  tft.drawString(temp, 141, 56, 2);

  // 晴天，不区分早晚
  if (s.equals("Sunny") || s.equals("Clear"))
  {
    tft.pushImage(109, 60, 26, 27, img_sunny);
    tft.drawString("SN", 141, 75, 2);
  }
  // 多云
  else if (s.equals("Cloudy"))
  {
    tft.pushImage(109, 60, 27, 27, img_cloudy);
    tft.drawString("CL", 141, 75, 2);
  }
  else if (s.equals("Partly cloudy"))
  {
    tft.pushImage(109, 60, 27, 27, img_cloudy);
    tft.drawString("PC", 141, 75, 2);
  }
  else if (s.equals("Mostly cloudy"))
  {
    tft.pushImage(109, 60, 27, 27, img_cloudy);
    tft.drawString("MC", 141, 75, 2);
  }
  // 阴天
  else if (s.equals("Overcast"))
  {
    tft.pushImage(109, 60, 27, 27, img_overcast);
    tft.drawString("OC", 141, 75, 2);
  }
  // 阵雨、雷阵雨
  else if (s.equals("Shower"))
  {
    tft.pushImage(109, 60, 27, 27, img_shower);
    tft.drawString("SW", 141, 75, 2);
  }
  else if (s.equals("Thundershower"))
  {
    tft.pushImage(109, 60, 27, 27, img_thunder_shower);
    tft.drawString("TS", 141, 75, 2);
  }
  // 中、小雨
  else if (s.equals("Light rain"))
  {
    tft.pushImage(109, 60, 27, 27, img_rainy);
    tft.drawString("LR", 141, 75, 2);
  }
  else if (s.equals("Moderate rain"))
  {
    tft.pushImage(109, 60, 27, 27, img_rainy);
    tft.drawString("MR", 141, 75, 2);
  }
  // 特大、大暴雨
  else if (s.equals("Heavy rain"))
  {
    tft.pushImage(109, 60, 27, 27, img_heavy_rainy);
    tft.drawString("HR", 141, 75, 2);
  }
  else if (s.equals("Storm"))
  {
    tft.pushImage(109, 60, 27, 27, img_heavy_rainy);
    tft.drawString("ST", 141, 75, 2);
  }
  // 雾
  else if (s.equals("Foggy"))
  {
    tft.pushImage(109, 60, 27, 27, img_foggy);
    tft.drawString("FG", 141, 75, 2);
  }
  // 霾
  else if (s.equals("Haze"))
  {
    tft.pushImage(109, 60, 27, 27, img_haze);
    tft.drawString("HZ", 141, 75, 2);
  }
  // 有风
  else if (s.equals("Windy"))
  {
    tft.pushImage(109, 60, 27, 21, img_windy);
    tft.drawString("WD", 141, 75, 2);
  }
  // 大风
  else if (s.equals("Blustery"))
  {
    tft.pushImage(109, 60, 27, 21, img_windy);
    tft.drawString("BL", 141, 75, 2);
  }
  // 寒冷
  else if (s.equals("Cold"))
  {
    tft.pushImage(109, 60, 27, 27, img_cold);
    tft.drawString("CD", 141, 75, 2);
  }
}

/**
 *  串口2回调函数，接收AsrPro串口发送的数据（状态码，如灯光状态、风扇状态），并处理
 */
void getStat_ASRPRO()
{
  if (Serial2.available())
  {
    Serial2.read(&sta, 1);
  }
  // Serial.println(objBuffer);
  // Serial.println(statBuffer);
}
