#ifndef CONFIG_H
#define CONFIG_H

// ==========================================
// 1. Wi-Fi 接続設定
// ==========================================
#define WIFI_SSID     "YOUR_WIFI_SSID"      // Wi-FiのSSID
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"  // Wi-Fiのパスワード

// ==========================================
// 2. クラウド（enebular / サーバー）設定
// ==========================================
#define SENSOR_NUMBER   "2*3670**"                    // センサー識別番号
#define SERVER_HOST     "lcdp003.enebular.com"        // サーバーホスト名
#define SERVER_PORT     443                           // HTTPS ポート
#define SERVER_PATH     "/ttc-iot-sensor/"            // POST 送信先パス
#define UPLOAD_INTERVAL 5000                          // アップロード間隔 (ミリ秒)

// ==========================================
// 3. センサー有効化フラグ（true / false で切り替え）
// ==========================================
#define ENABLE_BME280   true   // BME280 (温度・湿度・気圧) を有効化
#define ENABLE_MPU6050  true   // MPU-6050 (加速度・ジャイロ・温度) を有効化

// ==========================================
// 4. I2C アドレス設定
// ==========================================
// BME280: 一般的に 0x76 または 0x77
#define BME280_I2C_ADDR 0x76

// MPU-6050: 一般的に 0x68 または 0x69
#define MPU6050_I2C_ADDR 0x68

#endif // CONFIG_H
