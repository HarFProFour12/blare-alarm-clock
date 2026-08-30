#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>
#include <wifi_codes.h>

#define TFT_SCLK D9
#define TFT_MOSI D10
#define TFT_RST  D8
#define TFT_DC   D4
#define TFT_CS   D5
#define TFT_BL   D6

#define enterPin D0
#define backPin  D1
#define upPin    D2
#define downPin  D3

#define buzzerPin D7

#define NOTE_B3   247
#define NOTE_C4   262
#define NOTE_C4S  277
#define NOTE_D4   294
#define NOTE_E4   330
#define NOTE_F4   349
#define NOTE_F4S  370
#define NOTE_G4   392
#define NOTE_A4   440
#define NOTE_B4   494
#define NOTE_C5   523
#define NOTE_D5   587
#define NOTE_D5S  622
#define NOTE_E5   659

uint16_t bg_color = 0x883;
uint16_t text_color = 0x07E0;
uint16_t time_size = 6;

int settings_selected = 0;
int color_selected = 0;
int time_selected = 0;
int sounds_selected = 0;

int wake_time_hours = 7;
int wake_time_minutes = 0;

int current_hour = 0;
int current_minute = 0;

bool alarmRinging = false;
unsigned long snoozeUntil = 0;
unsigned long lastSnoozePress = 0;
int snoozeMinutes = 5;

const char* colorNames[6] = { "White", "Red", "Green", "Blue", "Yellow", "Cyan" };
uint16_t colorValues[6] = {
  0xFFFF, // White
  0xF800, // Red
  0x07E0, // Green
  0x001F, // Blue
  0xFFE0, // Yellow
  0x07FF  // Cyan
};

int colorX[6] = { 43, 43, 43, 148, 148, 148 };
int colorY[6] = { 6, 30, 54, 6, 30, 54 };

bool alarmFiredThisMinute = false;

// Für Elise
int furEliseNotes[] = { NOTE_E5, NOTE_D5S, NOTE_E5, NOTE_D5S, NOTE_E5, NOTE_B4, NOTE_D5, NOTE_C5, NOTE_A4 };
int furEliseDurations[] = { 200, 200, 200, 200, 200, 200, 200, 200, 400 };
int furEliseLength = 9;

// Reveille
int reveilleNotes[] = { NOTE_C4, NOTE_C4, NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5, NOTE_G4, NOTE_E4, NOTE_C4 };
int reveilleDurations[] = { 150, 150, 300, 300, 300, 400, 300, 300, 500 };
int reveilleLength = 9;

// In the Hall of the Mountain King
int mountainKingNotes[] = { NOTE_B3, NOTE_C4S, NOTE_D4, NOTE_E4, NOTE_F4S, NOTE_G4, NOTE_F4S, NOTE_E4, NOTE_D4, NOTE_C4S, NOTE_B3 };
int mountainKingDurations[] = { 300, 300, 300, 300, 300, 300, 200, 200, 200, 200, 400 };
int mountainKingLength = 11;

enum Screens {
    TIME,
    SETTINGS,
    ALARM_TIME,
    ALARM_SOUND,
    FONT_COLOR
};


Screens currentScreen = TIME;
Screens lastDrawnScreen = SETTINGS;
int lastDrawnSelection = -1;

class MyST7789 : public Adafruit_ST7789 {
public:
  MyST7789(int8_t cs, int8_t dc, int8_t mosi, int8_t sclk, int8_t rst)
    : Adafruit_ST7789(cs, dc, mosi, sclk, rst) {}
  void setOffsets(uint8_t col, uint8_t row) {
    _colstart = _colstart2 = col;
    _rowstart = _rowstart2 = row;
  }
};

MyST7789 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

void setup() {
    Serial.begin(115200); // lets the board talk to your computer
    pinMode(TFT_BL, OUTPUT); // Set the backlight pin mode, or just wire it to 3.3V
    digitalWrite(TFT_BL, LOW); // Turns the backlight ON, for some reason this screen is active Low, so setting it to LOW is really HIGH

    tft.init(76, 284); // Our panel size (portrait)
    tft.setOffsets(82, 18); // Offsets for the weird resolution
    tft.invertDisplay(false); // Invert the colors (This display is flipped from normal)
    tft.setRotation(1); // Landscape, if it's upside down use 3!
    Serial.println("TFT Initialized!");

    tft.fillScreen(bg_color); // clear the screen

    pinMode(enterPin, INPUT_PULLUP);
    pinMode(backPin, INPUT_PULLUP);
    pinMode(upPin, INPUT_PULLUP);
    pinMode(downPin, INPUT_PULLUP); 

    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) delay(500);
}

void loop() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        current_hour = timeinfo.tm_hour;
        current_minute = timeinfo.tm_min;
    }

    if (wake_time_hours == current_hour && wake_time_minutes == current_minute) {
        if (!alarmFiredThisMinute) {
            alarmFiredThisMinute = true;
            alarmRinging = true;
        }
    }
    else {
        alarmFiredThisMinute = false;
    }

    if (snoozeUntil != 0 && millis() >= snoozeUntil) {
        snoozeUntil = 0;
        alarmRinging = true;
    }

    if (alarmRinging) {
        handleRingingAlarm();
    }
    bool screenChanged = (currentScreen != lastDrawnScreen);

    switch (currentScreen) {

        case TIME: {
            if (screenChanged) {
                drawTime();
                lastDrawnScreen = currentScreen;
            }
            if (digitalRead(enterPin) == LOW) {
                currentScreen = SETTINGS;
                settings_selected = 0;
            }
            break;
        }

        case SETTINGS: {
            if (digitalRead(downPin) == LOW) settings_selected++;
            if (digitalRead(upPin) == LOW) settings_selected--;
            if (settings_selected > 2) settings_selected = 0;
            if (settings_selected < 0) settings_selected = 2;

            if (screenChanged || settings_selected != lastDrawnSelection) {
                drawSettings();
                lastDrawnScreen = currentScreen;
                lastDrawnSelection = settings_selected;
            }

            if (digitalRead(enterPin) == LOW) {
                if (settings_selected == 0) currentScreen = ALARM_TIME;
                if (settings_selected == 1) currentScreen = ALARM_SOUND;
                if (settings_selected == 2) currentScreen = FONT_COLOR;
                lastDrawnSelection = -1;
            }

            if (digitalRead(backPin) == LOW) {
                currentScreen = TIME;
            }
            break;
        }

        case ALARM_SOUND: {
            if (digitalRead(downPin) == LOW) sounds_selected++;
            if (digitalRead(upPin) == LOW) sounds_selected--;
            if (sounds_selected > 2) sounds_selected = 0;
            if (sounds_selected < 0) sounds_selected = 2;

            if (screenChanged || sounds_selected != lastDrawnSelection) {
                drawAlarmSound();
                lastDrawnScreen = currentScreen;
                lastDrawnSelection = sounds_selected;
            }

            if (digitalRead(enterPin) == LOW) {
                if (sounds_selected == 0) playMelody(furEliseNotes, furEliseDurations, furEliseLength);
                if (sounds_selected == 1) playMelody(reveilleNotes, reveilleDurations, reveilleLength);
                if (sounds_selected == 2) playMelody(mountainKingNotes, mountainKingDurations, mountainKingLength);
            }

            if (digitalRead(backPin) == LOW) {
                currentScreen = SETTINGS;
                lastDrawnSelection = -1;
            }
            break;
        }

        case ALARM_TIME: {
            if (digitalRead(downPin) == LOW) {
                if (time_selected == 0) wake_time_hours--;
                else wake_time_minutes--;
            }
            if (digitalRead(upPin) == LOW) {
                if (time_selected == 0) wake_time_hours++;
                else wake_time_minutes++;
            }

            if (wake_time_hours > 23) wake_time_hours = 0;
            if (wake_time_hours < 0) wake_time_hours = 23;
            if (wake_time_minutes > 59) wake_time_minutes = 0;
            if (wake_time_minutes < 0) wake_time_minutes = 59;

            if (digitalRead(enterPin) == LOW) {
                time_selected++;
                if (time_selected > 1) time_selected = 0;
            }

            drawAlarmTime();

            if (digitalRead(backPin) == LOW) {
                currentScreen = SETTINGS;
                lastDrawnSelection = -1;
            }
            break;
        }
        case FONT_COLOR: {
            if (digitalRead(downPin) == LOW) color_selected++;
            if (digitalRead(upPin) == LOW) color_selected--;
            if (color_selected > 5) color_selected = 0;
            if (color_selected < 0) color_selected = 5;

            if (screenChanged || color_selected != lastDrawnSelection) {
                drawFontColor();
                lastDrawnScreen = currentScreen;
                lastDrawnSelection = color_selected;
            }

            if (digitalRead(enterPin) == LOW) {
                if (color_selected == 0) text_color = 0xFFFF; // White
                if (color_selected == 1) text_color = 0xF800; // Red
                if (color_selected == 2) text_color = 0x07E0; // Green
                if (color_selected == 3) text_color = 0x001F; // Blue
                if (color_selected == 4) text_color = 0xFFE0; // Yellow
                if (color_selected == 5) text_color = 0x07FF; // Cyan
                lastDrawnSelection = -1; // force redraw so preview updates immediately
            }

            if (digitalRead(backPin) == LOW) {
                currentScreen = SETTINGS;
                lastDrawnSelection = -1;
            }
            break;
            }
    }
    delay(150);
}

void drawTime() {
    tft.fillScreen(bg_color);

    // Time
    tft.setTextColor(text_color);
    tft.setTextSize(6);
    tft.setTextWrap(false);
    tft.setCursor(55, 17);

    char buf[6];
    sprintf(buf, "%02d:%02d", current_hour, current_minute);
    tft.print(buf);
}

void getTime() {
    
}

void playMelody(int notes[], int durations[], int length) {
  for (int i = 0; i < length; i++) {
    tone(buzzerPin, notes[i], durations[i]);
    delay(durations[i] * 1.3); // gap so notes don't blur together
    noTone(buzzerPin);
  }
}

void handleRingingAlarm() {
    if (sounds_selected == 0) playMelody(furEliseNotes, furEliseDurations, furEliseLength);
    if (sounds_selected == 1) playMelody(reveilleNotes, reveilleDurations, reveilleLength);
    if (sounds_selected == 2) playMelody(mountainKingNotes, mountainKingDurations, mountainKingLength);

    unsigned long waitStart = millis();
    while (millis() - waitStart < 5000) {
        if (digitalRead(enterPin) == LOW) {
            unsigned long now = millis();

            if (now - lastSnoozePress < 500) {
                alarmRinging = false;
                lastSnoozePress = 0;
                return;
            }

            lastSnoozePress = now;
            delay(200);
        }
    }
    if (lastSnoozePress != 0) {
        snoozeUntil = millis() + (snoozeMinutes * 60000UL);
        alarmRinging = false;
        lastSnoozePress = 0;
    }
    else {

    }
}

void drawSettings() {
    tft.fillScreen(bg_color);
    tft.setTextColor(text_color);
    tft.setTextSize(2);
    tft.setTextWrap(false);

    tft.setCursor(71, 6);
    tft.print(settings_selected == 0 ? "> Alarm time" : "  Alarm time");

    tft.setCursor(71, 30);
    tft.print(settings_selected == 1 ? "> Alarm sound" : "  Alarm sound");

    tft.setCursor(71, 54);
    tft.print(settings_selected == 2 ? "> Font color" : "  Font color");
}

void drawAlarmTime() {
  tft.fillScreen(bg_color);
  tft.setTextColor(text_color);
  tft.setTextSize(6);
  tft.setTextWrap(false);
  tft.setCursor(55, 17);

  char buf[6];
  sprintf(buf, "%02d:%02d", wake_time_hours, wake_time_minutes);
  tft.print(buf);

  tft.setTextSize(2);
  tft.setCursor(55, 60);
  tft.print(time_selected == 0 ? "^^^" : "   ^^^");
}

void drawFontColor() {
  tft.fillScreen(bg_color);
  tft.setTextColor(text_color);
  tft.setTextSize(2);
  tft.setTextWrap(false);

  for (int i = 0; i < 6; i++) {
    tft.setCursor(colorX[i], colorY[i]);
    tft.print(color_selected == i ? "> " : "  ");
    tft.print(colorNames[i]);
  }
}

void drawAlarmSound() {
    tft.fillScreen(bg_color);
    tft.setTextColor(text_color);
    tft.setTextSize(2);
    tft.setTextWrap(false);

    tft.setCursor(71, 6);
    tft.print(sounds_selected == 0 ? "> Fur Elise" : "  Fur Elise");

    tft.setCursor(71, 30);
    tft.print(sounds_selected == 1 ? "> Reveille" : "  Reveille");

    tft.setCursor(71, 54);
    tft.print(sounds_selected == 2 ? "> Mountain King" : "  Mountain King");
}