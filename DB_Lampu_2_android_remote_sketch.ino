#include <WebServer.h> // Tambahkan library ini
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h> // Library untuk NTP

// Konfigurasi Wi-Fi dan XAMPP
const char* ssid = "GLB3--WIFI Internal-AREA IT01";
const char* password = "bgpwifi2023sa";
const char* xampp_host = "192.168.20.179"; // Ganti dengan IP Address komputer XAMPP Anda
const char* update_api_path = "/lampu_api/update_ip.php";
const char* log_api_path = "/lampu_api/log_api.php";

// Konfigurasi NTP
const char* ntpServer = "id.pool.ntp.org";
const long  gmtOffset_sec = 25200; // GMT+7 untuk WIB (7 * 3600)
const int   daylightOffset_sec = 0;

const int relay1Pin = 4;
const int relay2Pin = 5;

const String DEVICE_NAME = "ESP32-101";

// Variabel global
bool lamp1State = LOW;
bool lamp2State = LOW;
unsigned long lamp1OnTime = 0;
unsigned long lamp2OnTime = 0;

WebServer server(80);

// Deklarasi fungsi
void sendLog(String status, String sumber, unsigned long onTime, unsigned long offTime);
void updateIpAddress();
String getFormattedTime(unsigned long millisTime);
void handleCommand();
void initTime();

void setup() {
  Serial.begin(115200);
  pinMode(relay1Pin, OUTPUT);
  pinMode(relay2Pin, OUTPUT);
  digitalWrite(relay1Pin, LOW);
  digitalWrite(relay2Pin, LOW);

  // Koneksi Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Inisialisasi NTP
  initTime();

  // Kirim IP Address ke database saat boot
  updateIpAddress();

  // Inisialisasi server web
  server.on("/command", HTTP_POST, handleCommand);
  server.begin();
  Serial.println("Server HTTP dimulai.");
}

void loop() {
  server.handleClient();

  // --- Logika untuk Lampu 1 ---
  bool newLamp1State = (digitalRead(relay1Pin) == HIGH);
  if (newLamp1State != lamp1State) {
    lamp1State = newLamp1State;
    if (lamp1State == HIGH) {
      lamp1OnTime = millis();
    } else {
      unsigned long offTime = millis();
      sendLog("OFF", "Android", lamp1OnTime, offTime);
    }
  }
  
  // --- Logika untuk Lampu 2 ---
  bool newLamp2State = (digitalRead(relay2Pin) == HIGH);
  if (newLamp2State != lamp2State) {
    lamp2State = newLamp2State;
    if (lamp2State == HIGH) {
      lamp2OnTime = millis();
    } else {
      unsigned long offTime = millis();
      sendLog("OFF", "Android", lamp2OnTime, offTime);
    }
  }
}

// --- Definisi fungsi ---

// Fungsi baru untuk inisialisasi NTP
void initTime() {
  // Mengatur server NTP dan offset zona waktu
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Gagal mendapatkan waktu dari NTP. Menggunakan millis().");
  } else {
    Serial.println("Waktu NTP berhasil disinkronkan.");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  }
}

void sendLog(String status, String sumber, unsigned long onTime, unsigned long offTime) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://" + String(xampp_host) + String(log_api_path);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    // Dapatkan waktu yang akurat dari NTP saat log dikirim
    struct tm timeinfoOn, timeinfoOff;
    time_t onTimeNTP = (onTime / 1000) + (time(nullptr) - (millis() / 1000));
    time_t offTimeNTP = (offTime / 1000) + (time(nullptr) - (millis() / 1000));
    
    localtime_r(&onTimeNTP, &timeinfoOn);
    localtime_r(&offTimeNTP, &timeinfoOff);

    char waktuNyala[25];
    char waktuPadam[25];
    strftime(waktuNyala, sizeof(waktuNyala), "%Y-%m-%d %H:%M:%S", &timeinfoOn);
    strftime(waktuPadam, sizeof(waktuPadam), "%Y-%m-%d %H:%M:%S", &timeinfoOff);

    String jsonPayload = "{\"status_lampu\":\"" + status + "\","
                       + "\"sumber\":\"" + sumber + "\","
                       + "\"waktu_menyala\":\"" + String(waktuNyala) + "\","
                       + "\"waktu_padam\":\"" + String(waktuPadam) + "\","
                       + "\"lama_menyala_detik\":" + (offTime - onTime) / 1000 + "}";

    Serial.println("Mengirim log: " + jsonPayload);
    int httpCode = http.POST(jsonPayload);
    
    if (httpCode > 0) {
      Serial.printf("Kode HTTP: %d\n", httpCode);
      String payload = http.getString();
      Serial.println(payload);
    } else {
      Serial.printf("Gagal mengirim log. Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

void updateIpAddress() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://" + String(xampp_host) + String(update_api_path);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{\"device_name\":\"" + DEVICE_NAME + "\","
                       + "\"ip_address\":\"" + WiFi.localIP().toString() + "\"}";
                       
    Serial.println("Mengirim IP: " + jsonPayload);
    int httpCode = http.POST(jsonPayload);
    
    if (httpCode > 0) {
      Serial.printf("Kode HTTP: %d\n", httpCode);
      String payload = http.getString();
      Serial.println(payload);
    } else {
      Serial.printf("Gagal mengirim IP. Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

void handleCommand() {
  String command = server.arg("plain");
  Serial.print("Menerima perintah: ");
  Serial.println(command);

  String responseMessage = "Perintah tidak valid";

  if (command == "LAMP1_ON") {
    digitalWrite(relay1Pin, HIGH);
    responseMessage = "Lampu 1 ON";
  } else if (command == "LAMP1_OFF") {
    digitalWrite(relay1Pin, LOW);
    responseMessage = "Lampu 1 OFF";
  } else if (command == "LAMP2_ON") {
    digitalWrite(relay2Pin, HIGH);
    responseMessage = "Lampu 2 ON";
  } else if (command == "LAMP2_OFF") {
    digitalWrite(relay2Pin, LOW);
    responseMessage = "Lampu 2 OFF";
  }
  
  server.send(200, "text/plain", responseMessage);
}