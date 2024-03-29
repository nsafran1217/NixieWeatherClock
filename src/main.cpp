#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <time.h>
#include <ESP32Ping.h>
#include <cstring>

#include <INS1Matrix.h>
#include <IV17.h>
#include <NixieBoard.h>
#include <secrets.h>
#include <icons.h>

// EEPROM ADDRESSES
#define PHONE_PING

#define EEPROM_CRC_ADDRESS 0
#define SETTINGS_ADDRESS 4

// pin definitions
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

#define SHUTDOWN_PWR_SUPPLY_PIN 4 // set HIGH to enable power supplies. LOW to turn them off

#define DYN_STAT_SW_PIN 5 // 13
#define ON_OFF_SW_PIN 27

#define WEATHER_MODE_BTN_PIN 35 // value may change
#define NOW_LED_PIN 14
#define TMRW_LED_PIN 12
#define WIFI_LED_PIN 2

#define INS1_DISPLAYS 2 // number of INS1 6x10 matrices
#define IV17_DISPLAYS 6 // number of iv17 tubes
// Status defines
#define WEATHERNOW true
#define WEATHERTMRW false
#define NIXIE_MODE_DISPLAY_TIME 0
#define NIXIE_MODE_DISPLAY_DATE 1
#define NIXIE_MODE_DISPLAY_ROTATE 2
#define NIXIE_IS_DISPLAY_TIME true
#define NIXIE_IS_DISPLAY_DATE false
#define WEATHER_DISPLAY_NOW 0
#define WEATHER_DISPLAY_TMRW 1
#define WEATHER_DISPLAY_ROTATE 2

const char *PhoneIPAddress = "10.35.0.98"; // used to detect if i am home
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *openWeatherMapApiKey = WEATHER_API;
const char *lat = WEATHER_LAT;
const char *lon = WEATHER_LON;
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;
const int daylightOffset_sec = 3600;
String apiCallURL = "http://api.openweathermap.org/data/3.0/onecall?exclude=minutely,alerts&units=imperial&lat=" + String(lat) + "&lon=" + String(lon) + "&appid=" + String(openWeatherMapApiKey);

// Setup Devices
INS1Matrix matrix = INS1Matrix(INS1_DATA_PIN, INS1_CLK_PIN, INS1_LATCH_PIN, INS1_BLNK_PIN, true, INS1_DISPLAYS);
IV17 ivtubes = IV17(IV17_DATA_PIN, IV17_CLK_PIN, IV17_STRB_PIN, IV17_BLNK_PIN, IV17_DISPLAYS);
NixieBoard Nixies = NixieBoard(IN12_DATA_PIN, IN12_CLK_PIN, IN12_LATCH_PIN);

// Mutexs
const TickType_t delay500ms = pdMS_TO_TICKS(500);
SemaphoreHandle_t nixieMutex;
SemaphoreHandle_t ivtubesMutex;
SemaphoreHandle_t weatherMutex;
SemaphoreHandle_t matrixMutex;
SemaphoreHandle_t updateWeatherTaskMutex;
// Global Vars
// Rotary Encoder
uint8_t currentStateCLK = 0, lastStateCLK = 0;
unsigned long lastButtonPress = 0;
// DateTime Vars
uint8_t hour = 0, minute = 0, second = 0, month = 0, day = 0;
int year = 0;
uint8_t lastsecond = second;
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
char weatherVFD[7]; // characters displayed on VFD
// Timer Vars
const int transDelay = 25;
const uint16_t userInputBlinkTime = 500;
const uint8_t weatherCheckFreqMin = 10; // consider replacing with #defines since some of these are static
uint8_t nextMinuteToUpdateWeather = 0;
uint8_t lastWeatherCheckMin = 0; // Last Minute weather was checked
uint8_t nextPoisonRunMinute = 0; // next minute that antipoison will run
uint8_t nextSecondToChangeDateTimeModes = 0;
uint8_t nextSecondToChangeWeatherTime = 0;
unsigned long userNotifyTimer = 0;
struct SuccessfulPingTime
{
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t NextMinuteToCheck = 0;
  bool PowerIsOn = true;
};
SuccessfulPingTime lastSuccessfulPing;
// Mode Vars
uint8_t timeDateDisplayMode = NIXIE_MODE_DISPLAY_TIME; // display time, date, or rotate between them
boolean displayDateOrTime = NIXIE_IS_DISPLAY_TIME;     // are we displaying the date or time
boolean vfdCurrentDisplayTime = WEATHERNOW;            // displaying weather now, or tomorrow's forecast
uint8_t weatherDisplayMode = WEATHER_DISPLAY_ROTATE;   // rotate, static now, static tomorrow
boolean matrixDisplayDynamic = true;                   // dynamic or static. true=dyn
boolean currentMatrixDisplayTime = WEATHERNOW;         // tmrw or now. 1 = now
// Struct for clock user settings
struct DeviceSettings
{
  bool twelveHourMode;
  uint8_t dateTimeDisplayRotateSpeed;
  uint8_t ClockTransitionMode;
  uint8_t displayOnHour;
  uint8_t displayOffHour;
  uint8_t poisonTimeSpan;
  IV17::TransitionMode vfd_transition;
  INS1Matrix::TransitionMode matrix_transition;
};
DeviceSettings settings;

// FUNCTIONS
void settingsMenu();

int readRotEncoder(int counter);
boolean readButton(uint8_t pin);
void userInputClock(uint8_t digits[3], uint8_t minValues[3], uint8_t maxValues[3], uint8_t colons);
void setAntiPoisonMinute();
void setRotationSpeed();
void setTransitionMode();
void setHourDisplayMode();
void setOnOffTime();
void setVfdMatrixTransition();
int changeMode(int mode, int numOfModes);
void updateDateTime();
void displayTime();
void scrollNixieTimeTask(void *parameter);
void nixieAntiPoisonTask(void *parameter);
void displayDate();
void updateWeatherTask(void *parameter);
void updateWeather();
const uint32_t *selectIcon(const char *iconStr);
void displayVFDWeather();
void setMatrixWeatherDisplay();
void setMatrixWeatherDisplayTask(void *parameter);
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
    settings.poisonTimeSpan = 17;
    settings.dateTimeDisplayRotateSpeed = 15;
    settings.ClockTransitionMode = 1;
    settings.twelveHourMode = 1;
    settings.vfd_transition = IV17::ROTATE;
    settings.matrix_transition = INS1Matrix::VERTICAL_BOUNCE;
    writeEEPROMWithCRC(settings);
    EEPROM.commit();
  }
  else
  {
    Serial.println("CRC GOOD");
  }

  pinMode(SHUTDOWN_PWR_SUPPLY_PIN, OUTPUT);
  pinMode(ROTCLK_PIN, INPUT);
  pinMode(ROTDT_PIN, INPUT);
  pinMode(ROTBTTN_PIN, INPUT);

  pinMode(DYN_STAT_SW_PIN, INPUT_PULLUP);
  pinMode(ON_OFF_SW_PIN, INPUT_PULLUP);

  pinMode(WEATHER_MODE_BTN_PIN, INPUT);
  pinMode(NOW_LED_PIN, OUTPUT);
  pinMode(TMRW_LED_PIN, OUTPUT);
  pinMode(WIFI_LED_PIN, OUTPUT);
  int i = 0;
  while (i < 10)
  {
    // wait 10 seconds for 12v power to come up
    // even though the esp32 wont be booted until then. Just some extra saftey.
    digitalWrite(WIFI_LED_PIN, !(digitalRead(WIFI_LED_PIN)));
    delay(1000);
    i++;
  }

  digitalWrite(SHUTDOWN_PWR_SUPPLY_PIN, HIGH); // turn on power
  // setup rotary encoder
  currentStateCLK = digitalRead(ROTCLK_PIN);
  lastStateCLK = currentStateCLK;

  // blank display while booting
  matrix.writeStaticImgToDisplay(matrixAllOff);
  Nixies.writeToNixie(255, 255, 255, 0);
  ivtubes.shiftOutString("       ");
  nixieMutex = xSemaphoreCreateMutex();
  ivtubesMutex = xSemaphoreCreateMutex();
  weatherMutex = xSemaphoreCreateMutex();
  matrixMutex = xSemaphoreCreateMutex();
  updateWeatherTaskMutex = xSemaphoreCreateMutex();
  // connect to WiFi
  initWiFi();
  // init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  updateDateTime();
  nextPoisonRunMinute = (minute + settings.poisonTimeSpan) % 60;
  nextMinuteToUpdateWeather = (((minute / 10) * 10) + 10) % 60;
  lastSuccessfulPing.NextMinuteToCheck = (minute + 1) % 60;
  updateWeather();
  setCpuFrequencyMhz(80); // slow down for power savings
  delay(1000);
  setMatrixWeatherDisplay();
}

void loop()
{
  // turn off time
  if (digitalRead(ON_OFF_SW_PIN) && isBetweenHours(hour, settings.displayOffHour, settings.displayOnHour)) // LOW, switch is ON, in auto position
  {
    // turn off supply and go to sleep, until waken by interrupt or by timer
    digitalWrite(SHUTDOWN_PWR_SUPPLY_PIN, LOW);

    Serial.println("SLEEPING");
    delay(1000);
    // Configure wakeup time to wake up at displayOnHour
    uint64_t wakeup_interval_us = ((((settings.displayOnHour - hour + 24) % 24) * 3600) - (minute * 60) - second) * 1000000ULL; // Calculate the time until displayOnHour
    Serial.println(wakeup_interval_us);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_27, LOW); // this pin has the auto/off switch on it
    esp_sleep_enable_timer_wakeup(wakeup_interval_us);

    // Enter deep sleep mode
    esp_deep_sleep_start();
  }
// Turn off Power supplies while im not home
#ifdef PHONE_PING
  if (minute == lastSuccessfulPing.NextMinuteToCheck && digitalRead(ON_OFF_SW_PIN)) // check every 5 minutes, if in auto position
  {
    //Serial.println("check");
    lastSuccessfulPing.NextMinuteToCheck = (lastSuccessfulPing.NextMinuteToCheck + 5) % 60;
    if (isBetweenHours(hour, 9, 16)) // only do this between 9 and 4
    {
      if (Ping.ping(PhoneIPAddress, 1))
      {
        //Serial.println("PING");
        lastSuccessfulPing.hour = hour;
        lastSuccessfulPing.minute = minute;
        if (lastSuccessfulPing.PowerIsOn == false)
        {
          //Serial.println("TurnOn");
          digitalWrite(SHUTDOWN_PWR_SUPPLY_PIN, HIGH);
          lastSuccessfulPing.PowerIsOn = true;
        }
      }
      else
      {
        //Serial.println("NO PING");
        // Check if it's been 30 minutes since a successful ping
        if ((hour * 60 + minute) - (lastSuccessfulPing.hour * 60 + lastSuccessfulPing.minute) >= 30 && lastSuccessfulPing.PowerIsOn == true)
        {
          //Serial.println("TurnOff");
          digitalWrite(SHUTDOWN_PWR_SUPPLY_PIN, LOW); // If it has been 30 minutes since the last successful ping, turn off
          lastSuccessfulPing.PowerIsOn = false;
        }
      }
    }
    else if (lastSuccessfulPing.PowerIsOn == false)
    {
      digitalWrite(SHUTDOWN_PWR_SUPPLY_PIN, HIGH); // turn back on after the work day is over
    }
  }
  if (lastSuccessfulPing.PowerIsOn == false && digitalRead(ON_OFF_SW_PIN) == 0)
  { // turn the supply back on if switch is moved to ON position
    digitalWrite(SHUTDOWN_PWR_SUPPLY_PIN, HIGH);
    lastSuccessfulPing.PowerIsOn = true;
  }
#endif

  // NixieTube section
  timeDateDisplayMode = changeMode(timeDateDisplayMode, 2); // rotary encoder to change time mode while in main loop
  updateDateTime();                                         // update the time vars
  if (minute == nextPoisonRunMinute)
  {
    nextPoisonRunMinute = (nextPoisonRunMinute + settings.poisonTimeSpan) % 60;
    xTaskCreate(
        nixieAntiPoisonTask,      // Function that should be called
        "anit poison for Nixies", // Name of the task (for debugging)
        1000,                     // Stack size (bytes)
        NULL,                     // Parameter to pass
        5,                        // Task priority
        NULL                      // Task handle
    );
  }
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

  // weather section
  if (minute == nextMinuteToUpdateWeather) // update the weather forcast
  {
    nextMinuteToUpdateWeather = (minute + weatherCheckFreqMin) % 60;
    Serial.println("UPDATE WEATHER");
    updateWeather();
    if (weatherDisplayMode != WEATHER_DISPLAY_ROTATE)
    {                            // issue is these will run before the weather struct is updates most likely
      displayVFDWeather();       // only callled when we update the weather. Otherwise, its static
      setMatrixWeatherDisplay(); // and update what it set for the matrix
    }
  }

  // VFD section
  if (millis() > userNotifyTimer + 1000) // allow time for VFD to notify user, then redisplay the weather
  {
    userNotifyTimer = UINT32_MAX - 1001; // set to max value
    // Serial.println(userNotifyTimer);
    displayVFDWeather();
  }
  if (weatherDisplayMode == WEATHER_DISPLAY_ROTATE) // if in rotate mode, do the rotation. otherwise we update it when we refresh the forecast
  {
    if (second == nextSecondToChangeWeatherTime)
    {
      nextSecondToChangeWeatherTime = (nextSecondToChangeWeatherTime + 15) % 60;
      vfdCurrentDisplayTime = !vfdCurrentDisplayTime;
      currentMatrixDisplayTime = !currentMatrixDisplayTime;
      if (vfdCurrentDisplayTime)
      {
        digitalWrite(NOW_LED_PIN, HIGH);
        digitalWrite(TMRW_LED_PIN, LOW);
      }
      else
      {
        digitalWrite(NOW_LED_PIN, LOW);
        digitalWrite(TMRW_LED_PIN, HIGH);
      }
      // if (xSemaphoreTake(weatherMutex, delay500ms) == pdTRUE)
      //{
      // if (std::memcmp(weather.currentIcon, weather.tmrwIcon, sizeof(weather.currentIcon)) != 0) // only change the display if the icons are different.
      //{
      // xSemaphoreGive(weatherMutex);
      setMatrixWeatherDisplay();
      //}
      //}
      displayVFDWeather();
    }
  }
  if (readButton(WEATHER_MODE_BTN_PIN)) // change VFD display mode
  {
    weatherDisplayMode = (weatherDisplayMode + 1) % 3; // 3 modes, loop back to 0
    switch (weatherDisplayMode)
    {
    case WEATHER_DISPLAY_NOW:
      if (xSemaphoreTake(ivtubesMutex, delay500ms) == pdTRUE)
      {
        ivtubes.shiftOutString("СЕЙЧАС");
        xSemaphoreGive(ivtubesMutex);
      }
      digitalWrite(NOW_LED_PIN, HIGH);
      digitalWrite(TMRW_LED_PIN, LOW);
      vfdCurrentDisplayTime = WEATHERNOW;
      currentMatrixDisplayTime = WEATHERNOW;
      setMatrixWeatherDisplay();
      break;
    case WEATHER_DISPLAY_TMRW:
      if (xSemaphoreTake(ivtubesMutex, delay500ms) == pdTRUE)
      {
        ivtubes.shiftOutString("ЗАВТРА");
        xSemaphoreGive(ivtubesMutex);
      }
      digitalWrite(NOW_LED_PIN, LOW);
      digitalWrite(TMRW_LED_PIN, HIGH);
      vfdCurrentDisplayTime = WEATHERTMRW;
      currentMatrixDisplayTime = WEATHERTMRW;
      setMatrixWeatherDisplay();
      break;
    case WEATHER_DISPLAY_ROTATE:
      nextSecondToChangeWeatherTime = 0;
      if (xSemaphoreTake(ivtubesMutex, delay500ms) == pdTRUE)
      {
        ivtubes.shiftOutString("ДИН   ");
        xSemaphoreGive(ivtubesMutex);
      }
    }
    userNotifyTimer = millis();
  }

  // matrix section
  if (digitalRead(DYN_STAT_SW_PIN) != matrixDisplayDynamic)
  {
    // Switch off = high = animate
    matrixDisplayDynamic = !matrixDisplayDynamic;
    setMatrixWeatherDisplay();
  }
  displayMatrixWeather(); // display weather, animate if needed

  // Settings section
  if (readButton(ROTBTTN_PIN))
  {
    if (xSemaphoreTake(nixieMutex, delay500ms) == pdTRUE)
    {
      if (xSemaphoreTake(ivtubesMutex, delay500ms) == pdTRUE)
      {
        settingsMenu();
        xSemaphoreGive(nixieMutex);
        xSemaphoreGive(ivtubesMutex);
      }
      else
      {
        xSemaphoreGive(nixieMutex);
      }
    }
  }
}
// user input via encoder to change clock settings
void settingsMenu()
{
  DeviceSettings oldSettings = settings;
  int settingsMode = 0;
  const int numOfSettings = 6; // number of settings needs manually set here

  while (1)
  {
    delay(5);
    settingsMode = changeMode(settingsMode, numOfSettings);
    switch (settingsMode)
    {
    case 0: // exit, just have number
      ivtubes.setScrollingString("Exit", 150);
      Nixies.writeToNixie(settingsMode, 255, 255, ALLOFF);
      break;
    case 1: // set date time rotation speed
      ivtubes.setScrollingString("Date/Time display rotate speed", 150);
      Nixies.writeToNixie(settingsMode, 255, settings.dateTimeDisplayRotateSpeed, ALLOFF);
      break;
    case 2: // setHourDisplay
      ivtubes.setScrollingString("12/24 hour mode", 150);
      Nixies.writeToNixie(255, 255, (settings.twelveHourMode ? 12 : 24), ALLOFF);
      break;
    case 3: // set setTransitionMode
      ivtubes.setScrollingString("Nixie Transition Mode", 150);
      Nixies.writeToNixie(255, 255, settings.ClockTransitionMode, ALLOFF);
      break;
    case 4: // set on off time for display
      ivtubes.setScrollingString("On Hour / Off Hour", 150);
      Nixies.writeToNixie(settings.displayOnHour, 255, settings.displayOffHour, 8);
      break;
    case 5:
      ivtubes.setScrollingString("Anti Poison Timer", 150);
      Nixies.writeToNixie(255, 255, settings.poisonTimeSpan, 8);
      break;
    case 6:
      ivtubes.setScrollingString("Matrix / VFD Transition", 150);
      Nixies.writeToNixie(settings.matrix_transition, 255, settings.vfd_transition, ALLOFF);
    }
    int lastSettingsMode = settingsMode;
    while (settingsMode == lastSettingsMode)
    {
      settingsMode = changeMode(settingsMode, numOfSettings);

      ivtubes.scrollString();
      if (readButton(ROTBTTN_PIN))
      {
        switch (settingsMode)
        {
        case 0:
          if (settings.twelveHourMode == oldSettings.twelveHourMode &&
              settings.dateTimeDisplayRotateSpeed == oldSettings.dateTimeDisplayRotateSpeed &&
              settings.ClockTransitionMode == oldSettings.ClockTransitionMode &&
              settings.displayOnHour == oldSettings.displayOnHour &&
              settings.displayOffHour == oldSettings.displayOffHour &&
              settings.poisonTimeSpan == oldSettings.poisonTimeSpan &&
              settings.matrix_transition == oldSettings.matrix_transition &&
              settings.vfd_transition == oldSettings.vfd_transition)
          {
          }
          else
          {
            EEPROM.commit();
          }
          // cleanup and get back to normal operation
          displayVFDWeather();
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
        case 6:
          setVfdMatrixTransition();
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
// use for general input on the clock display. see settings methods for example of usage
void userInputClock(uint8_t digits[3], uint8_t minValues[3], uint8_t maxValues[3], uint8_t colons) // digits[3] = hour, minute, second. //same for other arrays
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
    }
    else
    {
      digitMod++;
    }
  }
}

void setAntiPoisonMinute()
{
  uint8_t digits[3] = {255, 255, settings.poisonTimeSpan};
  uint8_t minValues[3] = {0, 0, 5};
  uint8_t maxValues[3] = {0, 0, 30};
  userInputClock(digits, minValues, maxValues, ALLOFF);
  settings.poisonTimeSpan = digits[2];
  writeEEPROMWithCRC(settings);
}
void setTransitionMode()
{
  uint8_t digits[3] = {255, 255, settings.ClockTransitionMode};
  uint8_t minValues[3] = {0, 0, 0};
  uint8_t maxValues[3] = {0, 0, 1};
  userInputClock(digits, minValues, maxValues, ALLOFF);
  settings.ClockTransitionMode = digits[2];
  writeEEPROMWithCRC(settings);
}
void setRotationSpeed()
{
  uint8_t digits[3] = {255, 255, settings.dateTimeDisplayRotateSpeed};
  uint8_t minValues[3] = {0, 0, 4};
  uint8_t maxValues[3] = {0, 0, 59};
  userInputClock(digits, minValues, maxValues, ALLOFF);
  settings.dateTimeDisplayRotateSpeed = digits[2];
  writeEEPROMWithCRC(settings);
}
void setHourDisplayMode()
{
  uint8_t digits[3] = {255, 255, settings.twelveHourMode};
  uint8_t minValues[3] = {0, 0, 0};
  uint8_t maxValues[3] = {0, 0, 1};
  userInputClock(digits, minValues, maxValues, ALLOFF);
  settings.twelveHourMode = (boolean)digits[2];
  writeEEPROMWithCRC(settings);
}
void setOnOffTime()
{
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
  settings.displayOffHour = digits[2];
  settings.displayOnHour = digits[0];
  writeEEPROMWithCRC(settings);
}
void setVfdMatrixTransition()
{
  uint8_t digits[3] = {settings.matrix_transition, 255, settings.vfd_transition};
  uint8_t minValues[3] = {0, 0, 0};
  uint8_t maxValues[3] = {INS1Matrix::num_enums - 1, 0, IV17::num_enums - 1};
  userInputClock(digits, minValues, maxValues, ALLOFF);
  settings.matrix_transition = (INS1Matrix::TransitionMode)digits[0];
  settings.vfd_transition = (IV17::TransitionMode)digits[2];
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
  int i = 0;
  while (!getLocalTime(&timeinfo))
  {
    i++;
    Serial.println("Failed to obtain time");
    ivtubes.setScrollingString("НЕ МОГУ ПОЛУЧИТЬ ВРЕМЯ ", 100);
    ivtubes.scrollStringSync();
    if (i > 10)
    {
      esp_restart(); // just reboot and try again
    }
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
    if (lastsecond != second) // only call if time has changed
    {
      lastsecond = second;
      xTaskCreate(
          scrollNixieTimeTask,                 // Function that should be called
          "Scroll transition mode for Nixies", // Name of the task (for debugging)
          1000,                                // Stack size (bytes)
          NULL,                                // Parameter to pass
          3,                                   // Task priority
          NULL                                 // Task handle
      );
    }
  }
  else
  {
    if (xSemaphoreTake(nixieMutex, delay500ms) == pdTRUE)
    {
      Nixies.writeToNixie((settings.twelveHourMode ? (hour > 12 ? hour - 12 : (hour == 00 ? 12 : hour)) : hour), minute, second, (second % 2 ? ALLOFF : ALLON));
      xSemaphoreGive(nixieMutex); // Release the mutex
    }
  }
}
void scrollNixieTimeTask(void *parameter) // used as a task
{
  if (xSemaphoreTake(nixieMutex, delay500ms) == pdTRUE)
  {
    Nixies.writeToNixieScroll((settings.twelveHourMode ? (hour > 12 ? hour - 12 : (hour == 00 ? 12 : hour)) : hour), minute, second, (second % 2 ? ALLOFF : ALLON));
    xSemaphoreGive(nixieMutex); // Release the mutex
  }
  vTaskDelete(NULL);
}
void nixieAntiPoisonTask(void *parameter) // used as a task
{
  if (xSemaphoreTake(nixieMutex, delay500ms) == pdTRUE)
  {
    Nixies.antiPoison();
    xSemaphoreGive(nixieMutex); // Release the mutex
  }
  vTaskDelete(NULL);
}
void vfdFancyTransitionTask(void *parameter) // used as a task
{
  if (xSemaphoreTake(ivtubesMutex, delay500ms) == pdTRUE)
  {
    ivtubes.fancyTransitionString(weatherVFD, settings.vfd_transition, transDelay);
    xSemaphoreGive(ivtubesMutex); // Release the mutex
  }
  vTaskDelete(NULL);
}
void displayDate()
{
  if (xSemaphoreTake(nixieMutex, delay500ms) == pdTRUE)
  {
    Nixies.writeToNixie(month, day, year - 2000, (second % 2 ? ALLOFF : 6));
    xSemaphoreGive(nixieMutex);
  }
}
void updateWeatherTask(void *parameter)
{
  //  generated code below
  //  Courtesy of https://arduinojson.org/v6/assistant
  //  Stream& input;
  //  for openweathermap
  if (xSemaphoreTake(updateWeatherTaskMutex, pdMS_TO_TICKS(5)) == pdTRUE)
  {
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
    Serial.println(WiFi.status());
    DeserializationError error = deserializeJson(doc, httpGETRequest(apiCallURL.c_str()), DeserializationOption::Filter(filter));
    int count = 0;
    while (error != DeserializationError::Ok) // if we dont get the data, attempt to connect to wifi again
    {
      count++;
      digitalWrite(WIFI_LED_PIN, LOW);
      Serial.print("deserializeJson() failed");

      int i = 0;
      if (WiFi.status() != WL_CONNECTED)
      {
        WiFi.disconnect(true); // disconnect and turn off radio
        WiFi.begin(ssid, password);
        // WiFi.setSleep(WIFI_PS_NONE);
        Serial.print("wifiBAD:");
      }
      else
      {
        delay(500);
        Serial.print("wifigood:");
      }
      while (WiFi.status() != WL_CONNECTED)
      {
        i++;
        Serial.print(WiFi.status());
        digitalWrite(WIFI_LED_PIN, !(digitalRead(WIFI_LED_PIN)));
        delay(50);
        // Serial.print(WiFi.status());
        if (i > 600)
        { // wait max of 30 seconds for wifi to come up.
          digitalWrite(WIFI_LED_PIN, LOW);
          xSemaphoreGive(updateWeatherTaskMutex);
          vTaskDelete(NULL); // bail. light will be out to indicate an issue
        }
      }
      error = deserializeJson(doc, httpGETRequest(apiCallURL.c_str()), DeserializationOption::Filter(filter)); // wifi connected, so we should be good
      // and if some reason we arent, it contiunes in this loop.
      if (count > 10)
      { // bail after 10 tries to get api while connected
        Serial.println("bailing, wifi connected");
        digitalWrite(WIFI_LED_PIN, LOW);
        xSemaphoreGive(updateWeatherTaskMutex);
        vTaskDelete(NULL);
      }
    }
    digitalWrite(WIFI_LED_PIN, LOW);
    if (xSemaphoreTake(weatherMutex, delay500ms) == pdTRUE)
    {
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

      xSemaphoreGive(weatherMutex);
      digitalWrite(WIFI_LED_PIN, HIGH);
    }
    xSemaphoreGive(updateWeatherTaskMutex);
  }
  vTaskDelete(NULL);
}
void updateWeather()
{
  xTaskCreate(
      updateWeatherTask,       // Function that should be called
      "update weather struct", // Name of the task (for debugging)
      4000,                    // Stack size (bytes)
      NULL,                    // Parameter to pass
      5,                       // Task priority
      NULL                     // Task handle
  );
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

void displayVFDWeather()
{
  //  todo, maybe round
  if (xSemaphoreTake(weatherMutex, delay500ms) == pdTRUE)
  {
    if (vfdCurrentDisplayTime)
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
    xSemaphoreGive(weatherMutex);
    xTaskCreate(
        vfdFancyTransitionTask,        // Function that should be called
        "fancy transition of ivtubes", // Name of the task (for debugging)
        1000,                          // Stack size (bytes)
        NULL,                          // Parameter to pass
        4,                             // Task priority
        NULL                           // Task handle
    );
    // Serial.println(weatherVFD);
  }
}
void setMatrixWeatherDisplay()
{
  if (xSemaphoreTake(weatherMutex, delay500ms) == pdTRUE)
  {
    const uint32_t *iconToDisplay = selectIcon(currentMatrixDisplayTime ? weather.currentIcon : weather.tmrwIcon);
    xSemaphoreGive(weatherMutex);
    xTaskCreate(
        setMatrixWeatherDisplayTask, // Function that should be called
        "set the matrix display",    // Name of the task (for debugging)
        2000,                        // Stack size (bytes)
        (void *)iconToDisplay,       // Parameter to pass
        5,                           // Task priority
        NULL                         // Task handle
    );
  }
}
void setMatrixWeatherDisplayTask(void *parameter)
{
  const uint32_t *iconToDisplay = static_cast<const uint32_t *>(parameter);
  if (xSemaphoreTake(matrixMutex, delay500ms) == pdTRUE)
  {
    if (matrixDisplayDynamic)
    {
      matrix.setAnimationToDisplay(iconToDisplay);
    }
    else
    {
      matrix.writeStaticImgToDisplay(const_cast<uint32_t *>((iconToDisplay + 1))); // just display the first frame of icon
    }
    matrix.fancyTransitionFrame(const_cast<uint32_t *>((iconToDisplay + 1)), settings.matrix_transition, transDelay);
    xSemaphoreGive(matrixMutex);
  }
  vTaskDelete(NULL);
}
void displayMatrixWeather()
{
  if (matrixDisplayDynamic) // if true, animate the display
  {
    if (xSemaphoreTake(matrixMutex, pdMS_TO_TICKS(5)) == pdTRUE) // 5 ms timeout
    {
      matrix.animateDisplay();
      xSemaphoreGive(matrixMutex);
    }
  }
}
String httpGETRequest(const char *serverName)
{
  HTTPClient http;
  // Your Domain name with URL path or IP address with path
  http.begin(serverName);
  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "";

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
  EEPROM.get(SETTINGS_ADDRESS, tempSettings);

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
  EEPROM.put(SETTINGS_ADDRESS, settings);

  // Calculate CRC for the data in EEPROM
  uint32_t calculatedCRC = calculateCRC(settings);

  // Write CRC to EEPROM
  EEPROM.put(EEPROM_CRC_ADDRESS, calculatedCRC);
}
void initWiFi()
{
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  // WiFi.setSleep(WIFI_PS_NONE);
  matrix.setAnimationToDisplay(loadingAnimation);
  ivtubes.setScrollingString("ПОДКЛЮЧЕНИЕ К Wi-Fi", 150); // connecting to wifi
  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(WIFI_LED_PIN, !(digitalRead(WIFI_LED_PIN)));
    ivtubes.scrollString();
    matrix.animateDisplay();
    delay(75);
    Serial.print(".");
  }
  digitalWrite(WIFI_LED_PIN, HIGH);
  Serial.println(" CONNECTED");
  // WiFi.setSleep(true);                          // enable wifi sleep for power savings
  matrix.writeStaticImgToDisplay(const_cast<uint32_t *>(checkMarkIcon));
  ivtubes.setScrollingString("ПОДКЛЮЧЕН ", 150); // connected
  ivtubes.scrollStringSync();
}
