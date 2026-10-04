# Arduino UNO R4 WiFi センサーデータ クラウドアップロード サンプル

Arduino UNO R4 WiFi を使用して、**BME280**（温湿度・気圧センサー）および **MPU-6050**（6軸加速度・ジャイロセンサー）のデータを取得し、enebular / クラウドサーバーへ HTTPS POST で送信するサンプルプログラムです。

**PlatformIO** および **Arduino IDE** の両方でそのまま開いて開発・ビルドできるように構成されています。

---

## 1. ハードウェア接続 (I2C)

Arduino UNO R4 WiFi と各センサーは I2C（SDA / SCL）で接続します。両センサーを同時に接続することも可能です。

| Arduino UNO R4 WiFi | BME280 | MPU-6050 | 備考 |
|---|---|---|---|
| **3.3V** または **5V** | VCC / VIN | VCC | センサーモジュールの定格電圧に合わせてください |
| **GND** | GND | GND | 共通グランド |
| **SDA** (A4 または 専用SDAピン) | SDA | SDA | I2C データ線 |
| **SCL** (A5 または 専用SCLピン) | SCL | SCL | I2C クロック線 |

---

## 2. 設定ファイルの編集 (`config.h`)

[`config.h`](config.h) を開いて、お使いの環境に合わせて設定してください。

```cpp
// 1. Wi-Fi設定
#define WIFI_SSID     "YOUR_WIFI_SSID"      // Wi-FiのSSID
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"  // Wi-Fiのパスワード

// 2. クラウド設定
#define SENSOR_NUMBER   "2*3670**"                    // センサー識別番号
#define SERVER_HOST     "lcdp003.enebular.com"        // サーバーホスト名
#define SERVER_PORT     443                           // HTTPS ポート
#define SERVER_PATH     "/ttc-iot-sensor/"            // POST 送信先パス
#define UPLOAD_INTERVAL 5000                          // 送信間隔 (ミリ秒)

// 3. センサーの有効/無効切り替え
#define ENABLE_BME280   true   // BME280 を使う場合は true、使わない場合は false
#define ENABLE_MPU6050  true   // MPU-6050 を使う場合は true、使わない場合は false

// 4. I2Cアドレス
#define BME280_I2C_ADDR 0x76   // モジュールにより 0x76 または 0x77
#define MPU6050_I2C_ADDR 0x68  // モジュールにより 0x68 または 0x69
```

---

## 3. 送信される JSON データ形式

元の JavaScript サンプルの形式（`no`, `ts`, `temp`）を踏襲しつつ、センサーに応じた全項目が含まれます。

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
  }
}
```

- `ts`: UNO R4 WiFi 内蔵の NTP 同期機能から取得した UNIX 時刻（ミリ秒）
- `temp`: BME280 有効時は BME280 の温度、MPU-6050 のみの場合は MPU-6050 の温度が入ります

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
     *(※依存する Adafruit Unified Sensor や Adafruit BusIO も「Install all」で自動導入されます)*
4. **書き込みと実行**:
   - 「書き込み」ボタンをクリックします。
   - 「シリアルモニタ」を **115200 bps** で開くと、Wi-Fi接続ログやHTTP送信結果を確認できます。
