#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <GyverButton.h>
#include "interface.h"

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif
#include <time.h>

// Pins
#define OLED_SCL 4
#define OLED_SDA 3
#define BTN_UP 6
#define BTN_DOWN 5
#define BTN_OK 7
#define BTN_BACK 8

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Buttons
GButton btn_up(BTN_UP, HIGH_PULL, NORM_OPEN);
GButton btn_down(BTN_DOWN, HIGH_PULL, NORM_OPEN);
GButton btn_ok(BTN_OK, HIGH_PULL, NORM_OPEN);
GButton btn_back(BTN_BACK, HIGH_PULL, NORM_OPEN);

// --- Custom embedded bitmaps (small icons / pattern) ---
// 16x16 joystick icon
static const unsigned char PROGMEM icon_joystick_bits[] = {
  0x00,0x00,0x03,0xc0,0x07,0xe0,0x0f,0xf0,0x1f,0xf8,0x1f,0xf8,0x0f,0xf0,0x07,0xe0,
  0x03,0xc0,0x00,0x00,0x00,0x00,0x1f,0xf8,0x1f,0xf8,0x1f,0xf8,0x07,0xe0,0x00,0x00
};

// 16x16 dice icon
static const unsigned char PROGMEM icon_dice_bits[] = {
  0xff,0xff,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x99,0x99,0x81,0x81,0x81,0x81,
  0x81,0x81,0x81,0x81,0xff,0xff,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00
};

// 16x16 memory (cards) icon
static const unsigned char PROGMEM icon_memory_bits[] = {
  0xfc,0x00,0xfe,0x00,0xff,0x00,0xff,0x80,0xff,0xc0,0xff,0xe0,0xff,0xe0,0xff,0xc0,
  0xff,0x80,0xff,0x00,0xfe,0x00,0xfc,0x00,0x00,0x00,0x18,0x00,0x18,0x00,0x00,0x00
};

// 16x16 pong icon (paddle + ball)
static const unsigned char PROGMEM icon_pong_bits[] = {
  0x00,0x00,0x1f,0x00,0x1f,0x00,0x1f,0x00,0x00,0x00,0x00,0x40,0x00,0xc0,0x00,0x60,
  0x00,0x30,0x00,0x18,0x00,0x0c,0x00,0x06,0x00,0x03,0x00,0x01,0x00,0x00,0x00,0x00
};

// 128x8 subtle horizontal stripe pattern (used as thin background)
static const unsigned char PROGMEM bg_strip_bits[] = {
  // 128 / 8 = 16 bytes per row; include 1 row of repeats to tile horizontally.
  0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55
};

// --- End custom bitmaps ---

// Menu states
enum emMenuState { menuLogo, menuMain, menuGames, menuSettings, menuAbout };
static emMenuState menuState = menuLogo;

// Main menu
static byte menuIndex = 0;
static const int menuItemsCount = 4;

// Games
static byte gameIndex = 0;
static const int gamesCount = 5;

// WiFi / Time / TZ / EEPROM layout
static bool wifiConnected = false;
static String wifi_ssid = "";
static String wifi_pass = "";
static const int EEPROM_SSID_ADDR = 0;   // 32
static const int EEPROM_PASS_ADDR = 32;  // 96
static const int EEPROM_FLAG_ADDR = 160;
static const byte EEPROM_FLAG = 0x42;
static const int EEPROM_TZ_ADDR = 200;
static int8_t tzOffsetHours = 0;
static const char *NTP_SERVER = "pool.ntp.org";

// GUI visibility flag — controls whether header/footer and other persistent GUI are drawn
static bool guiVisible = true;

// Prototypes
void OLED_printLogo();
void drawHeader(const String &clockStr);
void drawFooter();
void drawMainMenuAt(byte idx, int offsetX);
void animateMainMenu(byte fromIdx, byte toIdx, bool toLeft);
void OLED_printMenuStart();
void OLED_printSettings();
void OLED_printAbout();

void OLED_printGamesSliderAt(byte idx, int offsetX);
void drawGamesSliderFrame(byte idx, int offsetX);
void animateGamesSlider(byte fromIdx, byte toIdx, bool toLeft);
void OLED_printGamesStart();
void runGame(byte idx);

// Games
void game_Reaction();
void game_Catch();
void game_Dice();
void game_Memory();
void game_PongLite();

// WiFi / EEPROM / Time helpers
void loadWiFiCredentials();
void saveWiFiCredentials(const String &s, const String &p);
void clearWiFiCredentials();
void wifiConnectAttempt(unsigned long timeoutMs = 8000);
void wifiDisconnect();
String getClockString();
void handleSerialWifiConfig();
void loadTZ();
void saveTZ();
bool waitForTimeSync(unsigned long timeoutMs);

// ----------------- Setup / Loop -----------------
void setup() {
  Serial.begin(115200);
  delay(50);

  // Buttons
  btn_up.setDebounce(50); btn_up.setTimeout(500); btn_up.setClickTimeout(300); btn_up.setStepTimeout(200);
  btn_down.setDebounce(50); btn_down.setTimeout(500); btn_down.setClickTimeout(300); btn_down.setStepTimeout(200);
  btn_ok.setDebounce(50); btn_ok.setTimeout(500); btn_ok.setClickTimeout(300); btn_ok.setStepTimeout(200);
  btn_back.setDebounce(50); btn_back.setTimeout(500); btn_back.setClickTimeout(300); btn_back.setStepTimeout(200);
  btn_up.setTickMode(MANUAL); btn_down.setTickMode(MANUAL); btn_ok.setTickMode(MANUAL); btn_back.setTickMode(MANUAL);

  // OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.display();

  EEPROM.begin(512);

  // load saved data
  loadWiFiCredentials();
  loadTZ();

  if (wifi_ssid.length() > 0) wifiConnectAttempt(6000);

  // show animated logo
  guiVisible = false; // hide GUI while showing logo
  OLED_printLogo();

  // wait for key to continue (allow Serial input)
  while (!(btn_up.isClick() || btn_down.isClick() || btn_ok.isClick() || btn_back.isClick())) {
    btn_up.tick(); btn_down.tick(); btn_ok.tick(); btn_back.tick();
    handleSerialWifiConfig();
    delay(10);
  }

  // enter main menu -> show GUI
  menuState = menuMain;
  guiVisible = true;
  menuIndex = 0;
  OLED_printMenuStart();
  randomSeed(micros());
}

void loop() {
  btn_up.tick(); btn_down.tick(); btn_ok.tick(); btn_back.tick();
  handleSerialWifiConfig();

  switch (menuState) {
    case menuLogo:
      if (btn_ok.isClick() || btn_up.isClick() || btn_down.isClick() || btn_back.isClick()) {
        menuState = menuMain;
        guiVisible = true;
        OLED_printMenuStart();
      }
      break;

    case menuMain: {
      if (btn_up.isClick()) {
        byte old = menuIndex;
        menuIndex = (menuIndex == 0) ? menuItemsCount - 1 : menuIndex - 1;
        animateMainMenu(old, menuIndex, false);
      }
      if (btn_down.isClick()) {
        byte old = menuIndex;
        menuIndex = (menuIndex == menuItemsCount - 1) ? 0 : menuIndex + 1;
        animateMainMenu(old, menuIndex, true);
      }
      if (btn_ok.isClick()) {
        if (menuIndex == 0) { menuState = menuGames; guiVisible = true; gameIndex = 0; OLED_printGamesStart(); }
        else if (menuIndex == 1) { menuState = menuSettings; guiVisible = true; OLED_printSettings(); }
        else if (menuIndex == 2) { menuState = menuAbout; guiVisible = true; OLED_printAbout(); }
        else { menuState = menuLogo; guiVisible = false; OLED_printLogo(); }
      }
      if (btn_back.isClick()) { menuState = menuLogo; guiVisible = false; OLED_printLogo(); }

      static unsigned long lastClock = 0;
      if (millis() - lastClock > 1000) { lastClock = millis(); drawMainMenuAt(menuIndex, 0); }
      break;
    }

    case menuGames:
      if (btn_up.isClick()) { byte old = gameIndex; gameIndex = (gameIndex == 0) ? gamesCount - 1 : gameIndex - 1; animateGamesSlider(old, gameIndex, false); }
      if (btn_down.isClick()) { byte old = gameIndex; gameIndex = (gameIndex == gamesCount - 1) ? 0 : gameIndex + 1; animateGamesSlider(old, gameIndex, true); }
      if (btn_ok.isClick()) { runGame(gameIndex); OLED_printGamesStart(); }
      if (btn_back.isClick()) { menuState = menuMain; guiVisible = true; OLED_printMenuStart(); }
      break;

    case menuSettings:
      if (btn_ok.isClick()) {
        if (!wifiConnected) {
          if (wifi_ssid.length() > 0) wifiConnectAttempt(10000);
          else {
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(8, 12); display.print("No WiFi creds saved");
            display.setCursor(8, 24); display.print("Use Serial: WIFI:SSID,PASS");
            display.display();
          }
        } else {
          wifiDisconnect(); OLED_printSettings();
        }
      }
      if (btn_back.isClick()) { menuState = menuMain; guiVisible = true; OLED_printMenuStart(); }
      if (btn_back.isHolded()) {
        clearWiFiCredentials(); wifi_ssid = ""; wifi_pass = ""; wifiDisconnect();
        display.clearDisplay(); display.setTextSize(1); display.setCursor(8,24); display.print("WiFi cleared"); display.display();
        delay(900); OLED_printSettings();
      }
      break;

    case menuAbout:
      if (btn_back.isClick()) { menuState = menuMain; guiVisible = true; OLED_printMenuStart(); }
      break;
  }

  delay(10);
}

// ----------------- UI helpers -----------------
// If guiVisible is false header/footer and some decorations won't be drawn.
void drawHeader(const String &clockStr) {
  if (!guiVisible) return;

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // small satellite (left)
  display.drawBitmap(4, 2, image_satellite_dish_bits, 15, 16, SSD1306_WHITE);

  // app name centered
  const char *app = "ESP-Control";
  int appW = 6 * strlen(app); // approx width at size 1
  int appX = (SCREEN_WIDTH - appW) / 2;
  display.setCursor(appX, 4);
  display.print(app);

  // clock on right
  int cw = clockStr.length() * 6;
  int clockX = SCREEN_WIDTH - cw - 6;
  display.setCursor(clockX, 4);
  display.print(clockStr);

  // wifi indicator left of clock
  int barX = clockX - 14;
  int baseY = 6;
  if (wifiConnected) {
    display.fillRect(barX, baseY + 6, 2, 6, SSD1306_WHITE);
    display.fillRect(barX + 4, baseY + 3, 2, 9, SSD1306_WHITE);
    display.fillRect(barX + 8, baseY, 2, 12, SSD1306_WHITE);
  } else {
    display.drawRect(barX, baseY + 6, 2, 6, SSD1306_WHITE);
    display.drawRect(barX + 4, baseY + 3, 2, 9, SSD1306_WHITE);
    display.drawRect(barX + 8, baseY, 2, 12, SSD1306_WHITE);
  }

  // decorative subtle stripe (tile)
  display.drawBitmap(0, 18, bg_strip_bits, 128, 1, SSD1306_WHITE);

  // separator
  display.drawFastHLine(2, 19, SCREEN_WIDTH - 4, SSD1306_WHITE);
}

void drawFooter() {
  if (!guiVisible) return;
  display.drawFastHLine(2, SCREEN_HEIGHT - 14, SCREEN_WIDTH - 4, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(6, SCREEN_HEIGHT - 12);
  display.print("UP/DOWN - Nav   OK - Select   BACK - Exit");
}

// ----------------- Main menu -----------------
void drawMainMenuAt(byte idx, int offsetX) {
  display.clearDisplay();
  if (guiVisible) drawHeader(getClockString());

  // decorative rounded preview area
  const int areaX = 6 + offsetX;
  const int areaY = 22;
  const int areaW = 116;
  const int areaH = 36;

  display.drawRoundRect(areaX - 1, areaY - 1, areaW + 2, areaH + 2, 4, SSD1306_WHITE);

  // small left icon for the current item + title
  display.setTextSize(2);
  if (idx == 0) {
    display.drawBitmap(areaX + 4, areaY + 2, icon_joystick_bits, 16, 16, SSD1306_WHITE);
    display.setCursor(areaX + 26, areaY + 4); display.print("Games");
  } else if (idx == 1) {
    display.drawBitmap(areaX + 4, areaY + 2, image_Connected_bits, 16, 16, SSD1306_WHITE);
    display.setCursor(areaX + 26, areaY + 4); display.print("Settings");
  } else if (idx == 2) {
    display.drawBitmap(areaX + 4, areaY + 2, image_DolphinSaved_bits, 16, 16, SSD1306_WHITE);
    display.setCursor(areaX + 26, areaY + 4); display.print("About");
  } else {
    display.drawBitmap(areaX + 4, areaY + 2, image_file_delete_bin_bits, 16, 16, SSD1306_WHITE);
    display.setCursor(areaX + 26, areaY + 4); display.print("Exit");
  }

  // arrows (only show if GUI visible)
  if (guiVisible) {
    display.setTextSize(1);
    display.setCursor(4, 22); display.print("<");
    display.setCursor(SCREEN_WIDTH - 10, 22); display.print(">");
    drawFooter();
  }

  display.display();
}

// smoother animation with proper compositing and final frame stability
void animateMainMenu(byte fromIdx, byte toIdx, bool toLeft) {
  const int steps = 12;
  for (int s = 1; s <= steps; ++s) {
    float p = (float)s / (float)steps;
    float ease = p < 0.5f ? 2.0f * p * p : -1.0f + (4.0f - 2.0f * p) * p;
    int t = (int)(SCREEN_WIDTH * ease);
    int offFrom = toLeft ? -t : t;
    int offTo = toLeft ? SCREEN_WIDTH - t : -(SCREEN_WIDTH - t);

    display.clearDisplay();
    if (guiVisible) drawHeader(getClockString());

    // draw 'from' and 'to' boxes and their content shifted
    const int areaY = 22; const int areaW = 116; const int areaH = 36;
    int areaXf = 6 + offFrom;
    int areaXt = 6 + offTo;

    display.drawRoundRect(areaXf - 1, areaY - 1, areaW + 2, areaH + 2, 4, SSD1306_WHITE);
    if (fromIdx == 0) { display.drawBitmap(areaXf + 4, areaY + 2, icon_joystick_bits, 16, 16, SSD1306_WHITE); display.setCursor(areaXf + 26, areaY + 4); display.setTextSize(2); display.print("Games"); }
    else if (fromIdx == 1) { display.drawBitmap(areaXf + 4, areaY + 2, image_Connected_bits, 16, 16, SSD1306_WHITE); display.setCursor(areaXf + 26, areaY + 4); display.setTextSize(2); display.print("Settings"); }
    else if (fromIdx == 2) { display.drawBitmap(areaXf + 4, areaY + 2, image_DolphinSaved_bits, 16, 16, SSD1306_WHITE); display.setCursor(areaXf + 26, areaY + 4); display.setTextSize(2); display.print("About"); }
    else { display.drawBitmap(areaXf + 4, areaY + 2, image_file_delete_bin_bits, 16, 16, SSD1306_WHITE); display.setCursor(areaXf + 26, areaY + 4); display.setTextSize(2); display.print("Exit"); }

    display.drawRoundRect(areaXt - 1, areaY - 1, areaW + 2, areaH + 2, 4, SSD1306_WHITE);
    if (toIdx == 0) { display.drawBitmap(areaXt + 4, areaY + 2, icon_joystick_bits, 16, 16, SSD1306_WHITE); display.setCursor(areaXt + 26, areaY + 4); display.setTextSize(2); display.print("Games"); }
    else if (toIdx == 1) { display.drawBitmap(areaXt + 4, areaY + 2, image_Connected_bits, 16, 16, SSD1306_WHITE); display.setCursor(areaXt + 26, areaY + 4); display.setTextSize(2); display.print("Settings"); }
    else if (toIdx == 2) { display.drawBitmap(areaXt + 4, areaY + 2, image_DolphinSaved_bits, 16, 16, SSD1306_WHITE); display.setCursor(areaXt + 26, areaY + 4); display.setTextSize(2); display.print("About"); }
    else { display.drawBitmap(areaXt + 4, areaY + 2, image_file_delete_bin_bits, 16, 16, SSD1306_WHITE); display.setCursor(areaXt + 26, areaY + 4); display.setTextSize(2); display.print("Exit"); }

    if (guiVisible) drawFooter();
    display.display();

    // keep processing buttons so user can interrupt animation
    btn_up.tick(); btn_down.tick(); btn_ok.tick(); btn_back.tick();
    if (btn_ok.isClick() || btn_back.isClick()) break;
    delay(16);
  }

  // final centered state
  drawMainMenuAt(toIdx, 0);
}

void OLED_printMenuStart() {
  for (int i = 0; i < 6; ++i) {
    int offset = map(i, 0, 5, -8, 0);
    drawMainMenuAt(menuIndex, offset);
    delay(22);
  }
  drawMainMenuAt(menuIndex, 0);
}

// ----------------- Games slider (fixed bug) -----------------
// Bug fixed: previous layering drew the 'to' frame on top repeatedly causing artifacts.
// Now animation renders both frames clearly each step and performs a single display.display() per frame.
// Bounds and ticks are handled properly; final frame drawn cleanly by OLED_printGamesSliderAt.

void drawGamesSliderFrame(byte idx, int offsetX) {
  if (guiVisible) drawHeader(getClockString());

  display.setTextSize(1);
  display.setCursor(6 + offsetX, 22);
  display.print("Games");

  // compute left/center/right indices safely
  byte left = (idx == 0) ? (gamesCount - 1) : idx - 1;
  byte right = (idx == gamesCount - 1) ? 0 : idx + 1;

  // left preview (small icon + label)
  int leftX = 6 + offsetX - 34;
  display.drawBitmap(leftX, 26, image_Scanning_short_bits, 96, 52, SSD1306_WHITE);
  display.setCursor(leftX + 4, 52);
  switch (left) {
    case 0: display.print("Reaction"); break;
    case 1: display.print("Catch"); break;
    case 2: display.print("Dice"); break;
    case 3: display.print("Memory"); break;
    case 4: display.print("Pong"); break;
  }

  // center large preview
  display.drawBitmap(26 + offsetX, 18, image_Scanning_bits, 123, 52, SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(46 + offsetX, 14);
  switch (idx) {
    case 0: display.print("Reaction"); break;
    case 1: display.print("Catch"); break;
    case 2: display.print("Dice"); break;
    case 3: display.print("Memory"); break;
    case 4: display.print("Pong"); break;
  }

  // overlay a small icon for the center game
  int iconX = 30 + offsetX;
  int iconY = 30;
  switch (idx) {
    case 0: display.drawBitmap(iconX, iconY, icon_joystick_bits, 16, 16, SSD1306_WHITE); break;
    case 1: display.drawBitmap(iconX, iconY, icon_joystick_bits, 16, 16, SSD1306_WHITE); break;
    case 2: display.drawBitmap(iconX, iconY, icon_dice_bits, 16, 16, SSD1306_WHITE); break;
    case 3: display.drawBitmap(iconX, iconY, icon_memory_bits, 16, 16, SSD1306_WHITE); break;
    case 4: display.drawBitmap(iconX, iconY, icon_pong_bits, 16, 16, SSD1306_WHITE); break;
  }

  // right preview
  display.setTextSize(1);
  display.drawBitmap(106 + offsetX, 26, image_Scanning_short_bits, 96, 52, SSD1306_WHITE);
  display.setCursor(110 + offsetX, 52);
  switch (right) {
    case 0: display.print("Reaction"); break;
    case 1: display.print("Catch"); break;
    case 2: display.print("Dice"); break;
    case 3: display.print("Memory"); break;
    case 4: display.print("Pong"); break;
  }
}

void OLED_printGamesSliderAt(byte idx, int offsetX) {
  display.clearDisplay();
  drawGamesSliderFrame(idx, offsetX);
  if (guiVisible) drawFooter();
  display.display();
}

void animateGamesSlider(byte fromIdx, byte toIdx, bool toLeft) {
  const int steps = 12;
  for (int s = 1; s <= steps; ++s) {
    float p = (float)s / (float)steps;
    float ease = p < 0.5f ? 2.0f * p * p : -1.0f + (4.0f - 2.0f * p) * p;
    int t = (int)(SCREEN_WIDTH * ease);
    int offFrom = toLeft ? -t : t;
    int offTo = toLeft ? SCREEN_WIDTH - t : -(SCREEN_WIDTH - t);

    display.clearDisplay();
    // draw both frames composited (from then to) so motion looks correct
    drawGamesSliderFrame(fromIdx, offFrom);
    drawGamesSliderFrame(toIdx, offTo);
    if (guiVisible) drawFooter();
    display.display();

    // allow input during animation
    btn_up.tick(); btn_down.tick(); btn_ok.tick(); btn_back.tick();
    if (btn_ok.isClick() || btn_back.isClick()) break;
    delay(16);
  }
  // final stable frame
  OLED_printGamesSliderAt(toIdx, 0);
}

void OLED_printGamesStart() {
  // subtle bounce
  for (int i = 0; i < 4; ++i) {
    OLED_printGamesSliderAt(gameIndex, (i & 1) ? -6 : 6);
    delay(60);
  }
  OLED_printGamesSliderAt(gameIndex, 0);
}

// ----------------- Run games -----------------
void runGame(byte idx) {
  // ensure idx valid
  idx = idx % gamesCount;
  switch (idx) {
    case 0: game_Reaction(); break;
    case 1: game_Catch(); break;
    case 2: game_Dice(); break;
    case 3: game_Memory(); break;
    case 4: game_PongLite(); break;
    default: break;
  }
}

// ----------------- Settings / About -----------------
void OLED_printSettings() {
  display.clearDisplay();
  if (guiVisible) drawHeader(getClockString());
  display.setTextSize(2);
  display.setCursor(8, 26); display.print("Settings");
  display.setTextSize(1);
  display.setCursor(8, 46); display.print("UP:"); display.print(BTN_UP);
  display.setCursor(48, 46); display.print("DN:"); display.print(BTN_DOWN);
  display.setCursor(88, 46); display.print("OK:"); display.print(BTN_OK);

  display.setCursor(8, 56);
  if (wifiConnected) {
    display.print("WiFi: "); display.print(WiFi.SSID());
    display.print(" "); display.print(WiFi.localIP().toString());
  } else {
    if (wifi_ssid.length()) { display.print("Saved: "); display.print(wifi_ssid); }
    else { display.print("WiFi: none (Serial->WIFI:SSID,PASS)"); }
  }

  display.setCursor(8, 64 - 2);
  display.print("TZ: "); display.print(tzOffsetHours);

  display.display();
}

void OLED_printAbout() {
  display.clearDisplay();
  if (guiVisible) drawHeader(getClockString());
  display.setTextSize(2);
  display.setCursor(14, 22); display.print("About");
  display.setTextSize(1);
  display.setCursor(8, 44); display.print("ESP-Control v1.0");
  display.setCursor(8, 54); display.print("Custom icons + polished UI");
  display.display();
}

// ----------------- Logo (polished) -----------------
void OLED_printLogo() {
  // guiVisible is false when calling this function
  const char *title = "ESP-Cont";
  const char *badge = "roll";
  const int titleSize = 2;
  const int badgeSize = 1;
  const int approxCharW = 6;

  int titleChars = strlen(title);
  int titleW = approxCharW * titleSize * titleChars;
  int titleX = max(0, (SCREEN_WIDTH - titleW) / 2);
  int titleY = 18;
  int badgeW = approxCharW * badgeSize * strlen(badge);
  int badgeX = titleX + titleW - badgeW - 6;
  int badgeY = titleY + 22;

  for (int i = 0; i < 14; ++i) {
    display.clearDisplay();

    // Title
    display.setTextSize(titleSize);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(titleX, titleY);
    display.print(title);

    // Underline
    int ux1 = titleX;
    int ux2 = constrain(titleX + titleW, 2, SCREEN_WIDTH - 4);
    display.drawFastHLine(ux1, titleY + 14, ux2 - ux1, SSD1306_WHITE);

    // Badge with rounded rectangle
    int padX = 4;
    int padY = 2;
    int rectW = badgeW + padX * 2;
    int rectH = 10;
    int rectX = max(4, badgeX - padX);
    int rectY = min(badgeY - padY, SCREEN_HEIGHT - rectH - 10);
    display.fillRoundRect(rectX, rectY, rectW, rectH, 3, SSD1306_WHITE);
    display.setTextSize(badgeSize);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setCursor(rectX + padX, rectY + 2);
    display.print(badge);
    display.setTextColor(SSD1306_WHITE);

    // subtitle
    display.setTextSize(1);
    const char *sub = "by KyrexX  v1.0";
    int subW = approxCharW * strlen(sub);
    display.setCursor((SCREEN_WIDTH - subW) / 2, rectY + rectH + 6);
    display.print(sub);

    // satellite bobbing at right
    int bob = (i % 6) - 3;
    int satX = SCREEN_WIDTH - 22;
    int satY = 6 + bob;
    display.drawBitmap(satX, satY, image_satellite_dish_bits, 15, 16, SSD1306_WHITE);

    display.display();

    btn_up.tick(); btn_down.tick(); btn_ok.tick(); btn_back.tick();
    if (btn_ok.isClick() || btn_up.isClick() || btn_down.isClick() || btn_back.isClick()) break;
    delay(70);
  }
}

// ----------------- Games implementations (kept behavior, small UI improvements) -----------------
void game_Reaction() {
  display.clearDisplay(); if (guiVisible) drawHeader(getClockString());
  display.setTextSize(2); display.setCursor(10, 20); display.print("Reaction");
  display.setTextSize(1); display.setCursor(10, 44); display.print("Press OK to start"); display.display();
  while (!btn_ok.isClick()) { btn_up.tick(); btn_down.tick(); btn_ok.tick(); btn_back.tick(); if (btn_back.isClick()) return; delay(10); }

  unsigned long waitMs = random(700, 2500);
  display.clearDisplay(); if (guiVisible) drawHeader(getClockString());
  display.setTextSize(1); display.setCursor(10, 24); display.print("Get ready..."); display.display();
  unsigned long t0 = millis();
  while (millis() - t0 < waitMs) {
    btn_up.tick(); btn_down.tick(); btn_ok.tick(); btn_back.tick();
    if (btn_ok.isClick()) { display.clearDisplay(); display.setCursor(10,24); display.print("Too early!"); display.display(); delay(800); return; }
    if (btn_back.isClick()) return;
    delay(10);
  }

  display.clearDisplay(); if (guiVisible) drawHeader(getClockString());
  display.setTextSize(3); display.setCursor(36, 12); display.print("GO!"); display.display();
  unsigned long tstart = micros();
  while (true) {
    btn_up.tick(); btn_down.tick(); btn_ok.tick(); btn_back.tick();
    if (btn_ok.isClick()) {
      unsigned long tend = micros(); unsigned long react = (tend - tstart) / 1000;
      display.clearDisplay(); if (guiVisible) drawHeader(getClockString());
      display.setTextSize(1); display.setCursor(10, 20); display.print("Reaction:");
      display.setTextSize(2); display.setCursor(10, 34); display.print(react); display.print("ms"); display.display();
      delay(1200); return;
    }
    if (btn_back.isClick()) return;
    delay(10);
  }
}

void game_Catch() {
  const int cols = 4;
  int paddle = 1, objCol = random(0, cols), objY = 0, score = 0, lives = 3;
  unsigned long lastMove = millis(), speed = 450;
  while (true) {
    btn_up.tick(); btn_down.tick(); btn_ok.tick(); btn_back.tick();
    if (btn_back.isClick()) return;
    if (btn_up.isClick()) paddle = (paddle == 0) ? cols - 1 : paddle - 1;
    if (btn_down.isClick()) paddle = (paddle == cols - 1) ? 0 : paddle + 1;

    if (millis() - lastMove >= speed) {
      lastMove = millis(); objY++;
      if (objY > 5) {
        if (objCol == paddle) { score++; if (speed > 150) speed -= 15; }
        else { lives--; if (lives <= 0) { display.clearDisplay(); if (guiVisible) drawHeader(getClockString()); display.setTextSize(2); display.setCursor(10,16); display.print("Game Over"); display.setTextSize(1); display.setCursor(28,42); display.print("Score:"); display.print(score); display.display(); delay(1200); return; } }
        objCol = random(0, cols); objY = 0;
      }
    }

    display.clearDisplay(); if (guiVisible) drawHeader(getClockString());
    display.setTextSize(1); display.setCursor(6, 22); display.print("Catch");
    display.setCursor(86, 22); display.print("S:"); display.print(score);
    display.setCursor(106, 22); display.print("L:"); display.print(lives);

    int colW = SCREEN_WIDTH / cols;
    for (int c = 0; c < cols; ++c) {
      int x = c * colW + (colW / 2 - 6);
      if (c == paddle) display.fillRect(x, 56, 12, 6, SSD1306_WHITE);
      else display.drawRect(x, 56, 12, 6, SSD1306_WHITE);
      if (c == objCol) {
        int y = objY * 9 + 28;
        display.fillRect(x, y, 12, 6, SSD1306_WHITE);
      }
    }

    if (guiVisible) drawFooter();
    display.display();
    delay(18);
  }
}

void game_Dice() {
  display.clearDisplay(); if (guiVisible) drawHeader(getClockString());
  display.setTextSize(2); display.setCursor(10, 20); display.print("Dice");
  display.setTextSize(1); display.setCursor(10, 44); display.print("Press OK to roll"); display.display();
  while (true) {
    btn_up.tick(); btn_down.tick(); btn_ok.tick(); btn_back.tick();
    if (btn_back.isClick()) return;
    if (btn_ok.isClick()) {
      for (int i = 0; i < 10; ++i) {
        int n = random(1, 7);
        display.clearDisplay(); if (guiVisible) drawHeader(getClockString());
        display.setTextSize(4); display.setCursor(36, 16); display.print(n); display.display();
        delay(60 + i * 20);
      }
      int res = random(1, 7);
      display.clearDisplay(); if (guiVisible) drawHeader(getClockString());
      display.setTextSize(4); display.setCursor(36, 16); display.print(res); display.display();
      delay(900);
      display.clearDisplay(); if (guiVisible) drawHeader(getClockString());
      display.setTextSize(1); display.setCursor(10, 44); display.print("Press OK to roll"); display.display();
    }
    delay(10);
  }
}

void game_Memory() {
  const int maxLen = 6;
  uint8_t seq[maxLen]; int len = 1;
  for (int i = 0; i < maxLen; ++i) seq[i] = random(0,4);

  while (true) {
    for (int i = 0; i < len; ++i) {
      display.clearDisplay(); if (guiVisible) drawHeader(getClockString());
      display.setTextSize(2); display.setCursor(10,6); display.print("Watch:");
      display.setTextSize(4); display.setCursor(48,20);
      switch (seq[i]) { case 0: display.print("U"); break; case 1: display.print("D"); break; case 2: display.print("O"); break; case 3: display.print("B"); break; }
      display.display();
      unsigned long t0 = millis();
      while (millis() - t0 < 700) { btn_up.tick(); btn_down.tick(); btn_ok.tick(); btn_back.tick(); if (btn_back.isHolded()) return; delay(10); }
      delay(120);
    }

    display.clearDisplay(); if (guiVisible) drawHeader(getClockString());
    display.setTextSize(1); display.setCursor(6,6); display.print("Repeat using buttons"); display.setCursor(6,24); display.print("U D O B"); display.display();

    for (int i = 0; i < len; ++i) {
      bool got = false;
      while (!got) {
        btn_up.tick(); btn_down.tick(); btn_ok.tick(); btn_back.tick();
        if (btn_back.isHolded()) return;
        if (btn_up.isClick()) { if (seq[i] != 0) { display.clearDisplay(); display.setCursor(10,28); display.print("Wrong!"); display.display(); delay(800); return; } got = true; }
        else if (btn_down.isClick()) { if (seq[i] != 1) { display.clearDisplay(); display.setCursor(10,28); display.print("Wrong!"); display.display(); delay(800); return; } got = true; }
        else if (btn_ok.isClick()) { if (seq[i] != 2) { display.clearDisplay(); display.setCursor(10,28); display.print("Wrong!"); display.display(); delay(800); return; } got = true; }
        else if (btn_back.isClick()) { if (seq[i] != 3) { display.clearDisplay(); display.setCursor(10,28); display.print("Wrong!"); display.display(); delay(800); return; } got = true; }
        delay(10);
      }
      display.clearDisplay(); if (guiVisible) drawHeader(getClockString());
      display.setTextSize(2); display.setCursor(10,20); display.print("OK"); display.display(); delay(250);
    }

    if (len < maxLen) ++len;
    else { display.clearDisplay(); if (guiVisible) drawHeader(getClockString()); display.setTextSize(2); display.setCursor(10,24); display.print("You Win!"); display.display(); delay(1200); return; }
  }
}

void game_PongLite() {
  int paddleY = 24; int ballX = 64, ballY = 30; int vx = 2, vy = 1; unsigned long last = millis();
  while (true) {
    btn_up.tick(); btn_down.tick(); btn_ok.tick(); btn_back.tick();
    if (btn_back.isClick()) return;
    if (btn_up.isHold() || btn_up.isPress()) { paddleY -= 2; if (paddleY < 0) paddleY = 0; }
    if (btn_down.isHold() || btn_down.isPress()) { paddleY += 2; if (paddleY > 56 - 12) paddleY = 56 - 12; }
    if (millis() - last > 30) {
      last = millis(); ballX += vx; ballY += vy;
      if (ballY <= 8 || ballY >= 56) vy = -vy;
      if (ballX <= 8) {
        if (ballY >= paddleY && ballY <= paddleY + 12) { vx = -vx; ballX = 10; } else { ballX = 64; ballY = 30; vx = 2; vy = (random(0,2) ? 1 : -1); }
      }
      if (ballX >= 120) vx = -vx;
    }
    display.clearDisplay(); if (guiVisible) drawHeader(getClockString());
    display.fillRect(0, paddleY + 8, 6, 12, SSD1306_WHITE); display.fillRect(ballX, ballY, 3, 3, SSD1306_WHITE);
    if (guiVisible) drawFooter(); display.display(); delay(10);
  }
}

// ----------------- WiFi / EEPROM / Time -----------------
void loadWiFiCredentials() {
  byte flag = EEPROM.read(EEPROM_FLAG_ADDR);
  if (flag != EEPROM_FLAG) { wifi_ssid = ""; wifi_pass = ""; return; }
  char buf[97];
  for (int i = 0; i < 32; ++i) buf[i] = (char)EEPROM.read(EEPROM_SSID_ADDR + i);
  buf[32] = 0; wifi_ssid = String(buf);
  for (int i = 0; i < 96; ++i) buf[i] = (char)EEPROM.read(EEPROM_PASS_ADDR + i);
  buf[96] = 0; wifi_pass = String(buf);
  Serial.print("Loaded WiFi SSID: "); Serial.println(wifi_ssid);
}

void saveWiFiCredentials(const String &s, const String &p) {
  String ss = s, pp = p;
  if (ss.length() > 31) ss = ss.substring(0,31);
  if (pp.length() > 95) pp = pp.substring(0,95);
  for (int i = 0; i < 32; ++i) EEPROM.write(EEPROM_SSID_ADDR + i, (i < ss.length()) ? ss[i] : 0);
  for (int i = 0; i < 96; ++i) EEPROM.write(EEPROM_PASS_ADDR + i, (i < pp.length()) ? pp[i] : 0);
  EEPROM.write(EEPROM_FLAG_ADDR, EEPROM_FLAG);
  EEPROM.commit();
  wifi_ssid = ss; wifi_pass = pp;
  Serial.println("WiFi credentials saved.");
}

void clearWiFiCredentials() {
  for (int i = 0; i < 32; ++i) EEPROM.write(EEPROM_SSID_ADDR + i, 0);
  for (int i = 0; i < 96; ++i) EEPROM.write(EEPROM_PASS_ADDR + i, 0);
  EEPROM.write(EEPROM_FLAG_ADDR, 0);
  EEPROM.commit();
  Serial.println("WiFi credentials cleared.");
}

void loadTZ() {
  int val = EEPROM.read(EEPROM_TZ_ADDR);
  tzOffsetHours = (val == 0xFF) ? 0 : (int8_t)val;
  Serial.print("Loaded TZ offset: "); Serial.println(tzOffsetHours);
}

void saveTZ() {
  EEPROM.write(EEPROM_TZ_ADDR, (uint8_t)tzOffsetHours);
  EEPROM.commit();
  Serial.print("Saved TZ offset: "); Serial.println(tzOffsetHours);
}

bool waitForTimeSync(unsigned long timeoutMs) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    time_t now = time(nullptr);
    if (now > 1600000000UL) return true;
    delay(200);
  }
  return false;
}

void wifiConnectAttempt(unsigned long timeoutMs) {
  if (wifi_ssid.length() == 0) { Serial.println("No SSID saved"); return; }
  display.clearDisplay(); display.setTextSize(1); display.setCursor(8, 26); display.print("Connecting to"); display.setCursor(8, 36); display.print(wifi_ssid); display.display();
  Serial.print("Connecting to "); Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  unsigned long start = millis(); wifiConnected = false;
  while (millis() - start < timeoutMs) {
    btn_up.tick(); btn_down.tick(); btn_ok.tick(); btn_back.tick();
    if (WiFi.status() == WL_CONNECTED) { wifiConnected = true; break; }
    if (btn_back.isClick()) { Serial.println("User aborted WiFi connect"); break; }
    delay(160);
  }
  if (wifiConnected && WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    configTime(0, 0, NTP_SERVER);
    waitForTimeSync(8000);
    OLED_printSettings();
  } else {
    wifiConnected = false;
    display.clearDisplay(); display.setTextSize(1); display.setCursor(8, 26); display.print("WiFi failed"); display.display(); Serial.println("WiFi failed.");
  }
}

void wifiDisconnect() {
  if (WiFi.status() == WL_CONNECTED) { WiFi.disconnect(); delay(100); }
  wifiConnected = false;
  Serial.println("WiFi disconnected.");
}

String getClockString() {
  time_t now = time(nullptr);
  if (wifiConnected && now > 1600000000UL) {
    now += (time_t)tzOffsetHours * 3600;
    struct tm t; gmtime_r(&now, &t);
    char buf[6]; snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    return String(buf);
  } else if (wifiConnected) return String("sync..");
  else {
    unsigned long s = millis() / 1000; unsigned long mm = (s/60)%60; unsigned long ss = s%60;
    char buf[8]; snprintf(buf, sizeof(buf), "%02lu:%02lu", mm, ss); return String(buf);
  }
}

void handleSerialWifiConfig() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n'); line.trim();
  if (line.length() == 0) return;
  Serial.print("Serial> "); Serial.println(line);
  if (line.startsWith("WIFI:")) {
    String rest = line.substring(5);
    int comma = rest.indexOf(',');
    if (comma > 0) {
      String ss = rest.substring(0, comma);
      String pp = rest.substring(comma + 1);
      saveWiFiCredentials(ss, pp);
      display.clearDisplay(); display.setTextSize(1); display.setCursor(8, 24); display.print("Saved WiFi:"); display.setCursor(8, 34); display.print(ss); display.display(); delay(800);
      wifiConnectAttempt(8000); OLED_printSettings();
    } else Serial.println("Invalid format. Use: WIFI:SSID,PASS");
  } else if (line.startsWith("TZ:")) {
    String rest = line.substring(3); rest.trim();
    if (rest.length() == 0) { Serial.println("TZ: specify hours (e.g. TZ:+3)"); return; }
    int val = rest.toInt();
    if (val < -12 || val > 14) { Serial.println("TZ out of range (-12..14)"); return; }
    tzOffsetHours = (int8_t)val; saveTZ();
    Serial.print("TZ set to "); Serial.println(tzOffsetHours);
    OLED_printSettings();
  } else Serial.println("Unknown command.");
}