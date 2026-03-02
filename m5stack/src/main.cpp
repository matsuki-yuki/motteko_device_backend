#include <M5CoreS3.h>
#include <WiFi.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <esp_camera.h>
#include <HTTPClient.h>

#include "secrets.h"

// ==========================================
//  定数定義
// ==========================================
const int WIFI_CONNECT_TIMEOUT = 20;
const int BOOT_RESTART_DELAY = 2000;
const int LOOP_DELAY = 100;
const int TOUCH_RESPONSE_DELAY = 3000;
const int CAMERA_QUALITY = 50;
const char* PREFERENCES_KEY = "motteko";

// ==========================================
//  グローバル変数
// ==========================================
Preferences preferences;
String ssid = "";
String password = "";
bool isConfigMode = false;
String firebaseUrl = "";

// ==========================================
//  関数宣言
// ==========================================
void uploadToFirebase(uint8_t* image_data, size_t image_size);
void startBLESetup();
void setupWiFi();
void setupCamera();
void displayCameraReady();
void handleCameraCapture();
void convertAndUploadImage();


// ==========================================
//  BLE通信コールバック
// ==========================================

class SSIDCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            ssid = String(value.c_str());
            Serial.printf("SSID受信: %s\n", ssid.c_str());
            preferences.putString("ssid", ssid);

            M5.Display.fillScreen(BLACK);
            M5.Display.setCursor(0, 0);
            M5.Display.setTextColor(YELLOW);
            M5.Display.println("SSID Received!");
            M5.Display.println(ssid.c_str());
        }
    }
};

class PassCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            password = String(value.c_str());
            Serial.println("パスワード受信");
            preferences.putString("password", password);

            M5.Display.fillScreen(GREEN);
            M5.Display.setCursor(0, 0);
            M5.Display.setTextColor(BLACK);
            M5.Display.println("Setup Complete!");
            M5.Display.println("Rebooting...");

            delay(BOOT_RESTART_DELAY);
            ESP.restart();
        }
    }
};


// ==========================================
//  BLE設定モード
// ==========================================

void startBLESetup() {
    isConfigMode = true;
    Serial.println("BLE設定モードを起動します");

    M5.Display.fillScreen(BLUE);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(0, 0);
    M5.Display.println("BLE Setup Mode");
    M5.Display.println("Connect via App");

    // デバイスの初期化
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // SSID特性
    BLECharacteristic *pSSIDChar = pService->createCharacteristic(
        CHAR_UUID_SSID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pSSIDChar->setCallbacks(new SSIDCallbacks());

    // パスワード特性
    BLECharacteristic *pPassChar = pService->createCharacteristic(
        CHAR_UUID_PASS,
        BLECharacteristic::PROPERTY_WRITE
    );
    pPassChar->setCallbacks(new PassCallbacks());

    // サービス開始
    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
}


// ==========================================
//　 Wi-Fi接続
// ==========================================

void setupWiFi() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.printf("Connecting to\n%s\n", ssid.c_str());
    Serial.printf("Connecting to %s\n", ssid.c_str());

    WiFi.begin(ssid.c_str(), password.c_str());

    int tryCount = 0;
    while (WiFi.status() != WL_CONNECTED && tryCount < WIFI_CONNECT_TIMEOUT) {
        delay(500);
        M5.Display.print(".");
        Serial.print(".");
        tryCount++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        M5.Display.fillScreen(RED);
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(WHITE);
        M5.Display.println("Wi-Fi Error!");
        M5.Display.println("Rebooting...");
        preferences.clear();
        delay(BOOT_RESTART_DELAY);
        ESP.restart();
    }
}

// ==========================================
//  カメラ初期化
// ==========================================

void setupCamera() {
    M5.Display.println("Init Camera...");
    if (!CoreS3.Camera.begin()) {
        M5.Display.setTextColor(RED);
        M5.Display.println("Camera Error");
        while (1) delay(100);
    }
    CoreS3.Camera.sensor->set_framesize(CoreS3.Camera.sensor, FRAMESIZE_QVGA);
}

// ==========================================
//  Wi-Fi接続成功時の表示
// ==========================================

void displayWiFiConnected() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(GREEN);
    M5.Display.println("Wi-Fi Connected!");

    String macAddress = WiFi.macAddress();
    macAddress.replace(":", "");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("ID: " + macAddress);
    Serial.println("IP Address: " + WiFi.localIP().toString());

    // Firebase URLの組み立て
    firebaseUrl = "https://firebasestorage.googleapis.com/v0/b/" +
                  String(FIREBASE_BUCKET) +
                  "/o?name=inbox%2F" +
                  macAddress +
                  ".jpg";
}

// ==========================================
//  カメラ準備完了表示
// ==========================================

void displayCameraReady() {
    M5.Display.setTextColor(WHITE);
    M5.Display.println("\nReady.");
    M5.Display.println("Touch to Shoot.");
}

// ==========================================
//  起動時のセットアップ
// ==========================================

void setup() {
    M5.begin();
    Serial.begin(115200);
    delay(1000);

    // 起動時のタッチでリセット
    M5.update();
    if (M5.Touch.getCount() > 0) {
        M5.Display.fillScreen(RED);
        M5.Display.setTextColor(WHITE);
        M5.Display.setTextSize(2);
        M5.Display.setCursor(0, 0);
        M5.Display.println("Force Reset!");
        M5.Display.println("Clearing Wi-Fi...");

        preferences.begin(PREFERENCES_KEY, false);
        preferences.clear();
        delay(BOOT_RESTART_DELAY);
        ESP.restart();
    }

    // 起動表示
    M5.Display.setTextSize(2);
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Booting Motteko...");

    preferences.begin(PREFERENCES_KEY, false);
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("password", "");

    // Wi-Fi認証情報がない場合はBLEセットアップ
    if (ssid == "" || password == "") {
        startBLESetup();
    } else {
        setupWiFi();
        displayWiFiConnected();
        setupCamera();
        displayCameraReady();
    }
}

// ==========================================
//  画像キャプチャと変換
// ==========================================

void convertAndUploadImage() {
    M5.Display.println("Converting to JPEG...");
    uint8_t* out_jpg = NULL;
    size_t out_jpg_len = 0;
    bool converted = frame2jpg(CoreS3.Camera.fb, CAMERA_QUALITY, &out_jpg, &out_jpg_len);

    if (converted) {
        M5.Display.println("Uploading...");
        uploadToFirebase(out_jpg, out_jpg_len);
        free(out_jpg);
    } else {
        M5.Display.setTextColor(RED);
        M5.Display.println("JPEG conversion failed.");
    }
}

// ==========================================
//  タッチ処理とカメラキャプチャ
// ==========================================

void handleCameraCapture() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(YELLOW);
    M5.Display.println("Taking picture...");

    if (CoreS3.Camera.get()) {
        convertAndUploadImage();
        CoreS3.Camera.free();
    } else {
        M5.Display.setTextColor(RED);
        M5.Display.println("Camera capture failed!");
    }

    delay(TOUCH_RESPONSE_DELAY);
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(WHITE);
    M5.Display.println("Ready.");
    M5.Display.println("Touch to Shoot.");
}

// ==========================================
//  メインループ
// ==========================================

void loop() {
    M5.update();

    // BLE設定中はタッチ処理をスキップ
    if (isConfigMode) {
        delay(LOOP_DELAY);
        return;
    }

    // タッチスクリーン検出
    if (M5.Touch.getCount() > 0 && M5.Touch.getDetail(0).wasPressed()) {
        handleCameraCapture();
    }

    delay(LOOP_DELAY);
}


// ==========================================
//  Firebase アップロード
// ==========================================

void uploadToFirebase(uint8_t* image_data, size_t image_size) {
    HTTPClient http;
    http.begin(firebaseUrl);

    Serial.println("Upload destination: " + firebaseUrl);
    http.addHeader("Content-Type", "image/jpeg");

    int httpResponseCode = http.POST(image_data, image_size);

    if (httpResponseCode == 200) {
        M5.Display.setTextColor(GREEN);
        M5.Display.printf("Success! Code: %d\n", httpResponseCode);
        Serial.printf("Success! Code: %d\n", httpResponseCode);
    } else {
        M5.Display.setTextColor(RED);
        M5.Display.printf("Error code: %d\n", httpResponseCode);
        Serial.printf("Error code: %d\n", httpResponseCode);
    }

    http.end();
}