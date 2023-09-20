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
#define EEPROM_CRC_ADDRESS 20
#define TWELVE_HOUR_MODE_ADDRESS 1
#define ROTATE_TIME_ADDRESS 2
#define TRANS_MODE_ADDRESS 3
#define ON_HOUR_ADDRESS 4
#define OFF_HOUR_ADDRESS 5
#define POISON_TIME_SPAN_ADDRESS 6
#define POISON_TIME_START_ADDRESS 7

#define INS1_LATCH_PIN 16 // u2_rxd
#define INS1_DATA_PIN 18
#define INS1_CLK_PIN 17  // u2_txd
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
float currentTemp = 0, currentPOP = 0, tmrwDayTemp = 0, tmrwPOP = 0;
uint16_t currentCode = 0, tomorrowCode = 0;
uint16_t currentID = 0, tmrwID = 0;

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
boolean currentMatrixDisplayMode = 0; // dynamic or static
boolean currentMatrixDisplayTime = 0; // tmrw or now
uint8_t wifiStatusLED = 0;            // 0=off, 1=on, 2=blinking

uint8_t dateTimeDisplayRotateSpeed = EEPROM.read(ROTATE_TIME_ADDRESS); // time in second, min 4, max 59
boolean twentyFourHourMode = EEPROM.read(TWELVE_HOUR_MODE_ADDRESS);    // 0 for 12, 1 for 24
uint8_t ClockTransitionMode = EEPROM.read(TRANS_MODE_ADDRESS);
uint8_t displayOnHour = EEPROM.read(ON_HOUR_ADDRESS);
uint8_t displayOffHour = EEPROM.read(OFF_HOUR_ADDRESS);
uint8_t poisonTimeSpan = EEPROM.read(POISON_TIME_SPAN_ADDRESS);
uint8_t poisonTimeStart = EEPROM.read(POISON_TIME_START_ADDRESS);

// FUNCTIONS
int readRotEncoder(int counter);
boolean readButton(uint8_t pin);
void userInputClock(uint8_t digits[3], uint8_t minValues[3], uint8_t maxValues[3], uint8_t colons);
void setAntiPoisonMinute();
void setTransitionMode();
void setHourDisplayMode();
void setOnOffTime();
int changeMode(int mode, int numOfModes);
void updateDateTime();
void displayTime();
void displayDate();
void updateWeather();
void displayVFDWeather();
void displayMatrixWeather();
String httpGETRequest(const char *serverName);
boolean isBetweenHours(int hour, int displayOffHour, int displayOn);

void setup()
{
  delay(500);
  Serial.begin(115200);
  Serial.println("ON");

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
  nextPoisonRunMinute = poisonTimeStart + poisonTimeSpan;

  // connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    ivtubes.setScrollingString("ПОДКЛЮЧЕНИЕ К Wi-Fi", 300);
    ivtubes.scrollString();
    delay(300);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");

  // init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  updateDateTime();
  nextMinuteToUpdateWeather = ((minute / 10) * 10) + 10;
  updateWeather();
}

void loop()
{
  displayVFDWeather();
  delay(10000);

  if (1 > 2) // digitalRead(ON_OFF_SW_PIN)) // HIGH, switch is off, in auto position
  {
    if (isBetweenHours(hour, displayOffHour, displayOnHour) && !displayOff) // turn off supply
    {
      digitalWrite(SHUTDOWN_SUPPLY_PIN, LOW);
      displayOff = true;

      // Configure wakeup time to wake up at displayOnHour
      uint64_t wakeup_interval_us = ((displayOnHour - hour + 24) % 24) * 3600 * 1000000; // Calculate the time until displayOnHour
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
  updateDateTime(); // update the time vars
  switch (timeDateDisplayMode)
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
      nextSecondToChangeDateTimeModes = ((second + dateTimeDisplayRotateSpeed) % 60);
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
    nextPoisonRunMinute = (nextPoisonRunMinute + poisonTimeSpan) % 60;
    Nixies.antiPoison();
    // matrix.antiPoison();
  }
  // weather section
  if (minute == nextMinuteToUpdateWeather)
  {
    nextMinuteToUpdateWeather = (minute + weatherCheckFreqMin) % 60;
    Serial.println("UPDATE WEATHER");
    updateWeather();
    displayVFDWeather();
    displayMatrixWeather();
  }
  // VFD section
  if (millis() > userNotifyTimer + 1000)
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
  if (2 < 1) // readButton(VFD_MODE_BTN_PIN)) //DISABLE FOR NOW SINCE HARDWARE PULL-UP
  {
    Serial.println("REGIME");
    vfdDisplayMode = (vfdDisplayMode + 1) % 3;
    switch (vfdDisplayMode)
    {
    case 0:
      ivtubes.shiftOutString("Today");
      break;
    case 1:
      ivtubes.shiftOutString("tmrw");
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
    currentMatrixDisplayMode != currentMatrixDisplayMode;
    displayMatrixWeather();
  }
  if (digitalRead(NOW_TMRW_SW_PIN) != currentMatrixDisplayTime)
  {
    currentMatrixDisplayTime != currentMatrixDisplayTime;
    displayMatrixWeather();
  }

  // matrix.animateDisplay();
  // printLocalTime();
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

  // return
}

void setAntiPoisonMinute()
{
  ivtubes.setScrollingString("Anti Poison Timer", 300); 
  uint8_t digits[3] = {poisonTimeStart, 255, poisonTimeSpan};
  uint8_t minValues[3] = {0, 0, 5};
  uint8_t maxValues[3] = {59, 0, 45};
  userInputClock(digits, minValues, maxValues, ALLOFF);
  EEPROM.write(POISON_TIME_START_ADDRESS, digits[0]); // commit change to EEPROM
  EEPROM.write(POISON_TIME_SPAN_ADDRESS, digits[2]);  // commit change to EEPROM
}
void setTransitionMode()
{
  ivtubes.setScrollingString("Nixie Transition Mode", 300); 
  uint8_t digits[3] = {255, 255, ClockTransitionMode};
  uint8_t minValues[3] = {0, 0, 0};
  uint8_t maxValues[3] = {0, 0, 1};
  userInputClock(digits, minValues, maxValues, ALLOFF);
  EEPROM.write(TRANS_MODE_ADDRESS, digits[2]); // commit change to EEPROM
}
void setRotationSpeed()
{
  ivtubes.setScrollingString("Date/Time display rotate speed", 300);
  uint8_t digits[3] = {255, 255, dateTimeDisplayRotateSpeed};
  uint8_t minValues[3] = {0, 0, 4};
  uint8_t maxValues[3] = {0, 0, 59};
  userInputClock(digits, minValues, maxValues, ALLOFF);
  EEPROM.write(ROTATE_TIME_ADDRESS, digits[2]); // commit change to EEPROM
}
void setHourDisplayMode()
{
  ivtubes.setScrollingString("12/24 hour mode", 300);
  uint8_t digits[3] = {255, 255, twentyFourHourMode};
  uint8_t minValues[3] = {0, 0, 0};
  uint8_t maxValues[3] = {0, 0, 1};
  userInputClock(digits, minValues, maxValues, ALLOFF);
  EEPROM.write(TWELVE_HOUR_MODE_ADDRESS, (boolean)digits[2]); // commmit change to EEPROM
}
void setOnOffTime()
{
  ivtubes.setScrollingString("On Hour / Off Hour", 300);
  uint8_t digits[3] = {displayOnHour, 255, displayOffHour};
  uint8_t minValues[3] = {0, 0, 0};
  uint8_t maxValues[3] = {23, 0, 23};
  userInputClock(digits, minValues, maxValues, ALLOFF);
  if (digits[2] == digits[0])
  {
    ivtubes.setScrollingString("CANNOT SET ON AND OFF TIME TO BE THE SAME", 300);
    ivtubes.scrollStringSync();
    return;
  }
  EEPROM.write(OFF_HOUR_ADDRESS, digits[2]); // commit change to EEPROM
  EEPROM.write(ON_HOUR_ADDRESS, digits[0]);  // commit change to EEPROM
}
int changeMode(int mode, int numOfModes) // numofmodes starts at 0! display mode on tube, easy for selecting
{
  // this can be shortened
  int lastmode = mode;
  mode = readRotEncoder(mode);
  if (mode > numOfModes) // if mode invalid, loop around
  {
    mode = 0;
  }
  else if (mode < 0)
  {
    mode = numOfModes;
  }
  if (mode != lastmode)
  {
    unsigned long waitForDisplay = millis();
    while (millis() - waitForDisplay < 500)
    {
      delay(5);
      Nixies.writeToNixie(mode, 255, 255, 0);
      mode = changeMode(mode, numOfModes);
    }
  }
  return mode;
}
void updateDateTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    ivtubes.setScrollingString("НЕ МОГУ ПОЛУЧИТЬ ВРЕМЯ", 300);
    ivtubes.scrollStringSync();
    return;
  }
  hour = timeinfo.tm_hour;
  minute = timeinfo.tm_min;
  second = timeinfo.tm_sec;
  year = timeinfo.tm_year + 1900;
  month = timeinfo.tm_mon + 1;
  day = timeinfo.tm_mday;

  Serial.printf("%02d/%02d/%d %02d:%02d:%02d\n", month, day, year, hour, minute, second); // debug
}
void displayTime()
{
  if (ClockTransitionMode)
  {
    Nixies.writeToNixieScroll((twentyFourHourMode ? (hour > 12 ? hour - 12 : (hour == 00 ? 12 : hour)) : hour), minute, second, (second % 2 ? ALLOFF : ALLON));
  }
  else
  {
    Nixies.writeToNixie((twentyFourHourMode ? (hour > 12 ? hour - 12 : (hour == 00 ? 12 : hour)) : hour), minute, second, (second % 2 ? ALLOFF : ALLON));
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
    filter_current["weather"][0]["id"] = true;
    filter["hourly"][0]["pop"] = true;

    JsonObject filter_daily_0 = filter["daily"].createNestedObject();
    filter_daily_0["pop"] = true;

    JsonObject filter_daily_0_temp = filter_daily_0.createNestedObject("temp");
    filter_daily_0_temp["day"] = true;
    filter_daily_0_temp["night"] = true;
    filter_daily_0["weather"][0]["id"] = true;

    DynamicJsonDocument doc(3072);

    DeserializationError error = deserializeJson(doc, httpGETRequest(apiCallURL.c_str()), DeserializationOption::Filter(filter));

    if (error)
    {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    // current
    currentTemp = doc["current"]["temp"]; // 65.62
    currentID = doc["current"]["weather"][0]["id"];
    // currentPOP = minute < 15 ? doc["hourly"][0]["pop"] : doc["hourly"][1]["pop"]; // if 15 min past hour, then display pop for next hour
    currentPOP = doc["daily"][0]["pop"];
    // might need to just get pop from the daily forcast. this looks like itll be wrong.
    //  tomorrow
    tmrwDayTemp = doc["daily"][1]["temp"]["day"];
    tmrwPOP = doc["daily"][1]["pop"];
    tmrwID = doc["daily"][1]["weather"][0]["id"];
  }
  else
  {
    Serial.println("WiFi Disconnected");
  }
}
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
void displayVFDWeather()
{
  char *weatherVFD = (char *)malloc(7);
  // todo, maybe round

  if (vfdCurrentDisplay)
  {
    snprintf(weatherVFD, 7, "%02d\037%02d%%", int(currentTemp), int(currentPOP * 100));
    digitalWrite(NOW_LED_PIN, HIGH);
    digitalWrite(TMRW_LED_PIN, LOW);
  }
  else
  {
    snprintf(weatherVFD, 7, "%02d\037%02d%%", int(tmrwDayTemp), int(tmrwPOP * 100));
    digitalWrite(NOW_LED_PIN, LOW);
    digitalWrite(TMRW_LED_PIN, HIGH);
  }
  ivtubes.shiftOutString(weatherVFD);

  Serial.println(currentTemp);
  Serial.println(currentPOP);
  Serial.println(currentID);
  Serial.println(tmrwDayTemp);
  Serial.println(tmrwPOP);
  Serial.println(tmrwID);
  Serial.println(weatherVFD);
  free(weatherVFD);
}
void displayMatrixWeather()
{
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