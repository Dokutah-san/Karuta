#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid = "Lantai_3";
const char* password = "56478900456283";

ESP8266WebServer server(80);

// Pin Lampu Bawaan ESP8266
const int LED_PIN = LED_BUILTIN; 

long dailyCooldownEnd = 0;
long voteCooldownEnd = 0;
long dropCooldownEnd = 0;
long grabCooldownEnd = 0;

// Variabel pendukung untuk logika kelap-kelip tanpa delay
unsigned long lastBlinkMillis = 0;
const long blinkInterval = 500; // Kecepatan kedip (500ms nyala, 500ms mati)
bool ledState = HIGH;           // HIGH = Mati (default awal)

void handleRoot() {
  server.send(200, "text/plain", "ESP8266 Karuta Tracker Ready");
}

// Sinkronisasi dari perintah "krm"
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

// Trigger manual individu
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

// Fungsi untuk mengecek status READY dan mengontrol kelap-kelip lampu
void checkLedStatus(long currentMillis) {
  // Cek apakah status masing-masing data sudah bernilai READY
  bool dailyReady = (dailyCooldownEnd <= currentMillis);
  bool voteReady  = (voteCooldownEnd <= currentMillis);
  bool dropReady  = (dropCooldownEnd <= currentMillis);
  bool grabReady  = (grabCooldownEnd <= currentMillis);

  // ------------------ LOGIKA KOMBO KHUSUS KAMU ------------------
  // JIKA (Drop sedang READY dan Grab BELUM Ready) DAN Daily/Vote juga tidak sedang READY
  if (dropReady && !grabReady && !dailyReady && !voteReady) {
    // Khusus kombo ini aja, lampu dipaksa untuk tetep MATI (HIGH)
    ledState = HIGH;
    digitalWrite(LED_PIN, ledState);
  }
  // JIKA ada data lain yang READY (di luar kombo pengecualian di atas)
  else if (dailyReady || voteReady || dropReady || grabReady) {
    // Jalankan logika kelap-kelip tanpa delay
    if (currentMillis - lastBlinkMillis >= blinkInterval) {
      lastBlinkMillis = currentMillis; 
      ledState = !ledState; 
      digitalWrite(LED_PIN, ledState);
    }
  } 
  else {
    // Jika semua sedang cooldown (tidak ada READY sama sekali) -> Lampu MATI
    ledState = HIGH;
    digitalWrite(LED_PIN, ledState); 
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  long currentMillis = millis();

  display.setCursor(0, 2);  display.print("Daily : "); formatAndPrintTime(dailyCooldownEnd, currentMillis, 54, 2);
  display.setCursor(0, 18); display.print("Vote  : "); formatAndPrintTime(voteCooldownEnd, currentMillis, 54, 18);
  display.setCursor(0, 34); display.print("Drop  : "); formatAndPrintTime(dropCooldownEnd, currentMillis, 54, 34);
  display.setCursor(0, 50); display.print("Grab  : "); formatAndPrintTime(grabCooldownEnd, currentMillis, 54, 50);

  display.display();
  
  // Jalankan logika pengecekan status lampu bawaan
  checkLedStatus(currentMillis);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Default awal mati saat booting

  Serial.begin(115200);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20); display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

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
  
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 100) {  
    lastUpdate = millis();
    updateDisplay();
  }
}