#include <M5CoreS3.h>
#include <WiFi.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <esp_camera.h>
#include <HTTPClient.h>

#include "secrets.h" 

// --- グローバル変数 ---
Preferences preferences;
String ssid = "";
String password = "";
bool isConfigMode = false;
String firebaseUrl = ""; // Wi-Fi接続後に組み立てる動的URL

// --- 関数宣言 ---
void uploadToFirebase(uint8_t* image_data, size_t image_size);
void startBLESetup();

// ==========================================
// 📡 BLE通信のコールバック
// ==========================================
class SSIDCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            ssid = String(value.c_str());
            Serial.printf("📱 スマホからSSIDを受信: %s\n", ssid.c_str());
            preferences.putString("ssid", ssid);
            
            M5.Display.fillScreen(BLACK);
            M5.Display.setCursor(0, 0);
            M5.Display.setTextColor(YELLOW);
            M5.Display.println("SSID Received!");
            M5.Display.println(ssid.c_str());
        }
    }
};

class PassCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            password = String(value.c_str());
            Serial.println("📱 スマホからパスワードを受信");
            preferences.putString("password", password);
            
            M5.Display.fillScreen(GREEN);
            M5.Display.setCursor(0, 0);
            M5.Display.setTextColor(BLACK);
            M5.Display.println("Setup Complete!");
            M5.Display.println("Rebooting...");
            
            delay(2000);
            ESP.restart();
        }
    }
};

void startBLESetup() {
    isConfigMode = true;
    Serial.println("🔵 BLE設定モードを起動します...");
    
    M5.Display.fillScreen(BLUE);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(0, 0);
    M5.Display.println("BLE Setup Mode");
    M5.Display.println("Connect via App");
    
    // 👇 secrets.h で定義したデバイス名を読み込んで使用！
    BLEDevice::init(BLE_DEVICE_NAME);
    
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);

    BLECharacteristic *pSSIDChar = pService->createCharacteristic(CHAR_UUID_SSID, BLECharacteristic::PROPERTY_WRITE);
    pSSIDChar->setCallbacks(new SSIDCallbacks());

    BLECharacteristic *pPassChar = pService->createCharacteristic(CHAR_UUID_PASS, BLECharacteristic::PROPERTY_WRITE);
    pPassChar->setCallbacks(new PassCallbacks());

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
}

// ==========================================
// 🚀 起動時のセットアップ
// ==========================================
void setup() {
    M5.begin();
    Serial.begin(115200);
    delay(1000);

    M5.Display.setTextSize(2);
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Booting Motteko...");

    preferences.begin("motteko", false);
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("password", "");

    if (ssid == "" || password == "") {
        startBLESetup();
    } else {
        M5.Display.fillScreen(BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.printf("Connecting to\n%s\n", ssid.c_str());
        Serial.printf("Connecting to %s\n", ssid.c_str());

        WiFi.begin(ssid.c_str(), password.c_str());
        
        int tryCount = 0;
        while (WiFi.status() != WL_CONNECTED && tryCount < 20) {
            delay(500);
            M5.Display.print(".");
            Serial.print(".");
            tryCount++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            M5.Display.fillScreen(BLACK);
            M5.Display.setCursor(0, 0);
            M5.Display.setTextColor(GREEN);
            M5.Display.println("Wi-Fi Connected!");
            
            // --- 💡 動的URLの生成 ---
            String macAddress = WiFi.macAddress();
            macAddress.replace(":", "");
            M5.Display.setTextColor(WHITE);
            M5.Display.println("ID: " + macAddress);
            Serial.println("\nIP Address: " + WiFi.localIP().toString());
            
            // 👇 secrets.h で定義したバケット名を読み込んでURLを組み立てる！
            firebaseUrl = "https://firebasestorage.googleapis.com/v0/b/" + String(FIREBASE_BUCKET) + "/o?name=inbox%2F" + macAddress + ".jpg";

            // --- 📷 カメラの初期化 ---
            M5.Display.println("Init Camera...");
            if(!CoreS3.Camera.begin()){
                M5.Display.setTextColor(RED);
                M5.Display.println("Camera Error");
                while(1) delay(100);
            }
            CoreS3.Camera.sensor->set_framesize(CoreS3.Camera.sensor, FRAMESIZE_QVGA);

            M5.Display.setTextColor(WHITE);
            M5.Display.println("\nReady.");
            M5.Display.println("Touch to Shoot.");
            
        } else {
            M5.Display.fillScreen(RED);
            M5.Display.setCursor(0, 0);
            M5.Display.setTextColor(WHITE);
            M5.Display.println("Wi-Fi Error!");
            M5.Display.println("Rebooting...");
            preferences.clear();
            delay(2000);
            ESP.restart();
        }
    }
}

// ==========================================
// 🔄 メインループ
// ==========================================
void loop() {
    M5.update();
    
    // BLE設定中はタッチ処理をスキップ
    if (isConfigMode) {
        delay(100);
        return; 
    }

    if(M5.Touch.getCount() > 0 && M5.Touch.getDetail(0).wasPressed()){
        M5.Display.fillScreen(BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(YELLOW);
        M5.Display.println("Taking picture...");

        if(CoreS3.Camera.get()){
            M5.Display.println("Converting to JPEG...");
            uint8_t* out_jpg = NULL;
            size_t out_jpg_len = 0;
            bool converted = frame2jpg(CoreS3.Camera.fb, 50, &out_jpg, &out_jpg_len);
            
            if(converted){
                M5.Display.println("Uploading...");
                uploadToFirebase(out_jpg, out_jpg_len);
                free(out_jpg);
            } else {
                M5.Display.setTextColor(RED);
                M5.Display.println("JPEG conversion failed.");
            }
            CoreS3.Camera.free(); 
        } else {
            M5.Display.setTextColor(RED);
            M5.Display.println("Camera capture failed!");
        }
        
        delay(3000); 
        M5.Display.fillScreen(BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(WHITE);
        M5.Display.println("Ready.");
        M5.Display.println("Touch to Shoot.");
    }
    delay(100);
}

// ==========================================
// 📤 Firebaseアップロード関数
// ==========================================
void uploadToFirebase(uint8_t* image_data, size_t image_size){
    HTTPClient http;
    http.begin(firebaseUrl);

    Serial.println("🔗 実際の送信先: " + firebaseUrl);
    
    http.addHeader("Content-Type", "image/jpeg");

    int httpResponseCode = http.POST(image_data, image_size);

    if(httpResponseCode == 200){
        M5.Display.setTextColor(GREEN);
        M5.Display.printf("Success! Code: %d\n", httpResponseCode);
        Serial.printf("✅ Success! Code: %d\n", httpResponseCode);
    } else {
        M5.Display.setTextColor(RED);
        M5.Display.printf("Error code: %d\n", httpResponseCode);
        Serial.printf("❌ Error code: %d\n", httpResponseCode);
    }
    http.end();
}