#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define JOY_X 34
#define JOY_Y 35
#define JOY_SW 32
#define MAX_NETWORKS 12

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

enum AppMode { MENU, WIFI_RESULTS, SNAKE_GAME, ABOUT };
AppMode mode = MENU;

const char* menuItems[] = {"Passive Wi-Fi scan", "Snake mini-game", "About"};
const int MENU_COUNT = 3;
int menuIndex = 0;
int networkCount = 0;
String networkNames[MAX_NETWORKS];
int networkSignals[MAX_NETWORKS];
int networkOffset = 0;
int snakeX[40], snakeY[40], snakeLength = 3;
int foodX = 64, foodY = 32, dirX = 1, dirY = 0;
unsigned long lastInput = 0;
unsigned long lastFrame = 0;
unsigned long lastSnakeStep = 0;
bool gameOver = false;

const int DEADZONE = 650;
const unsigned long DEBOUNCE_MS = 180;

bool pressed() { return digitalRead(JOY_SW) == LOW; }

int joyX() { return analogRead(JOY_X); }
int joyY() { return analogRead(JOY_Y); }

void drawTitle(const char* title) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(title);
  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);
}

void drawMenu() {
  drawTitle("FLOPPER ZERO - SAFE");
  for (int i = 0; i < MENU_COUNT; i++) {
    int y = 17 + i * 14;
    if (i == menuIndex) {
      display.fillRect(0, y - 2, 128, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(5, y);
    display.print(i == menuIndex ? "> " : "  ");
    display.println(menuItems[i]);
  }
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 56);
  display.print("Move: joystick  Select: press");
  display.display();
}

void scanWiFi() {
  drawTitle("PASSIVE WI-FI SCAN");
  display.setCursor(0, 19);
  display.println("Listening for nearby");
  display.println("broadcast networks...");
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  networkCount = WiFi.scanNetworks(false, true);
  if (networkCount < 0) networkCount = 0;
  if (networkCount > MAX_NETWORKS) networkCount = MAX_NETWORKS;
  networkOffset = 0;
  mode = WIFI_RESULTS;
  WiFi.scanDelete();
}

void drawNetworks() {
  drawTitle("NETWORKS (READ ONLY)");
  if (networkCount == 0) {
    display.setCursor(0, 22);
    display.println("No networks found.");
  } else {
    for (int i = 0; i < 4; i++) {
      int idx = networkOffset + i;
      if (idx >= networkCount) break;
      display.setCursor(0, 14 + i * 11);
      display.print(idx + 1);
      display.print(". ");
      String name = networkNames[idx];
      if (name.length() == 0) name = "<hidden>";
      if (name.length() > 13) name = name.substring(0, 13);
      display.print(name);
      display.print(" ");
      display.print(networkSignals[idx]);
    }
  }
  display.setCursor(0, 57);
  display.print("L/R scroll  press=menu");
  display.display();
}

void resetSnake() {
  snakeLength = 3;
  snakeX[0] = 32; snakeY[0] = 32;
  snakeX[1] = 24; snakeY[1] = 32;
  snakeX[2] = 16; snakeY[2] = 32;
  dirX = 1; dirY = 0;
  foodX = random(8, 120) / 8 * 8;
  foodY = random(16, 56) / 8 * 8;
  gameOver = false;
}

void drawSnake() {
  drawTitle("SNAKE - press to exit");
  for (int i = 0; i < snakeLength; i++)
    display.fillRect(snakeX[i], snakeY[i], 6, 6, SSD1306_WHITE);
  display.fillRect(foodX, foodY, 6, 6, SSD1306_WHITE);
  display.setCursor(100, 0);
  display.print(snakeLength - 3);
  if (gameOver) {
    display.fillRect(20, 25, 88, 20, SSD1306_BLACK);
    display.drawRect(20, 25, 88, 20, SSD1306_WHITE);
    display.setCursor(31, 32);
    display.print("GAME OVER");
  }
  display.display();
}

void stepSnake() {
  int nx = snakeX[0] + dirX * 8;
  int ny = snakeY[0] + dirY * 8;
  if (nx < 0 || nx > 120 || ny < 12 || ny > 56) { gameOver = true; return; }
  for (int i = 0; i < snakeLength; i++)
    if (nx == snakeX[i] && ny == snakeY[i]) { gameOver = true; return; }

  bool ate = (nx == foodX && ny == foodY);
  if (ate && snakeLength < 40) {
    snakeLength++;
    foodX = random(8, 120) / 8 * 8;
    foodY = random(16, 56) / 8 * 8;
  }
  for (int i = snakeLength - 1; i > 0; i--) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }
  snakeX[0] = nx; snakeY[0] = ny;
}

void setup() {
  Serial.begin(115200);
  pinMode(JOY_SW, INPUT_PULLUP);
  analogReadResolution(12);
  randomSeed(micros());
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 not found. Check wiring/address.");
    while (true) delay(100);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(18, 18);
  display.println("FLOPPER");
  display.setCursor(38, 40);
  display.println("ZERO");
  display.display();
  delay(900);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
}

void loop() {
  unsigned long now = millis();
  int x = joyX(), y = joyY();
  bool click = pressed();

  if (mode == MENU) {
    if (now - lastInput > DEBOUNCE_MS) {
      if (y < 1500) { menuIndex = (menuIndex + MENU_COUNT - 1) % MENU_COUNT; lastInput = now; }
      else if (y > 2600) { menuIndex = (menuIndex + 1) % MENU_COUNT; lastInput = now; }
      else if (click) {
        lastInput = now;
        if (menuIndex == 0) scanWiFi();
        else if (menuIndex == 1) { resetSnake(); mode = SNAKE_GAME; }
        else mode = ABOUT;
      }
    }
    if (now - lastFrame > 80) { drawMenu(); lastFrame = now; }
  } else if (mode == WIFI_RESULTS) {
    if (now - lastInput > DEBOUNCE_MS) {
      if (x < 1500 && networkOffset > 0) { networkOffset--; lastInput = now; }
      else if (x > 2600 && networkOffset + 4 < networkCount) { networkOffset++; lastInput = now; }
      else if (click) { mode = MENU; lastInput = now; }
    }
    if (now - lastFrame > 100) { drawNetworks(); lastFrame = now; }
  } else if (mode == SNAKE_GAME) {
    if (click && now - lastInput > DEBOUNCE_MS) { mode = MENU; lastInput = now; }
    if (!gameOver && now - lastInput > 120) {
      if (y < 1500 && dirY == 0) { dirX = 0; dirY = -1; lastInput = now; }
      else if (y > 2600 && dirY == 0) { dirX = 0; dirY = 1; lastInput = now; }
      else if (x < 1500 && dirX == 0) { dirX = -1; dirY = 0; lastInput = now; }
      else if (x > 2600 && dirX == 0) { dirX = 1; dirY = 0; lastInput = now; }
    }
    if (!gameOver && now - lastSnakeStep > 180) { stepSnake(); lastSnakeStep = now; }
    if (now - lastFrame > 70) { drawSnake(); lastFrame = now; }
  } else if (mode == ABOUT) {
    if (click && now - lastInput > DEBOUNCE_MS) { mode = MENU; lastInput = now; }
    if (now - lastFrame > 100) {
      drawTitle("ABOUT / SAFETY");
      display.setCursor(0, 17);
      display.println("Read-only Wi-Fi scan.");
      display.println("No packet injection,");
      display.println("fake access points,");
      display.println("password capture or");
      display.println("login guessing.");
      display.setCursor(0, 57);
      display.print("Press to return");
      display.display();
      lastFrame = now;
    }
  }
  delay(5);
}
