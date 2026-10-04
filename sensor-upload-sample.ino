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
bool mpuReady = false;

// MPU-6050 / 互換チップ (0x74等) の初期化
bool initMPUDirect(uint8_t addr) {
  // 1. スリープ解除 (PWR_MGMT_1 レジスタ 0x6B に 0x00 を書き込み)
  Wire.beginTransmission(addr);
  Wire.write(0x6B);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  delay(10);

  // 2. 加速度設定: ±8g (ACCEL_CONFIG レジスタ 0x1C に 0x10 を書き込み)
  Wire.beginTransmission(addr);
  Wire.write(0x1C);
  Wire.write(0x10);
  Wire.endTransmission();

  // 3. ジャイロ設定: ±500 deg/s (GYRO_CONFIG レジスタ 0x1B に 0x08 を書き込み)
  Wire.beginTransmission(addr);
  Wire.write(0x1B);
  Wire.write(0x08);
  Wire.endTransmission();

  return true;
}

// MPU-6050 / 互換チップから14バイトを一括読み出し
bool readMPUDirect(uint8_t addr, float &ax, float &ay, float &az, float &gx, float &gy, float &gz, float &tempC) {
  Wire.beginTransmission(addr);
  Wire.write(0x3B); // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  // 14バイト要求 (Accel X,Y,Z: 6バイト, Temp: 2バイト, Gyro X,Y,Z: 6バイト)
  if (Wire.requestFrom((uint8_t)addr, (uint8_t)14) != 14) {
    return false;
  }

  int16_t rawAx = (Wire.read() << 8) | Wire.read();
  int16_t rawAy = (Wire.read() << 8) | Wire.read();
  int16_t rawAz = (Wire.read() << 8) | Wire.read();
  int16_t rawTemp = (Wire.read() << 8) | Wire.read();
  int16_t rawGx = (Wire.read() << 8) | Wire.read();
  int16_t rawGy = (Wire.read() << 8) | Wire.read();
  int16_t rawGz = (Wire.read() << 8) | Wire.read();

  // 加速度 (±8g, 4096 LSB/g) -> m/s^2 (1g = 9.80665 m/s^2)
  ax = (rawAx / 4096.0) * 9.80665;
  ay = (rawAy / 4096.0) * 9.80665;
  az = (rawAz / 4096.0) * 9.80665;

  // 温度 (°C)
  tempC = (rawTemp / 340.0) + 36.53;

  // ジャイロ (±500 deg/s, 65.5 LSB/(deg/s)) -> rad/s
  gx = (rawGx / 65.5) * (PI / 180.0);
  gy = (rawGy / 65.5) * (PI / 180.0);
  gz = (rawGz / 65.5) * (PI / 180.0);

  return true;
}
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

// I2C バススキャン＆MPU-6050診断関数
void scanI2CBus() {
  Serial.println("\n--- [診断] I2C バススキャン開始 ---");
  byte count = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C デバイス検出: 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);

      if (address == 0x68 || address == 0x69) {
        Serial.print(" (MPU-6050 / 6500候補)");
        // WHO_AM_I レジスタ (0x75) を直接読み取り
        Wire.beginTransmission(address);
        Wire.write(0x75);
        if (Wire.endTransmission(false) == 0) {
          Wire.requestFrom((uint8_t)address, (uint8_t)1);
          if (Wire.available()) {
            byte whoami = Wire.read();
            Serial.print(" -> WHO_AM_I (Chip ID): 0x");
            Serial.print(whoami, HEX);
            if (whoami == 0x68) {
              Serial.print(" [MPU-6050 純正]");
            } else if (whoami == 0x70) {
              Serial.print(" [MPU-6500 互換チップ]");
            } else if (whoami == 0x72 || whoami == 0x98) {
              Serial.print(" [MPU互換/ICMシリーズ]");
            } else {
              Serial.print(" [未知のID]");
            }
          }
        }
      } else if (address == 0x76 || address == 0x77) {
        Serial.print(" (BME280 / BMP280候補)");
      }
      Serial.println();
      count++;
    } else if (error == 4) {
      Serial.print("0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" で通信エラー (error=4)");
    }
  }

  if (count == 0) {
    Serial.println("[警告] I2Cデバイスが1台も検出されませんでした！");
    Serial.println("  1. 配線 (VCC, GND, SDA, SCL) が接触不良になっていないか確認してください。");
    Serial.println("  2. GY-521モジュール等の場合、VCCは 5V ピンに接続してみてください (内蔵レギュレータのため)。");
    Serial.println("  3. SDA/SCLピンが正しく接続されているか確認してください (A4/A5 または専用SDA/SCL)。");
  } else {
    Serial.print("I2Cスキャン完了: 計 ");
    Serial.print(count);
    Serial.println(" 台のデバイスが応答しました。");
  }
  Serial.println("-----------------------------------\n");
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
  delay(100); // センサーの起動待ち

  // I2Cスキャンを実行して接続状態を診断
  scanI2CBus();

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
  if (initMPUDirect(MPU6050_I2C_ADDR)) {
    mpuReady = true;
    Serial.println("成功! (互換チップ対応モード)");
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
      float ax = 0, ay = 0, az = 0;
      float gx = 0, gy = 0, gz = 0;
      float mpuTemp = 0;

      if (readMPUDirect(MPU6050_I2C_ADDR, ax, ay, az, gx, gy, gz, mpuTemp)) {
        // BME280が未設定の場合はMPUの温度をメイン温度にする
        if (!tempAssigned) {
          primaryTemp = mpuTemp;
          tempAssigned = true;
          doc["temp"] = primaryTemp;
        } else {
          doc["mpu_temp"] = mpuTemp;
        }

        JsonObject accel = doc["accel"].to<JsonObject>();
        accel["x"] = ax;
        accel["y"] = ay;
        accel["z"] = az;

        JsonObject gyro = doc["gyro"].to<JsonObject>();
        gyro["x"] = gx;
        gyro["y"] = gy;
        gyro["z"] = gz;
      } else {
        Serial.println("[警告] MPU-6050 データ読み出し失敗");
      }
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
