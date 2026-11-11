#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>

// ESP32-WROOM-32 + 128x64 SSD1306 OLED + analog joystick
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define JOY_X 34
#define JOY_Y 35
#define JOY_SW 32

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

enum Screen { HOME, MENU, WIFI_SCAN, GAME };
Screen screen = HOME;
const char* menuItems[] = {"Passive Wi-Fi scan", "Pong mini-game", "Back"};
const int MENU_COUNT = 3;
int menuIndex = 0;
unsigned long lastInput = 0;
bool previousButton = false;

bool buttonPressed() {
  bool now = digitalRead(JOY_SW) == LOW;
  bool pressed = now && !previousButton;
  previousButton = now;
  return pressed;
}

int joyY() {
  int v = analogRead(JOY_Y);
  if (v < 1300) return -1;
  if (v > 2800) return 1;
  return 0;
}

void header(const char* title) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(title);
  display.drawFastHLine(0, 11, 128, SSD1306_WHITE);
}

void drawHome() {
  header("FLOPPER ZERO - WROOM");
  display.setCursor(0, 19);
  display.println("Safe firmware");
  display.println("OLED + joystick");
  display.println("Passive Wi-Fi scan");
  display.println("Pong mini-game");
  display.println();
  display.println("Press joystick to open");
  display.display();
}

void drawMenu() {
  header("MAIN MENU");
  for (int i = 0; i < MENU_COUNT; ++i) {
    display.setCursor(3, 17 + i * 14);
    display.print(i == menuIndex ? "> " : "  ");
    display.println(menuItems[i]);
  }
  display.display();
}

void scanNetworks() {
  header("PASSIVE WI-FI SCAN");
  display.setCursor(0, 18);
  display.println("Scanning nearby SSIDs...");
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  int count = WiFi.scanNetworks(false, true);
  header("SCAN RESULTS");
  if (count <= 0) {
    display.println("No networks found");
  } else {
    int shown = min(count, 4);
    for (int i = 0; i < shown; ++i) {
      display.setCursor(0, 14 + i * 12);
      String name = WiFi.SSID(i);
      if (name.length() > 15) name = name.substring(0, 15);
      display.printf("%s %d dBm", name.c_str(), WiFi.RSSI(i));
    }
    display.setCursor(0, 56);
    display.printf("%d networks total", count);
  }
  display.display();
  WiFi.scanDelete();
  delay(2500);
  screen = MENU;
}

void runPong() {
  int px = 4, py = 26, bx = 62, by = 30, dx = 2, dy = 1;
  int cpuY = 25;
  unsigned long start = millis();
  while (millis() - start < 30000) {
    if (buttonPressed()) break;
    int y = analogRead(JOY_Y);
    py = constrain(map(y, 0, 4095, 12, 48), 12, 48);
    bx += dx; by += dy;
    if (by <= 13 || by >= 60) dy = -dy;
    if (bx >= 120) dx = -dx;
    if (bx <= 9 && abs(by - (py + 6)) < 10) dx = -dx;
    if (bx < 0) { bx = 64; by = 30; dx = 2; }
    cpuY += (by > cpuY + 6) ? 1 : (by < cpuY + 6 ? -1 : 0);
    cpuY = constrain(cpuY, 12, 48);

    header("PONG - press to exit");
    display.fillRect(px, py, 3, 12, SSD1306_WHITE);
    display.fillRect(121, cpuY, 3, 12, SSD1306_WHITE);
    display.fillCircle(bx, by, 2, SSD1306_WHITE);
    display.display();
    delay(35);
  }
  screen = MENU;
}

void setup() {
  Serial.begin(115200);
  pinMode(JOY_SW, INPUT_PULLUP);
  analogReadResolution(12);
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true) delay(100);
  }
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  drawHome();
}

void loop() {
  if (screen == HOME) {
    if (buttonPressed()) { screen = MENU; drawMenu(); }
    delay(20);
    return;
  }

  if (screen == MENU) {
    if (millis() - lastInput > 180) {
      int direction = joyY();
      if (direction != 0) {
        menuIndex = (menuIndex + direction + MENU_COUNT) % MENU_COUNT;
        lastInput = millis();
        drawMenu();
      } else if (buttonPressed()) {
        lastInput = millis();
        if (menuIndex == 0) screen = WIFI_SCAN;
        else if (menuIndex == 1) screen = GAME;
        else screen = HOME;
      }
    }
    if (screen == WIFI_SCAN) scanNetworks();
    else if (screen == GAME) runPong();
    else if (screen == HOME) drawHome();
  }
  delay(10);
}
