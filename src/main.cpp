#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <time.h>

#include <INS1Matrix.h>
#include <IV17.h>
#include <NixieBoard.h>
#include <secrets.h>
#include <icons.h>

// EEPROM ADDRESSES
// EEPROM ADDRESSES
#define EEPROM_CRC_ADDRESS 0
#define TWELVE_HOUR_MODE_ADDRESS 4
#define ROTATE_TIME_ADDRESS (TWELVE_HOUR_MODE_ADDRESS + sizeof(uint8_t))
#define TRANS_MODE_ADDRESS (ROTATE_TIME_ADDRESS + sizeof(uint8_t))
#define ON_HOUR_ADDRESS (TRANS_MODE_ADDRESS + sizeof(uint8_t))
#define OFF_HOUR_ADDRESS (ON_HOUR_ADDRESS + sizeof(uint8_t))
#define POISON_TIME_SPAN_ADDRESS (OFF_HOUR_ADDRESS + sizeof(uint8_t))
#define POISON_TIME_START_ADDRESS (POISON_TIME_SPAN_ADDRESS + sizeof(uint8_t))

#define INS1_LATCH_PIN 16 // u2_rxd
#define INS1_DATA_PIN 18
#define INS1_CLK_PIN 17 // u2_txd
#define INS1_BLNK_PIN 19

#define IV17_DATA_PIN 26
#define IV17_STRB_PIN 25
#define IV17_CLK_PIN 32
#define IV17_BLNK_PIN 33

#define IN12_LATCH_PIN 23 // clk
#define IN12_DATA_PIN 21
#define IN12_CLK_PIN 22 // srclk

// the rotatry encoder has built in pull ups. Can use the input only pins for those
#define ROTCLK_PIN 36 // VP
#define ROTDT_PIN 39  // VN
#define ROTBTTN_PIN 34
#define SHUTDOWN_SUPPLY_PIN 4 // set HIGH to enable supplies. LOW to turn them off

#define NOW_TMRW_SW_PIN 5
#define DYN_STAT_SW_PIN 13
#define ON_OFF_SW_PIN 27

#define VFD_MODE_BTN_PIN 35 // value may change
#define NOW_LED_PIN 12
#define TMRW_LED_PIN 14
#define WIFI_LED_PIN 2

#define INS1_DISPLAYS 2
#define IV17_DISPLAYS 6
/*FREE:
15 -PWM at boot, Strapping |
//taken
2 -LED OUTPUT only
35 INPUT ONLY - REGIME
14 -PWM at boot | LED OUTPUT
12 - boot fails if pulled high, strapping pin
27
13
5 -PWM at boot, Strapping | Maybe an input for button
*/

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *apiKey = NEW_WEATHER_API;
const char *openWeatherMapApiKey = WEATHER_API;
const char *lat = WEATHER_LAT;
const char *lon = WEATHER_LON;
const char *zipCode = ZIP_CODE;
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;
const int daylightOffset_sec = 3600;
String apiCallURL = "http://api.openweathermap.org/data/3.0/onecall?exclude=minutely,alerts&units=imperial&lat=" + String(lat) + "&lon=" + String(lon) + "&appid=" + String(openWeatherMapApiKey);
String forecastURL = "http://api.weatherapi.com/v1/forecast.json?key=" + String(apiKey) + "&q=" + String(zipCode) + "&days=2&aqi=no&alerts=no";

// Setup Devices
INS1Matrix matrix = INS1Matrix(INS1_DATA_PIN, INS1_CLK_PIN, INS1_LATCH_PIN, INS1_BLNK_PIN, true, INS1_DISPLAYS);
IV17 ivtubes = IV17(IV17_DATA_PIN, IV17_CLK_PIN, IV17_STRB_PIN, IV17_BLNK_PIN, IV17_DISPLAYS);
NixieBoard Nixies = NixieBoard(IN12_DATA_PIN, IN12_CLK_PIN, IN12_LATCH_PIN);
// Global Vars
// Rotary Encoder
uint8_t currentStateCLK = 0, lastStateCLK = 0;
unsigned long lastButtonPress = 0;
// DateTime Vars
uint8_t hour = 0, minute = 0, second = 0, month = 0, day = 0;
int year = 0;
// Weather Vars
struct Weather
{
  float currentTemp = 0;
  float currentPOP = 0;
  float tmrwDayTemp = 0;
  float tmrwPOP = 0;
  char currentIcon[4];
  char tmrwIcon[4];
};
Weather weather;
// Timer Vars
uint16_t userInputBlinkTime = 500;
uint8_t weatherCheckFreqMin = 10; // consider replacing with #defines since some of these are static
uint8_t nextMinuteToUpdateWeather = 0;
uint8_t lastWeatherCheckMin = 0; // Last Minute weather was checked
uint8_t nextSecondToChangeDateTimeModes = 0;
uint8_t nextSecondToChangeVFDMode = 0;
unsigned long userNotifyTimer = 0;
// Mode Vars
uint8_t timeDateDisplayMode = 0;      // display time, date, or rotate between them
uint8_t nextPoisonRunMinute = 0;      // next minute that antipoison will run
boolean displayDateOrTime = 0;        // are we displaying the date or time
boolean vfdCurrentDisplay = 0;        // displaying weather now, or tomorrow's forecast
uint8_t vfdDisplayMode = 0;           // rotate, static now, static tomorrow
boolean displayOff = false;           // are the power supplies turned off?
boolean currentMatrixDisplayMode = 1; // dynamic or static. 1= dyn
boolean currentMatrixDisplayTime = 1; // tmrw or now. 1 = now
uint8_t wifiStatusLED = 0;            // 0=off, 1=on, 2=blinking
// Define a structure to hold your user-defined values
struct DeviceSettings
{
  bool twentyFourHourMode;
  uint8_t dateTimeDisplayRotateSpeed;
  uint8_t ClockTransitionMode;
  uint8_t displayOnHour;
  uint8_t displayOffHour;
  uint8_t poisonTimeSpan;
  uint8_t poisonTimeStart;
};
DeviceSettings settings;

// FUNCTIONS
void settingsMenu();
void newSettingsMenu(DeviceSettings &settings);
int readRotEncoder(int counter);
boolean readButton(uint8_t pin);
void userInputClock(uint8_t digits[3], uint8_t minValues[3], uint8_t maxValues[3], uint8_t colons);
void setAntiPoisonMinute();
void setRotationSpeed();
void setTransitionMode();
void setHourDisplayMode();
void setOnOffTime();
int changeMode(int mode, int numOfModes);
void updateDateTime();
void displayTime();
void displayDate();
void updateWeather();
const uint32_t *selectIcon(const char *iconStr);
void displayVFDWeather();
void setMatrixWeatherDisplay();
void displayMatrixWeather();
String httpGETRequest(const char *serverName);
boolean isBetweenHours(int hour, int displayOffHour, int displayOn);
uint32_t calculateCRC(const DeviceSettings &settings);
bool readEEPROMWithCRC(DeviceSettings &settings);
void writeEEPROMWithCRC(const DeviceSettings &settings);
void initWiFi();

void setup()
{
  delay(500);
  Serial.begin(115200);
  Serial.println("ON");

  EEPROM.begin(128);
  bool settingsValid = readEEPROMWithCRC(settings);
  if (!settingsValid)
  {
    Serial.println("CRC BAD");
    settings.displayOffHour = 0;
    settings.displayOnHour = 5;
    settings.poisonTimeSpan = 10;
    settings.poisonTimeStart = 10;
    settings.dateTimeDisplayRotateSpeed = 15;
    settings.ClockTransitionMode = 1;
    settings.twentyFourHourMode = 1;
    writeEEPROMWithCRC(settings);

    EEPROM.commit();
  }
  else
  {
    Serial.println("CRC GOOD");
  }

  pinMode(SHUTDOWN_SUPPLY_PIN, OUTPUT);
  pinMode(ROTCLK_PIN, INPUT);
  pinMode(ROTDT_PIN, INPUT);
  pinMode(ROTBTTN_PIN, INPUT);

  pinMode(NOW_TMRW_SW_PIN, INPUT_PULLUP);
  pinMode(DYN_STAT_SW_PIN, INPUT_PULLUP);
  pinMode(ON_OFF_SW_PIN, INPUT_PULLUP);

  pinMode(VFD_MODE_BTN_PIN, INPUT);
  pinMode(NOW_LED_PIN, OUTPUT);
  pinMode(TMRW_LED_PIN, OUTPUT);
  pinMode(WIFI_LED_PIN, OUTPUT);

  digitalWrite(SHUTDOWN_SUPPLY_PIN, HIGH); // turn on power
  currentStateCLK = digitalRead(ROTCLK_PIN);
  lastStateCLK = currentStateCLK;

  nextPoisonRunMinute = settings.poisonTimeStart + settings.poisonTimeSpan;
  Nixies.writeToNixie(255, 255, 255, 0);
  // connect to WiFi
  initWiFi();
  // init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  updateDateTime();
  nextMinuteToUpdateWeather = ((minute / 10) * 10) + 10;
  updateWeather();
  matrix.setAnimationToDisplay(testAnimation);
  matrix.writeStaticImgToDisplay(allOn);
  // analogWrite(INS1_BLNK_PIN, 50); //need to fix board polairity
  setCpuFrequencyMhz(80); // slow down for power savings
  delay(1000);
}

void loop()
{
  // displayVFDWeather();
  // delay(5);

  if (digitalRead(ON_OFF_SW_PIN)) // HIGH, switch is off, in auto position
  {
    if (isBetweenHours(hour, settings.displayOffHour, settings.displayOnHour) && !displayOff) // turn off supply
    {
      digitalWrite(SHUTDOWN_SUPPLY_PIN, LOW);
      displayOff = true;
      Serial.println("SLEEPING");
      delay(1000);
      // Configure wakeup time to wake up at displayOnHour
      uint64_t wakeup_interval_us = ((((settings.displayOnHour - hour + 24) % 24) * 3600) - (minute * 60) - second) * 1000000; // Calculate the time until displayOnHour
      Serial.println(wakeup_interval_us / 1000000);
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_27, LOW);
      esp_sleep_enable_timer_wakeup(wakeup_interval_us);

      // Enter deep sleep mode
      esp_deep_sleep_start();
    }
    else if (displayOff)
    {
      displayOff = false;
      digitalWrite(SHUTDOWN_SUPPLY_PIN, HIGH);
    }
    if (displayOff)
      delay(10000); // no need to run through the loop while display is turned off
  }
  else if (displayOff)
  {
    displayOff = false;
    digitalWrite(SHUTDOWN_SUPPLY_PIN, HIGH);
  }
  // NixieTube section
  timeDateDisplayMode = changeMode(timeDateDisplayMode, 2);
  updateDateTime();            // update the time vars
  switch (timeDateDisplayMode) // rotate, or just display date or time
  {
  case 0:
    displayTime();
    break;
  case 1:
    displayDate();
    break;
  case 2:
    if (second == nextSecondToChangeDateTimeModes)
    {
      nextSecondToChangeDateTimeModes = ((second + settings.dateTimeDisplayRotateSpeed) % 60);
      displayDateOrTime != displayDateOrTime;
    }
    if (displayDateOrTime)
      displayTime();
    else
      displayDate();
    break;
  }
  if (minute == nextPoisonRunMinute)
  {
    nextPoisonRunMinute = (nextPoisonRunMinute + settings.poisonTimeSpan) % 60;
    Nixies.antiPoison();
    // matrix.antiPoison();
  }
  // weather section
  if (minute == nextMinuteToUpdateWeather) // update the weather forcast
  {
    nextMinuteToUpdateWeather = (minute + weatherCheckFreqMin) % 60;
    Serial.println("UPDATE WEATHER");
    updateWeather();
    displayVFDWeather(); // only callled when we update the weather. Otherwise, its static
    setMatrixWeatherDisplay();
  }
  // VFD section
  if (millis() > userNotifyTimer + 1000) // allow time for VFD to notify user, then redisplay the weather
  {
    Serial.println("usernotif");
    Serial.println(userNotifyTimer);
    Serial.println(millis());
    userNotifyTimer = UINT32_MAX - 1001; // set to max value
    Serial.println(userNotifyTimer);
    displayVFDWeather();
  }
  if (vfdDisplayMode == 2) // if in rotate mode, do the rotation. otherwise we update it when we refresh the forecast
  {
    if (second == nextSecondToChangeVFDMode)
    {
      nextSecondToChangeVFDMode = (nextSecondToChangeVFDMode + 15) % 60;
      vfdCurrentDisplay = !vfdCurrentDisplay;
      if (vfdCurrentDisplay)
      {
        digitalWrite(NOW_LED_PIN, HIGH);
        digitalWrite(TMRW_LED_PIN, LOW);
      }
      else
      {
        digitalWrite(NOW_LED_PIN, LOW);
        digitalWrite(TMRW_LED_PIN, HIGH);
      }
      displayVFDWeather();
    }
  }
  if (readButton(VFD_MODE_BTN_PIN)) // change VFD display mode
  {
    Serial.println("REGIME");
    vfdDisplayMode = (vfdDisplayMode + 1) % 3;
    switch (vfdDisplayMode)
    {
    case 0:
      ivtubes.shiftOutString("Today");
      digitalWrite(NOW_LED_PIN, HIGH);
      digitalWrite(TMRW_LED_PIN, LOW);
      break;
    case 1:
      ivtubes.shiftOutString("tmrw");
      digitalWrite(NOW_LED_PIN, LOW);
      digitalWrite(TMRW_LED_PIN, HIGH);
      break;
    case 2:
      nextSecondToChangeVFDMode = 0;
      ivtubes.shiftOutString("DYN");
    }
    userNotifyTimer = millis();
  }

  // matrix section
  if (digitalRead(DYN_STAT_SW_PIN) != currentMatrixDisplayMode)
  {
    // Switch off = high = animate
    currentMatrixDisplayMode = !currentMatrixDisplayMode;
    Serial.println("Set1");
    setMatrixWeatherDisplay();
  }
  if (digitalRead(NOW_TMRW_SW_PIN) != currentMatrixDisplayTime)
  {
    // switch off = high = now
    currentMatrixDisplayTime = !currentMatrixDisplayTime;
    Serial.println("Set2");
    setMatrixWeatherDisplay();
  }

  displayMatrixWeather();
  // printLocalTime();
  // Settings section
  if (readButton(ROTBTTN_PIN))
  {
    Serial.println("BUTTON");
    settingsMenu();
  }
}
void settingsMenu()
{

  int settingsMode = 0;

  while (1)
  {

    delay(5);
    settingsMode = changeMode(settingsMode, 5);
    switch (settingsMode)
    {
    case 0: // exit, just have number
      ivtubes.setScrollingString("Exit", 150);
      Nixies.writeToNixie(settingsMode, 255, 255, 0);
      break;
    case 1: // set date time rotation speed
      ivtubes.setScrollingString("Date/Time display rotate speed", 150);
      Nixies.writeToNixie(settingsMode, 255, settings.dateTimeDisplayRotateSpeed, 0);
      break;
    case 2: // setHourDisplay
      ivtubes.setScrollingString("12/24 hour mode", 150);
      Nixies.writeToNixie(255, 255, (settings.twentyFourHourMode ? 24 : 12), 0);
      break;
    case 3: // set setTransitionMode
      ivtubes.setScrollingString("Nixie Transition Mode", 150);
      Nixies.writeToNixie(255, 255, settings.ClockTransitionMode, 0);
      break;
    case 4: // set on off time for display
      ivtubes.setScrollingString("On Hour / Off Hour", 150);
      Nixies.writeToNixie(settings.displayOffHour, 255, settings.displayOnHour, 8);
      break;
    case 5:
      ivtubes.setScrollingString("Anti Poison Timer", 150);
      Nixies.writeToNixie(settings.poisonTimeStart, 255, settings.poisonTimeSpan, 8);
      break;
    }
    int lastSettingsMode = settingsMode;
    while (settingsMode = lastSettingsMode)
    {
      settingsMode = changeMode(settingsMode, 5);

      ivtubes.scrollString();
      if (readButton(ROTBTTN_PIN))
      {
        switch (settingsMode)
        {
        case 0:
          return;
        case 1:
          setRotationSpeed();
          break;
        case 2:
          setHourDisplayMode();
          break;
        case 3:
          setTransitionMode();
          break;
        case 4:
          setOnOffTime();
          break;
        case 5:
          setAntiPoisonMinute();
          break;
        }
      }
    }
  }
}
int readRotEncoder(int counter) // takes a counter in and spits it back out if it changes
{
  // Read the current state of CLK
  currentStateCLK = digitalRead(ROTCLK_PIN);
  // If last and current state of CLK are different, then pulse occurred
  // React to only 1 state change to avoid double count
  if (currentStateCLK != lastStateCLK && currentStateCLK == 1)
  {
    // the encoder is rotating CCW so decrement
    if (digitalRead(ROTDT_PIN) != currentStateCLK)
    {
      counter--;
    }
    else
    {
      // Encoder is rotating CW so increment
      counter++;
    }
  }
  // Remember last CLK state
  lastStateCLK = currentStateCLK;
  return counter;
}
boolean readButton(uint8_t pin) // true if button pressed
{
  // Read the button state
  uint8_t btnState = digitalRead(pin);
  // If we detect LOW signal, button is pressed
  if (btnState == LOW)
  {
    // if 50ms have passed since last LOW pulse, it means that the
    // button has been pressed, released and pressed again
    if (millis() - lastButtonPress > 50)
    {
      lastButtonPress = millis();
      return true;
    }
    else
    {
      lastButtonPress = millis();
      return false;
    }
    // Remember last button press event
  }
  else
  {
    return false;
  }
}
void userInputClock(uint8_t digits[3], uint8_t minValues[3], uint8_t maxValues[3], uint8_t colons) // digits[3] = hour, minute, second
{
  uint8_t tempDigits[3]; // hour, mins, secs
  memcpy(tempDigits, digits, sizeof(uint8_t) * 3);
  uint8_t digitMod = 0;
  uint32_t lastBlinkTime = millis();

  for (int i = 0; i < 3; i++)
  {
    if (digits[i] != 255)
    {
      while (digitMod < 3)
      {
        memcpy(tempDigits, digits, sizeof(uint8_t) * 3);
        delay(3);
        ivtubes.scrollString();
        if (readButton(ROTBTTN_PIN))
        {
          lastBlinkTime = millis(); // reset timer
          digitMod++;
          break;
        }
        if ((millis() - lastBlinkTime) < userInputBlinkTime)
        {
          Nixies.writeToNixie(digits[0], digits[1], digits[2], colons);
        }
        else if (((millis() - lastBlinkTime) > userInputBlinkTime * 2))
        {
          Nixies.writeToNixie(digits[0], digits[1], digits[2], colons);
          lastBlinkTime = millis(); // reset timer
        }
        else
        {
          // switch off
          switch (digitMod)
          {
          case 0:
            Nixies.writeToNixie(255, digits[1], digits[2], colons);
            break;
          case 1:
            Nixies.writeToNixie(digits[0], 255, digits[2], colons);
            break;
          case 2:
            Nixies.writeToNixie(digits[0], digits[1], 255, colons);
            break;
          }
        }
      }
      digits[digitMod] = readRotEncoder(digits[digitMod]);
      if (digits[digitMod] != tempDigits[digitMod])
      {
        lastBlinkTime = millis();
        if (digits[digitMod] > maxValues[digitMod])
        {
          digits[digitMod] = minValues[digitMod];
        }
        else if (digits[digitMod] < minValues[digitMod])
        {
          digits[digitMod] = maxValues[digitMod];
        }
      }
    }
    else
    {
      digitMod++;
    }
  }
}

void setAntiPoisonMinute()
{
  ivtubes.setScrollingString("Anti Poison Timer", 150);
  uint8_t digits[3] = {settings.poisonTimeStart, 255, settings.poisonTimeSpan};
  uint8_t minValues[3] = {0, 0, 5};
  uint8_t maxValues[3] = {59, 0, 45};
  userInputClock(digits, minValues, maxValues, ALLOFF);
  settings.poisonTimeStart = digits[0];
  settings.poisonTimeSpan = digits[2];
  writeEEPROMWithCRC(settings);
}
void setTransitionMode()
{
  ivtubes.setScrollingString("Nixie Transition Mode", 150);
  uint8_t digits[3] = {255, 255, settings.ClockTransitionMode};
  uint8_t minValues[3] = {0, 0, 0};
  uint8_t maxValues[3] = {0, 0, 1};
  userInputClock(digits, minValues, maxValues, ALLOFF);
  settings.ClockTransitionMode = digits[2];
  writeEEPROMWithCRC(settings);
}
void setRotationSpeed()
{
  ivtubes.setScrollingString("Date/Time display rotate speed", 150);
  uint8_t digits[3] = {255, 255, settings.dateTimeDisplayRotateSpeed};
  uint8_t minValues[3] = {0, 0, 4};
  uint8_t maxValues[3] = {0, 0, 59};
  userInputClock(digits, minValues, maxValues, ALLOFF);
  settings.dateTimeDisplayRotateSpeed = digits[2];
  writeEEPROMWithCRC(settings);
}
void setHourDisplayMode()
{
  ivtubes.setScrollingString("12/24 hour mode", 150);
  uint8_t digits[3] = {255, 255, settings.twentyFourHourMode};
  uint8_t minValues[3] = {0, 0, 0};
  uint8_t maxValues[3] = {0, 0, 1};
  userInputClock(digits, minValues, maxValues, ALLOFF);
  settings.twentyFourHourMode = (boolean)digits[2];
  writeEEPROMWithCRC(settings);
}
void setOnOffTime()
{
  ivtubes.setScrollingString("On Hour / Off Hour", 150);
  uint8_t digits[3] = {settings.displayOnHour, 255, settings.displayOffHour};
  uint8_t minValues[3] = {0, 0, 0};
  uint8_t maxValues[3] = {23, 0, 23};
  userInputClock(digits, minValues, maxValues, ALLOFF);
  if (digits[2] == digits[0])
  {
    ivtubes.setScrollingString("CANNOT SET ON AND OFF TIME TO BE THE SAME", 150);
    ivtubes.scrollStringSync();
    return;
  }
  EEPROM.write(OFF_HOUR_ADDRESS, digits[2]); // commit change to EEPROM
  EEPROM.write(ON_HOUR_ADDRESS, digits[0]);  // commit change to EEPROM
  writeEEPROMWithCRC(settings);
}
int changeMode(int mode, int numOfModes) // numofmodes starts at 0! display mode on tube, easy for selecting
{

  int lastmode = mode;
  mode = readRotEncoder(mode);

  // Ensure mode stays within bounds
  mode = (mode + numOfModes + 1) % (numOfModes + 1);

  if (mode != lastmode)
  {
    unsigned long waitForDisplay = millis();
    while (millis() - waitForDisplay < 500)
    {
      // delay(5);
      Nixies.writeToNixie(mode, 255, 255, ALLOFF);
      mode = changeMode(mode, numOfModes);
    }
  }
  return mode;
}
void updateDateTime()
{
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    ivtubes.setScrollingString("НЕ МОГУ ПОЛУЧИТЬ ВРЕМЯ", 100);
    ivtubes.scrollStringSync();
    // return;
  }
  hour = timeinfo.tm_hour;
  minute = timeinfo.tm_min;
  second = timeinfo.tm_sec;
  year = timeinfo.tm_year + 1900;
  month = timeinfo.tm_mon + 1;
  day = timeinfo.tm_mday;

  // Serial.printf("%02d/%02d/%d %02d:%02d:%02d\n", month, day, year, hour, minute, second); // debug
}
void displayTime()
{
  if (settings.ClockTransitionMode)
  {
    Nixies.writeToNixieScroll((settings.twentyFourHourMode ? (hour > 12 ? hour - 12 : (hour == 00 ? 12 : hour)) : hour), minute, second, (second % 2 ? ALLOFF : ALLON));
  }
  else
  {
    Nixies.writeToNixie((settings.twentyFourHourMode ? (hour > 12 ? hour - 12 : (hour == 00 ? 12 : hour)) : hour), minute, second, (second % 2 ? ALLOFF : ALLON));
  }
}
void displayDate()
{
  Nixies.writeToNixie(month, day, year - 2000, (second % 2 ? ALLOFF : 6));
}
void updateWeather() // for now, focus on openWeatherMap
{
  if (WiFi.status() == WL_CONNECTED)
  {
    // generated code below
    // Courtesy of https://arduinojson.org/v6/assistant
    // Stream& input;

    StaticJsonDocument<272> filter;

    JsonObject filter_current = filter.createNestedObject("current");
    filter_current["temp"] = true;
    filter_current["weather"][0]["icon"] = true;
    filter["hourly"][0]["pop"] = true;

    JsonObject filter_daily_0 = filter["daily"].createNestedObject();
    filter_daily_0["pop"] = true;

    JsonObject filter_daily_0_temp = filter_daily_0.createNestedObject("temp");
    filter_daily_0_temp["day"] = true;
    filter_daily_0_temp["night"] = true;
    filter_daily_0["weather"][0]["icon"] = true;

    DynamicJsonDocument doc(3072);

    DeserializationError error = deserializeJson(doc, httpGETRequest(apiCallURL.c_str()), DeserializationOption::Filter(filter));

    if (error)
    {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    // current
    weather.currentTemp = doc["current"]["temp"]; // 65.62
    const char *currIcon = doc["current"]["weather"][0]["icon"];
    // currentPOP = minute < 15 ? doc["hourly"][0]["pop"] : doc["hourly"][1]["pop"]; // if 15 min past hour, then display pop for next hour
    weather.currentPOP = doc["daily"][0]["pop"];
    // might need to just get pop from the daily forcast. this looks like itll be wrong.
    //  tomorrow
    weather.tmrwDayTemp = doc["daily"][1]["temp"]["day"];
    weather.tmrwPOP = doc["daily"][1]["pop"];
    const char *tmrwIcon = doc["daily"][1]["weather"][0]["icon"];
    // copy to struct
    strcpy(weather.currentIcon, currIcon);
    strcpy(weather.tmrwIcon, tmrwIcon);
  }
  else
  {
    Serial.println("WiFi Disconnected");
  }
}
const uint32_t *selectIcon(const char *iconStr)
{
  // parse icons
  bool isDay = iconStr[2] == 'd' ? true : false;

  if (strncmp(iconStr, "01", 2) == 0)
  {
    return isDay ? icon_01d : icon_01n;
  }
  else if (strncmp(iconStr, "02", 2) == 0)
  {
    return isDay ? icon_02d : icon_02n;
  }
  else if (strncmp(iconStr, "03", 2) == 0)
  {
    return isDay ? icon_03d : icon_03n;
  }
  else if (strncmp(iconStr, "04", 2) == 0)
  {
    return isDay ? icon_04d : icon_04n;
  }
  else if (strncmp(iconStr, "09", 2) == 0)
  {
    return isDay ? icon_09d : icon_09n;
  }
  else if (strncmp(iconStr, "10", 2) == 0)
  {
    return isDay ? icon_10d : icon_10n;
  }
  else if (strncmp(iconStr, "11", 2) == 0)
  {
    return isDay ? icon_11d : icon_11n;
  }
  else if (strncmp(iconStr, "13", 2) == 0)
  {
    return isDay ? icon_13d : icon_13n;
  }
  else if (strncmp(iconStr, "50", 2) == 0)
  {
    return isDay ? icon_50d : icon_50n;
  }
  return defaultIcon; // Default to a default icon if no match is found
}

/*
void updateWeather_WeatherAPI()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    DynamicJsonDocument doc(8192);

    String forecast = httpGETRequest(forecastURL.c_str()); // for some reason I cant just directly put this into the deserialize.
    // Serial.println(forecast);
    DeserializationError error = deserializeJson(doc, forecast);
    if (error)
    {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    // current
    currentTemp = doc["current"]["temp_f"];
    currentCode = doc["current"]["condition"]["code"];
    if (minute < 15)
    {
      currentPOP = doc["forecast"]["forecastday"][0]["hour"][0]["chance_of_rain"] > doc["forecast"]["forecastday"][0]["hour"][0]["chance_of_snow"] ? doc["forecast"]["forecastday"][0]["hour"][0]["chance_of_rain"] : doc["forecast"]["forecastday"][0]["hour"][0]["chance_of_snow"]; // pick snow or rain to display based on whichever is better
    }
    else
    {
      currentPOP = doc["forecast"]["forecastday"][0]["hour"][1]["chance_of_rain"] > doc["forecast"]["forecastday"][0]["hour"][1]["chance_of_snow"] ? doc["forecast"]["forecastday"][0]["hour"][1]["chance_of_rain"] : doc["forecast"]["forecastday"][0]["hour"][1]["chance_of_snow"]; // pick snow or rain to display based on whichever is better
    }
    // tomorrow
    tmrwDayTemp = doc["forecast"]["forecastday"][1]["day"]["maxtemp_f"];
    tomorrowCode = doc["forecast"]["forecastday"][1]["day"]["condition"]["code"];
    tmrwPOP = doc["forecast"]["forecastday"][1]["day"]["daily_chance_of_rain"] > doc["forecast"]["forecastday"][1]["day"]["daily_chance_of_snow"] ? doc["forecast"]["forecastday"][1]["day"]["daily_chance_of_rain"] : doc["forecast"]["forecastday"][1]["day"]["daily_chance_of_snow"]; // pick snow or rain to display based on whichever is better
  }
  else
  {
    Serial.println("WiFi Disconnected");
  }
}
*/
void displayVFDWeather()
{
  char *weatherVFD = (char *)malloc(7);
  // todo, maybe round

  if (vfdCurrentDisplay)
  {
    snprintf(weatherVFD, 7, "%02d\037%02d%%", int(weather.currentTemp), int(weather.currentPOP * 100));
    digitalWrite(NOW_LED_PIN, HIGH);
    digitalWrite(TMRW_LED_PIN, LOW);
  }
  else
  {
    snprintf(weatherVFD, 7, "%02d\037%02d%%", int(weather.tmrwDayTemp), int(weather.tmrwPOP * 100));
    digitalWrite(NOW_LED_PIN, LOW);
    digitalWrite(TMRW_LED_PIN, HIGH);
  }
  ivtubes.shiftOutString(weatherVFD);

  Serial.println(weather.currentTemp);
  Serial.println(weather.currentPOP);
  Serial.println(weather.currentIcon);
  Serial.println(weather.tmrwDayTemp);
  Serial.println(weather.tmrwPOP);
  Serial.println(weather.tmrwIcon);
  Serial.println(weatherVFD);
  free(weatherVFD);
}
void setMatrixWeatherDisplay()
{
  const uint32_t *iconToDisplay = selectIcon(currentMatrixDisplayTime ? weather.currentIcon : weather.tmrwIcon);
  Serial.println(currentMatrixDisplayTime);
  Serial.println(weather.tmrwIcon);
  Serial.println(weather.currentIcon);
  Serial.println(currentMatrixDisplayTime ? weather.currentIcon : weather.tmrwIcon);
  if (currentMatrixDisplayMode)
  {
    Serial.println("animate");
    matrix.setAnimationToDisplay(iconToDisplay);
  }
  else
  {
    Serial.println("static");
    matrix.writeStaticImgToDisplay(const_cast<uint32_t *>((iconToDisplay + 1))); // just display the first frame of icon
  }
}
void displayMatrixWeather()
{
  if (currentMatrixDisplayMode)
  {
    matrix.animateDisplay();
  }
}
String httpGETRequest(const char *serverName)
{
  HTTPClient http;
  // Your Domain name with URL path or IP address with path
  http.begin(serverName);
  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
  return payload;
}
bool isBetweenHours(int hour, int displayOffHour, int displayOnHour)
{
  // Check if the hour is greater than or equal to displayOffHour
  // and less than displayOnHour, taking into account the 24-hour clock.
  if (displayOffHour < displayOnHour)
  {
    return (hour >= displayOffHour && hour < displayOnHour);
  }
  else
  {
    // Handle the case when displayOnHour crosses midnight
    return (hour >= displayOffHour || hour < displayOnHour);
  }
}
uint32_t calculateCRC(const DeviceSettings &settings) // Calculate CRC32 (simple algorithm)
{
  uint32_t crc = 0;
  const uint8_t *data = reinterpret_cast<const uint8_t *>(&settings);
  int dataSize = sizeof(settings);

  for (int i = 0; i < dataSize; ++i)
  {
    crc ^= (uint32_t)data[i] << 24;
    for (int j = 0; j < 8; ++j)
    {
      if (crc & 0x80000000)
      {
        crc = (crc << 1) ^ 0x04C11DB7; // CRC-32 polynomial
      }
      else
      {
        crc <<= 1;
      }
    }
  }

  return crc;
}
bool readEEPROMWithCRC(DeviceSettings &settings) // Read EEPROM and verify CRC
{
  uint32_t storedCRC;
  EEPROM.get(EEPROM_CRC_ADDRESS, storedCRC);

  DeviceSettings tempSettings;
  EEPROM.get(TWELVE_HOUR_MODE_ADDRESS, tempSettings);

  if (storedCRC == calculateCRC(tempSettings))
  {
    settings = tempSettings;
    return true; // CRC matches, data is valid
  }
  else
  {
    return false;
  }
}
void writeEEPROMWithCRC(const DeviceSettings &settings) // Write EEPROM with CRC
{
  // Write settings to EEPROM
  EEPROM.put(TWELVE_HOUR_MODE_ADDRESS, settings);

  // Calculate CRC for the data in EEPROM
  uint32_t calculatedCRC = calculateCRC(settings);

  // Write CRC to EEPROM
  EEPROM.put(EEPROM_CRC_ADDRESS, calculatedCRC);
}
void initWiFi()
{
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  ivtubes.setScrollingString("ПОДКЛЮЧЕНИЕ К Wi-Fi", 150); // connecting to wifi
  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(WIFI_LED_PIN, !(digitalRead(WIFI_LED_PIN)));
    ivtubes.scrollString();
    delay(75);
    Serial.print(".");
  }
  digitalWrite(WIFI_LED_PIN, HIGH);
  Serial.println(" CONNECTED");
  WiFi.setSleep(true);                          // enable wifi sleep for power savings
  ivtubes.setScrollingString("ПОДКЛЮЧЕН", 150); // connected
  ivtubes.scrollStringSync();
}
