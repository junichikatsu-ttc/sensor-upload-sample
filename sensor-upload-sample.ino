#include <Arduino.h>
#include <Wire.h>
#include <WiFiS3.h>
#include <WiFiSSLClient.h>
#include <ArduinoJson.h>

#include "config.h"

#if ENABLE_BME280
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
Adafruit_BME280 bme;
bool bmeReady = false;
#endif

#if ENABLE_MPU6050
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
Adafruit_MPU6050 mpu;
bool mpuReady = false;
#endif

// Wi-Fi SSL クライアント
WiFiSSLClient sslClient;
unsigned long lastUploadTime = 0;

// Wi-Fi 接続関数
void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.println();
  Serial.print("Wi-Fiに接続中: ");
  Serial.println(WIFI_SSID);

  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print(".");
    delay(2000);
  }

  Serial.println();
  Serial.println("Wi-Fi接続成功!");
  Serial.print("IPアドレス: ");
  Serial.println(WiFi.localIP());

  // NTP時刻取得待ち (WiFiS3標準機能)
  Serial.print("NTP時刻同期中...");
  unsigned long epoch = 0;
  int retry = 0;
  while (epoch == 0 && retry < 10) {
    epoch = WiFi.getTime();
    if (epoch == 0) {
      Serial.print(".");
      delay(1000);
      retry++;
    }
  }
  Serial.println();
  if (epoch > 0) {
    Serial.print("現在UNIXエポック秒: ");
    Serial.println(epoch);
  } else {
    Serial.println("NTP時刻同期タイムアウト（後ほど再試行されます）");
  }
}

// 現在のUNIX時刻ミリ秒を取得
uint64_t getUnixTimeMillis() {
  unsigned long epochSec = WiFi.getTime();
  if (epochSec == 0) {
    // NTP未同期時は内部タイマーミリ秒を使用
    return (uint64_t)millis();
  }
  return ((uint64_t)epochSec * 1000ULL) + (millis() % 1000);
}

// クラウドへセンサーデータをHTTPS POSTでアップロード
bool uploadSensorData(const String& jsonPayload) {
  Serial.println("----------------------------------------");
  Serial.println("サーバーへ送信中: " + String(SERVER_HOST) + String(SERVER_PATH));
  Serial.println("ペイロード: " + jsonPayload);

  // HTTPS (ポート443) で接続
  if (!sslClient.connect(SERVER_HOST, SERVER_PORT)) {
    Serial.println("[エラー] サーバーへのSSL接続に失敗しました。");
    return false;
  }

  // HTTP POST リクエストの送信
  sslClient.println("POST " + String(SERVER_PATH) + " HTTP/1.1");
  sslClient.println("Host: " + String(SERVER_HOST));
  sslClient.println("User-Agent: Arduino-UNO-R4-WiFi/1.0");
  sslClient.println("Content-Type: application/json");
  sslClient.println("Connection: close");
  sslClient.print("Content-Length: ");
  sslClient.println(jsonPayload.length());
  sslClient.println();
  sslClient.print(jsonPayload);

  // レスポンス待機
  unsigned long timeout = millis();
  while (sslClient.available() == 0) {
    if (millis() - timeout > 10000) {
      Serial.println("[エラー] レスポンス待機タイムアウト");
      sslClient.stop();
      return false;
    }
    delay(50);
  }

  // 1行目（HTTPステータス行）の読み取り
  String statusLine = sslClient.readStringUntil('\n');
  Serial.println("サーバー応答: " + statusLine);

  // 残りのレスポンスを読み捨てて切断
  while (sslClient.available()) {
    sslClient.read();
  }
  sslClient.stop();

  bool isSuccess = statusLine.indexOf("200") >= 0 || statusLine.indexOf("201") >= 0;
  if (isSuccess) {
    Serial.println("アップロード成功!");
  } else {
    Serial.println("[警告] サーバーから200/201以外のステータスが返されました。");
  }
  Serial.println("----------------------------------------");
  return isSuccess;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ; // シリアル接続待機（最大3秒）
  }

  Serial.println();
  Serial.println("========================================");
  Serial.println("Arduino UNO R4 WiFi センサーアップロード");
  Serial.println("========================================");

  // I2Cバス初期化
  Wire.begin();

  // BME280初期化
#if ENABLE_BME280
  Serial.print("BME280初期化中 (0x");
  Serial.print(BME280_I2C_ADDR, HEX);
  Serial.print(")... ");
  if (bme.begin(BME280_I2C_ADDR)) {
    bmeReady = true;
    Serial.println("成功!");
  } else {
    Serial.println("失敗! 配線またはI2Cアドレスを確認してください。");
  }
#endif

  // MPU-6050初期化
#if ENABLE_MPU6050
  Serial.print("MPU-6050初期化中 (0x");
  Serial.print(MPU6050_I2C_ADDR, HEX);
  Serial.print(")... ");
  if (mpu.begin(MPU6050_I2C_ADDR)) {
    mpuReady = true;
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println("成功!");
  } else {
    Serial.println("失敗! 配線またはI2Cアドレスを確認してください。");
  }
#endif

  // Wi-Fi 接続
  connectToWiFi();
}

void loop() {
  // Wi-Fi 切断時の自動再接続
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastUploadTime >= UPLOAD_INTERVAL) {
    lastUploadTime = currentMillis;

    // JSON ドキュメント構築 (ArduinoJson v7)
    JsonDocument doc;

    // 共通項目
    doc["no"] = SENSOR_NUMBER;
    doc["ts"] = getUnixTimeMillis();

    float primaryTemp = 0.0;
    bool tempAssigned = false;

    // BME280 のデータ取得
#if ENABLE_BME280
    if (bmeReady) {
      float bmeTemp = bme.readTemperature();
      float bmeHumidity = bme.readHumidity();
      float bmePressure = bme.readPressure() / 100.0F; // hPa

      primaryTemp = bmeTemp;
      tempAssigned = true;

      doc["temp"] = primaryTemp;
      doc["humidity"] = bmeHumidity;
      doc["pressure"] = bmePressure;
    }
#endif

    // MPU-6050 のデータ取得
#if ENABLE_MPU6050
    if (mpuReady) {
      sensors_event_t a, g, temp;
      mpu.getEvent(&a, &g, &temp);

      // BME280が未設定の場合はMPUの温度をメイン温度にする
      if (!tempAssigned) {
        primaryTemp = temp.temperature;
        tempAssigned = true;
        doc["temp"] = primaryTemp;
      } else {
        doc["mpu_temp"] = temp.temperature;
      }

      JsonObject accel = doc["accel"].to<JsonObject>();
      accel["x"] = a.acceleration.x;
      accel["y"] = a.acceleration.y;
      accel["z"] = a.acceleration.z;

      JsonObject gyro = doc["gyro"].to<JsonObject>();
      gyro["x"] = g.gyro.x;
      gyro["y"] = g.gyro.y;
      gyro["z"] = g.gyro.z;
    }
#endif

    // センサー未初期化時のフォールバック
    if (!tempAssigned) {
      doc["temp"] = 0.0;
    }

    // JSON文字列にシリアライズ
    String payload;
    serializeJson(doc, payload);

    // アップロード実行
    uploadSensorData(payload);
  }

  delay(10);
}
