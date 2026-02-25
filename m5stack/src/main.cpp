//標準ライブラリ
#include <M5CoreS3.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <HTTPClient.h>

//他ファイル読み込みよう
#include "secrets.h"

void uploadToFirebase(uint8_t* image_data, size_t image_size);

void setup() {
  M5.begin();
  M5.Display.setTextSize(2);
  M5.Display.println("Booting...");

  // --- 1. Wi-Fi接続 ---
  M5.Display.printf("Connecting to %s\n",WIFI_SSID);
  Serial.printf("Connecting to %s\n",WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    M5.Display.print(".");
    Serial.print(".");
  }
  /*
    WL_IDLE_STATUS:接続待機中
    WL_DISCONNECTED:切断
    WL_CONNECTED:接続成功
    WL_CONNECT_FAILED:接続失敗
  */

  M5.Display.println("\nWi-Fi Connected!");
  M5.Display.println("IP Address: ");
  M5.Display.println(WiFi.localIP()); // 割り当てられたIPアドレスを表示
// WiFi.localIP():Wi-Fi接続後にDHCPサーバーから付与された、192.168.x.x などのローカルIPアドレスを取得

  Serial.println("\nWi-Fi Connected!");
  Serial.println(WiFi.localIP());

  // --- 2. Cameraの初期化 ---
  M5.Display.println("Init Camera...");

  if(!CoreS3.Camera.begin()){
    M5.Display.println("Camera Error");
      while(1){
        delay(100);
      }
  }
  CoreS3.Camera.sensor->set_framesize(CoreS3.Camera.sensor, FRAMESIZE_QVGA);

  M5.Display.println("Touch screen to continue.");
  }

void loop() {
  M5.update();
  if(M5.Touch.getCount() > 0 && M5.Touch.getDetail(0).wasPressed()){
    M5.Display.clear();
    M5.Display.setCursor(0,0);
    M5.Display.println("Taking picture...");

    if(CoreS3.Camera.get()){
      M5.Display.println("Converting to JPEG...");
      uint8_t* out_jpg = NULL;
      size_t out_jpg_len = 0;
      bool converted = frame2jpg(CoreS3.Camera.fb,50,&out_jpg, &out_jpg_len);
      if(converted){
        M5.Display.println("Uploading...");
        uploadToFirebase(out_jpg, out_jpg_len);
        free(out_jpg);
      }else{
        M5.Display.println("JPEG conversion failed.");
      }
      CoreS3.Camera.free(); 
    } else {
      M5.Display.println("Camera capture failed!");
    }

  }
  delay(100);
}

void uploadToFirebase(uint8_t* image_data, size_t image_size){
  String url = "https://firebasestorage.googleapis.com/v0/b/" + bucketName + "/o?name=" + fileName;
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "image/jpeg");

  int httpResponseCode = http.POST(image_data, image_size);

  if(httpResponseCode == 200){
    M5.Display.printf("Success! Code: %d\n",httpResponseCode);
  } else{
    M5.Display.printf("Error code: %d\n",httpResponseCode);
  }
  http.end();
}