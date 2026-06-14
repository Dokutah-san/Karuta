#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "VideoFrame.h" // Memanggil data 39 frame animasi milikmu

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ====================================================================
// CONFIG PROFILE WIFI
// ====================================================================
const char* ssid_rumah     = "Lantai_3";
const char* password_rumah = "1253920464828";

const char* ssid_portable  = "DokutahSan";     // <--- Ganti nama Wi-Fi Portable / Hotspot HP
const char* password_portable = "rf23042005"; // <--- Ganti password Wi-Fi Portable / Hotspot HP
// ====================================================================

ESP8266WebServer server(80);

const int LED_PIN = LED_BUILTIN; 

// SEPARASI PIN FISIK
const int BUTTON_PIN = D5;      // D5 murni untuk tukar-tukar tampilan (3 Halaman)
const int WIFI_SWITCH_PIN = D6; // D6 murni untuk switch pemilihan Wi-Fi saat booting

int displayMode = 0;   // 0 = Tracker, 1 = Info WiFi & IP, 2 = Animasi Video
int currentFrame = 0; 

// Menyimpan nama wifi yang sedang aktif untuk ditampilkan di layar menu 1
String currentActiveSSID = "";

long dailyCooldownEnd = 0;
long voteCooldownEnd = 0;
long dropCooldownEnd = 0;
long grabCooldownEnd = 0;

unsigned long lastBlinkMillis = 0;
const long blinkInterval = 500; 
bool ledState = HIGH; 

void handleRoot() {
  server.send(200, "text/plain", "ESP8266 Karuta Tracker Ready");
}

// ====================================================================
// FUNGSI KOMUNIKASI DATA DISCORD
// ====================================================================
void handleSyncKrm() {
  long currentMillis = millis();
  if (server.hasArg("daily")) {
    long minLeft = server.arg("daily").toInt();
    dailyCooldownEnd = (minLeft == 0) ? 0 : currentMillis + (minLeft * 60000);
  }
  if (server.hasArg("vote")) {
    long minLeft = server.arg("vote").toInt();
    voteCooldownEnd = (minLeft == 0) ? 0 : currentMillis + (minLeft * 60000);
  }
  if (server.hasArg("drop")) {
    long minLeft = server.arg("drop").toInt();
    dropCooldownEnd = (minLeft == 0) ? 0 : currentMillis + (minLeft * 60000);
  }
  if (server.hasArg("grab")) {
    long minLeft = server.arg("grab").toInt();
    grabCooldownEnd = (minLeft == 0) ? 0 : currentMillis + (minLeft * 60000);
  }
  server.send(200, "text/plain", "Sync Success");
}

void handleDailyCooldown() { dailyCooldownEnd = millis() + 86400000; server.send(200, "text/plain", "Daily Started"); }
void handleVoteCooldown()  { voteCooldownEnd = millis() + 43200000;  server.send(200, "text/plain", "Vote Started"); }
void handleDropCooldown()  { dropCooldownEnd = millis() + 1800000;   server.send(200, "text/plain", "Drop Started"); }
void handleGrabCooldown()  { grabCooldownEnd = millis() + 600000;    server.send(200, "text/plain", "Grab Started"); }

void formatAndPrintTime(long cooldownEnd, long currentMillis, int x, int y) {
  display.setTextSize(1);
  display.setCursor(x, y);
  if (cooldownEnd > currentMillis) {
    long remainingSec = (cooldownEnd - currentMillis) / 1000;
    long hours = remainingSec / 3600;
    
    if (hours >= 1) {
      long minutes = (remainingSec % 3600) / 60;
      if(hours < 10) display.print("0");
      display.print(hours);
      display.print("h ");
      if(minutes < 10) display.print("0");
      display.print(minutes);
      display.print("m");
    } else {
      long minutes = remainingSec / 60;
      long seconds = remainingSec % 60;
      if(minutes < 10) display.print("0");
      display.print(minutes);
      display.print(":");
      if(seconds < 10) display.print("0");
      display.print(seconds);
    }
  } else {
    display.print("READY");
  }
}

void checkLedStatus(long currentMillis) {
  bool dailyReady = (dailyCooldownEnd <= currentMillis);
  bool voteReady  = (voteCooldownEnd <= currentMillis);
  bool dropReady  = (dropCooldownEnd <= currentMillis);
  bool grabReady  = (grabCooldownEnd <= currentMillis);

  if (dropReady && !grabReady && !dailyReady && !voteReady) {
    ledState = HIGH;
    digitalWrite(LED_PIN, ledState);
  }
  else if (dailyReady || voteReady || dropReady || grabReady) {
    if (currentMillis - lastBlinkMillis >= blinkInterval) {
      lastBlinkMillis = currentMillis;
      ledState = !ledState; 
      digitalWrite(LED_PIN, ledState);
    }
  } 
  else {
    ledState = HIGH;
    digitalWrite(LED_PIN, ledState); 
  }
}

void drawAnimation() {
  display.drawBitmap(0, 0, video_frames[currentFrame], SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  currentFrame++;
  if (currentFrame >= TOTAL_FRAMES) {
    currentFrame = 0; 
  }
}

// HALAMAN BARU: MENAMPILKAN STATUS JARINGAN AKTIF
void displayNetworkInfo() {
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  
  display.setCursor(0, 2);
  display.println("--- NETWORK INFO ---");
  
  display.setCursor(0, 20);
  display.print("SSID :");
  display.setCursor(42, 20);
  display.println(currentActiveSSID);

  display.setCursor(0, 36);
  display.print("IP :");
  display.setCursor(30, 36);
  display.println(WiFi.localIP());

  display.setCursor(0, 54);
  display.print("RSSI :");
  display.setCursor(42, 54);
  display.print(WiFi.RSSI());
  display.println(" dBm");
}

void updateDisplay() {
  long currentMillis = millis();
  display.clearDisplay();

  if (displayMode == 0) {
    // HALAMAN 0: TRACKER COOLDOWN
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 2);  display.print("Daily : ");  formatAndPrintTime(dailyCooldownEnd, currentMillis, 54, 2);
    display.setCursor(0, 18); display.print("Vote  : ");  formatAndPrintTime(voteCooldownEnd, currentMillis, 54, 18);
    display.setCursor(0, 34); display.print("Drop  : ");  formatAndPrintTime(dropCooldownEnd, currentMillis, 54, 34);
    display.setCursor(0, 50); display.print("Grab  : ");  formatAndPrintTime(grabCooldownEnd, currentMillis, 54, 50);
  } 
  else if (displayMode == 1) {
    // HALAMAN 1: INFO WIFI & IP
    displayNetworkInfo();
  }
  else {
    // HALAMAN 2: ANIMASI VIDEO
    drawAnimation();
  }
  display.display(); 
}

void readButton() {
  int reading = digitalRead(BUTTON_PIN);
  static bool buttonLock = false;

  if (reading == LOW) {
    if (!buttonLock) { 
      // Mengubah mode halaman (0 -> 1 -> 2 -> kembali ke 0)
      displayMode++;
      if (displayMode > 2) {
        displayMode = 0;
      }
      updateDisplay(); 
      buttonLock = true; 
    }
  } else {
    buttonLock = false; 
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); 
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(WIFI_SWITCH_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // ------------------------------------------------------------------
  // EKSEKUSI CEK SWITCH WIFI (D6) SEBELUM CONNECTING
  // ------------------------------------------------------------------
  delay(300); 
  int wifiSelection = digitalRead(WIFI_SWITCH_PIN);

  const char* selectedSSID;
  const char* selectedPASS;

  if (wifiSelection == LOW) {
    selectedSSID = ssid_portable;
    selectedPASS = password_portable;
    currentActiveSSID = String(ssid_portable); // Simpan nama wifi ke variabel global
    display.setCursor(0, 2);  display.println("[MODE: PORTABLE]");
  } else {
    selectedSSID = ssid_rumah;
    selectedPASS = password_rumah;
    currentActiveSSID = String(ssid_rumah);    // Simpan nama wifi ke variabel global
    display.setCursor(0, 2);  display.println("[MODE: WIFI RUMAH]");
  }

  display.setCursor(0, 18); display.println("Connecting to:");
  display.setCursor(0, 34); display.println(selectedSSID);
  display.display();

  WiFi.begin(selectedSSID, selectedPASS);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print(".");
  }

  // NOTIFIKASI BOOTING SUKSES (Dipersingkat jadi 1.5 detik saja agar cepat masuk)
  display.clearDisplay();
  display.setCursor(0, 15); display.println("WiFi Connected!");
  display.setCursor(0, 35); display.print("IP: "); display.println(WiFi.localIP());
  display.display();
  delay(1500); 

  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/sync_krm", handleSyncKrm);
  server.on("/daily_cooldown", handleDailyCooldown);
  server.on("/vote_cooldown", handleVoteCooldown);
  server.on("/drop_cooldown", handleDropCooldown);
  server.on("/grab_cooldown", handleGrabCooldown);
  
  server.begin();
}

void loop() {
  server.handleClient();
  unsigned long currentMillis = millis();
  
  readButton(); // Memantau D5 untuk tukar-tukar 3 halaman tampilan
  checkLedStatus(currentMillis);
  
  static unsigned long lastUpdate = 0;
  if (currentMillis - lastUpdate >= 100) {  
    lastUpdate = currentMillis;
    updateDisplay();
  }
}