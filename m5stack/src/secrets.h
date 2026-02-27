#ifndef SECRETS_H
#define SECRETS_H

// ==========================================
// ☁️ Firebase の設定
// ==========================================
// ⚠️ ご自身のFirebaseプロジェクトのバケット名に書き換えてください
// 例: "matsuriba-ma.appspot.com"
#define FIREBASE_BUCKET "matsuriba-max.firebasestorage.app"

// ==========================================
// 📡 Bluetooth (BLE) の設定
// ==========================================
// スマホのBluetooth検索画面に表示される名前（好きに変更してOK！）
#define BLE_DEVICE_NAME     "Motteko-Setup"

// --- BLE通信用の合言葉（UUID） ---
// ※アプリ（Flutter）側の設定と同じにする必要があります
#define SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"
#define CHAR_UUID_SSID      "12345678-1234-5678-1234-56789abcdef1"
#define CHAR_UUID_PASS      "12345678-1234-5678-1234-56789abcdef2"

#endif