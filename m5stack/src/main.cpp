#include <M5CoreS3.h>
#include <WiFi.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

// メモリ保存用のオブジェクト
Preferences preferences;

// Wi-Fi情報の変数
String ssid = "";
String password = "";

// --- BLEのUUID設定（フロントチームと合わせた合言葉） ---
#define SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"
#define CHAR_UUID_SSID      "12345678-1234-5678-1234-56789abcdef1"
#define CHAR_UUID_PASS      "12345678-1234-5678-1234-56789abcdef2"

// --- アプリからSSIDが書き込まれた時に呼ばれる処理 ---
class SSIDCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            ssid = String(value.c_str());
            Serial.printf("📱 スマホからSSIDを受信: %s\n", ssid.c_str());
            // メモリに保存
            preferences.putString("ssid", ssid);
        }
    }
};

// --- アプリからパスワードが書き込まれた時に呼ばれる処理 ---
class PassCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            password = String(value.c_str());
            Serial.println("📱 スマホからパスワードを受信しました（セキュリティのため非表示）");
            // メモリに保存
            preferences.putString("password", password);
            
            Serial.println("✅ Wi-Fi情報の設定が完了しました！再起動して接続を試みます...");
            delay(2000);
            ESP.restart(); // デバイスを再起動してWi-Fi接続へ移行
        }
    }
};

// --- BLEサーバーを立ち上げる関数 ---
void startBLESetup() {
    Serial.println("🔵 Wi-Fi情報がありません。BLE設定モードを起動します...");
    
    // デバイス名を設定（スマホのBluetooth画面に表示される名前）
    BLEDevice::init("Motteko-Setup");
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // SSID用ポストの作成
    BLECharacteristic *pSSIDChar = pService->createCharacteristic(
        CHAR_UUID_SSID, BLECharacteristic::PROPERTY_WRITE
    );
    pSSIDChar->setCallbacks(new SSIDCallbacks());

    // パスワード用ポストの作成
    BLECharacteristic *pPassChar = pService->createCharacteristic(
        CHAR_UUID_PASS, BLECharacteristic::PROPERTY_WRITE
    );
    pPassChar->setCallbacks(new PassCallbacks());

    // 電波の発信スタート
    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
    
    Serial.println("📡 BLE発信中... スマホアプリからの設定を待っています");
}

void setup() {
    M5.begin();
    Serial.begin(115200);
    delay(1000);

    // "motteko" という名前のメモリ空間を開く（false = 読み書きモード）
    preferences.begin("motteko", false);
    
    // メモリからWi-Fi情報を読み出す（保存されていなければ空文字 "" になる）
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("password", "");

    // 情報がない場合はBLE設定モード、ある場合はWi-Fi接続
    if (ssid == "" || password == "") {
        startBLESetup();
    } else {
        Serial.printf("🟢 保存されたWi-Fi (%s) に接続します...\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), password.c_str());
        
        int tryCount = 0;
        while (WiFi.status() != WL_CONNECTED && tryCount < 20) {
            delay(500);
            Serial.print(".");
            tryCount++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n🌐 Wi-Fi接続成功！");
            Serial.print("IPアドレス: ");
            Serial.println(WiFi.localIP());
            // 💡 ここからいつもの「カメラ撮影・送信」の処理に繋げます！
        } else {
            Serial.println("\n❌ Wi-Fi接続失敗。パスワードが間違っているか、電波が届いていません。");
            // 設定を初期化して再起動（再度BLEモードに入らせる）
            preferences.clear();
            ESP.restart();
        }
    }
}

void loop() {
    // BLEの設定待ちはバックグラウンドで動くので、loopは空でOKです
    delay(1000);
}