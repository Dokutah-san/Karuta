#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "VideoFrame.h" // MEMANGGIL DATA ANIMASI 39 FRAME MILIKMU

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid = "";
const char* password = "";

ESP8266WebServer server(80);

const int LED_PIN = LED_BUILTIN;

// MENGGUNAKAN PIN D7 SESUAI PENGUJIAN TERAKHIR
const int BUTTON_PIN = D5; 

int displayMode = 0; 
int currentFrame = 0; // Mengatur frame animasi dari VideoFrame.h

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
// FUNGSI UTAMA DISCORD (TETAP AMAN, UTUH, & TIDAK BERUBAH)
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

// Fungsi Menggambar Animasi mengambil bitmap asli dari VideoFrame.h milikmu
void drawAnimation() {
  // Menggambar frame aktif dari PROGMEM
  display.drawBitmap(0, 0, video_frames[currentFrame], SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  
  // Berpindah ke frame berikutnya
  currentFrame++;
  if (currentFrame >= TOTAL_FRAMES) {
    currentFrame = 0; // Reset ke frame awal jika sudah mencapai 39 frame
  }
}

void updateDisplay() {
  long currentMillis = millis();
  display.clearDisplay();

  if (displayMode == 0) {
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 2);  display.print("Daily : ");  formatAndPrintTime(dailyCooldownEnd, currentMillis, 54, 2);
    display.setCursor(0, 18); display.print("Vote  : ");  formatAndPrintTime(voteCooldownEnd, currentMillis, 54, 18);
    display.setCursor(0, 34); display.print("Drop  : ");  formatAndPrintTime(dropCooldownEnd, currentMillis, 54, 34);
    display.setCursor(0, 50); display.print("Grab  : ");  formatAndPrintTime(grabCooldownEnd, currentMillis, 54, 50);
  } 
  else {
    drawAnimation();
  }
  
  display.display(); 
}

// Fungsi Baca Tombol Tanpa Polling Waktu (Langsung Eksekusi)
void readButton() {
  int reading = digitalRead(BUTTON_PIN);
  static bool buttonLock = false;

  if (reading == LOW) {
    if (!buttonLock) { 
      displayMode = (displayMode == 0) ? 1 : 0; 
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
  
  unsigned long currentMillis = millis();
  
  readButton();
  checkLedStatus(currentMillis);
  
  // Pengaturan kecepatan update (100ms sesuai target FRAME_DELAY di VideoFrame.h)
  static unsigned long lastUpdate = 0;
  if (currentMillis - lastUpdate >= 100) { 
    lastUpdate = currentMillis;
    updateDisplay();
  }
}