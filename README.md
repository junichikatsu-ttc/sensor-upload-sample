# Arduino UNO R4 WiFi センサーデータ クラウドアップロード サンプル

Arduino UNO R4 WiFi を使用して、各種センサーのデータを取得し、enebular / クラウドサーバーへ HTTPS POST で送信するサンプルプログラムです。

対応センサー：
- **BME280 / BMP280**（温湿度・気圧センサー / I2C ※自動判別対応、BMP280時は温度・気圧）
- **MPU-6050**（6軸加速度・ジャイロセンサー / I2C）
- **GROVE 超音波距離センサモジュール** ([SKU: 1383](https://www.switch-science.com/products/1383) / デジタルピン)
- **GROVE デジタル温度・湿度センサ DHT11** ([SKU: 818](https://www.switch-science.com/products/818) / デジタルピン)

**PlatformIO** および **Arduino IDE** の両方でそのまま開いて開発・ビルドできるように構成されています。

---

## 1. ハードウェア接続

### A. I2C センサー接続 (BME280 / MPU-6050)
Arduino UNO R4 WiFi と各センサーは I2C（SDA / SCL）で接続します。両センサーを同時に接続することも可能です。

| Arduino UNO R4 WiFi | BME280 | MPU-6050 | 備考 |
|---|---|---|---|
| **3.3V** または **5V** | VCC / VIN | VCC | センサーモジュールの定格電圧に合わせてください |
| **GND** | GND | GND | 共通グランド |
| **SDA** (A4 または 専用SDAピン) | SDA | SDA | I2C データ線 |
| **SCL** (A5 または 専用SCLピン) | SCL | SCL | I2C クロック線 |

### B. GROVE デジタルセンサー接続 (超音波距離センサ / DHT11温湿度センサ)
Grove Base Shield 等を使用してデジタルポートに接続するか、Groveケーブルのピン（SIG, NC, VCC, GND）を直接配線します。

| センサー | Arduino UNO R4 WiFi ピン (デフォルト) | 備考 |
|---|---|---|
| **GROVE 超音波距離センサ (1383)** | **D7** (SIGピン) / 5V / GND | `config.h` の `ULTRASONIC_PIN` で変更可能 |
| **GROVE デジタル温湿度センサ (818)** | **D3** (SIGピン) / 5V / GND | `config.h` の `DHT11_PIN` で変更可能 |

---

## 2. 設定ファイルの編集 (`config.h`)

[`config.h`](config.h) を開いて、お使いの環境に合わせて設定してください。

```cpp
// 1. Wi-Fi設定
#define WIFI_SSID     "YOUR_WIFI_SSID"      // Wi-FiのSSID
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"  // Wi-Fiのパスワード

// 2. クラウド設定
#define SENSOR_NUMBER   "2*3670**"                    // センサー識別番号
#define SERVER_HOST     "lcdp005.enebular.com"        // サーバーホスト名
#define SERVER_PORT     443                           // HTTPS ポート
#define SERVER_PATH     "/ttc-iot-sensor-mcp/v1/sensors" // POST 送信先パス
#define UPLOAD_INTERVAL 5000                          // 送信間隔 (ミリ秒)

// 3. センサーの有効/無効切り替え (true / false)
#define ENABLE_BME280     true   // BME280 (温度・湿度・気圧)
#define ENABLE_MPU6050    true   // MPU-6050 (加速度・ジャイロ・温度)
#define ENABLE_ULTRASONIC true   // GROVE 超音波距離センサ (SKU: 1383)
#define ENABLE_DHT11      true   // GROVE デジタル温湿度センサ DHT11 (SKU: 818)

// 4. ピン / I2C アドレス設定
#define BME280_I2C_ADDR   0x76  // モジュールにより 0x76 または 0x77
#define MPU6050_I2C_ADDR  0x68  // モジュールにより 0x68 または 0x69
#define ULTRASONIC_PIN    7     // 超音波距離センサ接続ピン (例: D7)
#define DHT11_PIN         3     // DHT11温湿度センサ接続ピン (例: D3)
```

---

## 3. 送信される JSON データ形式

元の JavaScript サンプルの形式（`no`, `ts`, `temp`）を踏襲しつつ、有効にしたセンサーに応じた項目が含まれます。

```json
{
  "no": "2*3670**",
  "ts": 1728000000000,
  "temp": 24.85,
  "humidity": 52.3,
  "pressure": 1012.4,
  "mpu_temp": 25.1,
  "accel": {
    "x": 0.02,
    "y": -0.15,
    "z": 9.81
  },
  "gyro": {
    "x": 0.001,
    "y": -0.002,
    "z": 0.000
  },
  "distance": 35,
  "dht_temp": 24.0,
  "dht_humidity": 55.0
}
```

- `ts`: UNO R4 WiFi 内蔵の NTP 同期機能から取得した UNIX 時刻（ミリ秒）
- `temp`: メイン温度。BME280 有効時は BME280 の温度、BME280 が無く DHT11 のみ有効な場合は DHT11 の温度、MPU-6050 のみの場合は MPU-6050 の温度が入ります（併用時は `dht_temp`, `mpu_temp` として追加格納）。
- `humidity`: メイン湿度。BME280 有効時は BME280 の湿度、BME280 が無く DHT11 のみ有効な場合は DHT11 の湿度が入ります（併用時は `dht_humidity` として追加格納）。
- `distance`: 超音波距離センサーが有効な場合の測定距離（cm）。

---

## 4. 開発方法

### A. PlatformIO を使用する場合

すでにプロジェクトが初期化されており、ライブラリ依存関係も `platformio.ini` に設定済みです。

- **ビルド**:
  ```bash
  pio run
  ```
- **マイコンへ書き込み**:
  ```bash
  pio run -t upload
  ```
- **シリアルモニター（通信確認）**:
  ```bash
  pio device monitor -b 115200
  ```

---

### B. Arduino IDE を使用する場合

1. **プロジェクトを開く**:
   - Arduino IDE を起動し、`ファイル` → `開く...` から本フォルダー内の [`sensor-upload-sample.ino`](sensor-upload-sample.ino) を開きます。
2. **ボード選択**:
   - ボード: `Arduino UNO R4 WiFi`
   - ポート: 接続されているシリアルポートを選択
3. **必要なライブラリのインストール**:
   - Arduino IDE の「ライブラリマネージャー」（左側の本棚アイコン）を開き、以下を検索してインストールしてください：
     - **ArduinoJson** (by Benoit Blanchon, v7系)
     - **Adafruit BME280 Library** (by Adafruit)
     - **Adafruit MPU6050** (by Adafruit)
     - **DHT sensor library** (by Adafruit)
     - **Grove Ultrasonic Ranger** (by Seeed Studio)
     *(※依存する Adafruit Unified Sensor や Adafruit BusIO も「Install all」で自動導入されます)*
4. **書き込みと実行**:
   - 「書き込み」ボタンをクリックします。
   - 「シリアルモニタ」を **115200 bps** で開くと、Wi-Fi接続ログやHTTP送信結果を確認できます。

