//Made by Nathan Safran 4/4/2020
//https://github.com/nsafran1217
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>

// DO NOT CHANGE, PIN ASSIGNMENTS
#define LATCHPIN 2
#define DATAPIN 3
#define CLOCKPIN 4
#define ROTCLKPIN 10
#define ROTDTPIN 9
#define ROTBTTNPIN 8
#define SHTDNPIN 5

//EEPROM ADDRESSES
#define twelveHourModeADDRESS 0
#define ROTATETIMEADDRESS 1
#define TRANSMODEADDRESS 2
#define ONHOURADDRESS 3
#define OFFHOURADDRESS 4
#define poisonTimeSpanADDRESS 5
#define poisonTimeStartADDRESS 6

RTC_DS3231 rtc; //init RTC

int hour, minute, second, year, month, day;
int colons;

int currentStateCLK;
int lastStateCLK;
unsigned long lastButtonPress = 0;

int rotatespeed = EEPROM.read(ROTATETIMEADDRESS);            //time in seconds, min 4, max 59
boolean twelveHourMode = EEPROM.read(twelveHourModeADDRESS); //0 for 12, 1 for 24
int TransMode = EEPROM.read(TRANSMODEADDRESS);
int onHour = EEPROM.read(ONHOURADDRESS);
int offHour = EEPROM.read(OFFHOURADDRESS);
int poisonTimeSpan = EEPROM.read(poisonTimeSpanADDRESS);
int poisonTimeStart = EEPROM.read(poisonTimeStartADDRESS);
int nextPoisonRun;
boolean dateOrTime = false;
boolean displayOff = false;
unsigned long lastBlinkTime = 0; //for flashing display/digits
unsigned long blinkTime = 500;
unsigned long waitForDisplay = 0; //for displaying mode
int nextSwitch = 0;
int displayMode = 0;
//0  Time
//1  Date
//2  Rotate time and date

//int lastHourTen, lastHourOne, lastMinuteTen, lastMinuteOne, lastSecondTen, lastSecondOne; //for scroll mode
//int HourTen, HourOne, MinuteTen, MinuteOne, SecondTen, SecondOne;
//int secondsCombined, minutesCombined, hoursCombined;

void setup()
{
  Serial.begin(57600); //DEBUG

  rtc.begin();
  pinMode(LATCHPIN, OUTPUT);
  pinMode(CLOCKPIN, OUTPUT);
  pinMode(DATAPIN, OUTPUT);
  pinMode(SHTDNPIN, OUTPUT);
  pinMode(ROTCLKPIN, INPUT);
  pinMode(ROTDTPIN, INPUT);
  pinMode(ROTBTTNPIN, INPUT);
  digitalWrite(ROTBTTNPIN, HIGH);

  lastStateCLK = digitalRead(ROTCLKPIN);
  getDateTime();
  lastHourTen = (hour / 10) % 10;
  lastHourOne = hour % 10;
  lastMinuteTen = (minute / 10) % 10;
  lastMinuteOne = minute % 10;
  lastSecondTen = (second / 10) % 10;
  lastSecondOne = second % 10;
  nextPoisonRun = poisonTimeStart + poisonTimeSpan;
  while (minute > nextPoisonRun)
  {
    nextPoisonRun = nextPoisonRun + poisonTimeSpan;
    if (nextPoisonRun > 59)
    {
      nextPoisonRun = nextPoisonRun - 60;
    }
  }
}

int readRotEncoder(int counter) //takes a counter in and spits it back out if it changes
{
  // Read the current state of CLK
  currentStateCLK = digitalRead(ROTCLKPIN);
  // If last and current state of CLK are different, then pulse occurred
  // React to only 1 state change to avoid double count
  if (currentStateCLK != lastStateCLK && currentStateCLK == 1)
  {
    // the encoder is rotating CCW so decrement
    if (digitalRead(ROTDTPIN) != currentStateCLK)
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
boolean readRotButton() //true if button pressed
{
  // Read the button state
  int btnState = digitalRead(ROTBTTNPIN);
  //If we detect LOW signal, button is pressed
  if (btnState == LOW)
  {
    //if 50ms have passed since last LOW pulse, it means that the
    //button has been pressed, released and pressed again
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

int changeMode(int mode, int numOfModes) //numofmodes starts at 0! display mode on tube, easy for selecting
{
  int lastmode = mode;
  mode = readRotEncoder(mode);
  if (mode > numOfModes) //if mode invalid, loop around
  {
    mode = 0;
  }
  else if (mode < 0)
  {
    mode = numOfModes;
  }
  if (mode != lastmode)
  {
    waitForDisplay = millis();
    while (millis() - waitForDisplay < 500)
    {
      delay(5);
      writeToNixie(mode, 255, 255, 0);
      mode = changeMode(mode, numOfModes);
    }
  }
  return mode;
}
void loop()
{
  delay(5);
  getDateTime();
  if (hour == offHour && onHour != offHour && !displayOff) ///turn off power supply
  {
    displayOff = !displayOff;
    
  }
  if (hour == onHour && onHour != offHour && displayOff) ///turn back on
  {
    displayOff = !displayOff;
  }

  if (displayOff){
    digitalWrite(SHTDNPIN, HIGH);
    delay(10000);
  } else{
    digitalWrite(SHTDNPIN, LOW);
  }

  if (minute == nextPoisonRun)
  {
    antiPoison();
    nextPoisonRun = nextPoisonRun + poisonTimeSpan;
    if (nextPoisonRun > 59)
    {
      nextPoisonRun = nextPoisonRun - 60;
    }
  }

  if (readRotButton()) //enter settings menu
  {
    settingsMenu();
  }
  displayMode = changeMode(displayMode, 2);
  switch (displayMode)
  {
  case 0:
    if (TransMode == 0)
    {
      normalTimeMode();
    }
    else if (TransMode == 1)
    {
      scrollTimeMode();
    }

    break;
  case 1:
    dateMode();
    break;
  case 2:
    rotateMode();
    break;
  }
}

void settingsMenu()
{
  int settingsMode = 0;
  boolean exitSettings = false;
  lastBlinkTime = millis();
  while (!exitSettings)
  {
    delay(5);
    settingsMode = displayMode = changeMode(displayMode, 7);
    switch (settingsMode)
    {
    case 0: //exit, just have number
      writeToNixie(settingsMode, 255, 255, 0);
      break;
    case 1: //change time
      if ((millis() - lastBlinkTime) < blinkTime)
      {
        writeToNixie(hour, minute, second, 15);
      }
      else if (((millis() - lastBlinkTime) > blinkTime * 2))
      {
        writeToNixie(hour, minute, second, 15);
        lastBlinkTime = millis(); //reset timer
      }
      else
      {
        writeToNixie(255, 255, 255, 15);
      }
      break;
    case 2: //change date
      if ((millis() - lastBlinkTime) < blinkTime)
      {
        writeToNixie(month, day, year, 6);
      }
      else if (((millis() - lastBlinkTime) > blinkTime * 2))
      {
        writeToNixie(month, day, year, 6);
        lastBlinkTime = millis(); //reset timer
      }
      else
      {
        writeToNixie(255, 255, 255, 15);
      }
      break;
    case 3: //set date time rotation speed
      writeToNixie(settingsMode, 255, rotatespeed, 0);
      break;
    case 4: //setHourDisplay
      if (twelveHourMode)
      {
        writeToNixie(settingsMode, 255, 12, 0);
      }
      else
      {
        writeToNixie(settingsMode, 255, 24, 0);
      }
      break;
    case 5: //set setTransitionMode
      writeToNixie(settingsMode, 255, TransMode, 0);
      break;
    case 6: //set on off time for display
      writeToNixie(settingsMode, offHour, onHour, 8);
      break;
    case 7:
      writeToNixie(settingsMode, poisonTimeStart, poisonTimeSpan, 8);
      break;
    }

    if (readRotButton())
    {
      switch (settingsMode)
      {
      case 0:
        exitSettings = true; //exit back to display mode
        break;
      case 1:
        setTime();
        break;
      case 2:
        setDate();
        break;
      case 3:
        setRotationSpeed();
        break;
      case 4:
        setHourDisplay();
        break;
      case 5:
        setTransitionMode();
        break;
      case 6:
        setOnOffTime();
        break;
      case 7:
        setAntiPoisonMinute();
        break;
      }
    }
  }
}
void normalTimeMode()
{
  if (second % 2 == 0) //too slow, change to millis timer for 500 milliseconds
  {
    colons = 15;
  }
  else
  {
    colons = 0;
  }
  if (twelveHourMode)
  {
    if (hour > 12)
    {
      writeToNixie(hour - 12, minute, second, colons);
    }
    else if (hour == 00)
    {
      writeToNixie(12, minute, second, colons);
    }

    else
    {
      writeToNixie(hour, minute, second, colons);
    }
  }
  else
  {
    writeToNixie(hour, minute, second, colons);
  }
}

void scrollTimeMode()
{
  if (second % 2 == 0) //too slow, change to millis timer for 500 milliseconds
  {
    colons = 15;
  }
  else
  {
    colons = 0;
  }
  if (twelveHourMode)
  {
    if (hour > 12)
    {
      writeToNixieScroll(hour - 12, minute, second, colons);
    }
    else if (hour == 00)
    {
      writeToNixieScroll(12, minute, second, colons);
    }

    else
    {
      writeToNixieScroll(hour, minute, second, colons);
    }
  }
  else
  {
    writeToNixieScroll(hour, minute, second, colons); //////debug change
  }
}

void dateMode()
{
  //printtime(); //debug
  if (second % 2 == 0) //too slow, change to millis timer for 500 milliseconds
  {
    colons = 6;
  }
  else
  {
    colons = 0;
  }
  writeToNixie(month, day, year, colons);
}
void rotateMode()
{
  if (second == nextSwitch)
  {
    nextSwitch = ((second + rotatespeed) % 60);
    dateOrTime = !dateOrTime;
  }
  if (dateOrTime)
  {
    normalTimeMode(); /////////////////maybe change
  }
  else
  {
    dateMode();
  }
}
void antiPoison()
{
  int j;
  for (int i = 0; i < 10; i++)
  {
    j = i | ((i) << 4);
    writeToNixieRAW(j, j, j, 15);
    delay(1000);
  }
}
void setTime()
{
  int lasthour, lastminute, lastsecond;
  int digitMod = 0;
  lastBlinkTime = millis();
  while (digitMod < 3)
  {
    lasthour = hour;
    lastminute = minute;
    lastsecond = second;
    delay(5);
    if (readRotButton())
    {
      lastBlinkTime = millis(); //reset timer
      digitMod++;
    }
    if ((millis() - lastBlinkTime) < blinkTime)
    {

      writeToNixie(hour, minute, second, 15);
    }
    else if (((millis() - lastBlinkTime) > blinkTime * 2))
    {
      writeToNixie(hour, minute, second, 15);
      lastBlinkTime = millis(); //reset timer
    }
    else
    {
      //switch off
      switch (digitMod)
      {
      case 0:
        writeToNixie(255, minute, second, 15); //255 will blank the display
        break;
      case 1:
        writeToNixie(hour, 255, second, 15);
        break;
      case 2:
        writeToNixie(hour, minute, 255, 15);
        break;
      }
    }

    switch (digitMod)
    {
    case 0:
      hour = readRotEncoder(hour);
      if (hour != lasthour)
      {
        lastBlinkTime = millis(); //reset timer
        if (hour > 23)
        {
          hour = 0;
        }
        else if (hour < 0)
        {
          hour = 23;
        }
      }
      break;
    case 1:
      minute = readRotEncoder(minute);
      if (minute != lastminute)
      {
        lastBlinkTime = millis(); //reset timer
        if (minute > 59)
        {
          minute = 0;
        }
        else if (minute < 0)
        {
          minute = 59;
        }
      }
      break;
    case 2:
      second = readRotEncoder(second);
      if (second != lastsecond)
      {
        lastBlinkTime = millis(); //reset timer
        if (second > 59)
        {
          second = 0;
        }
        else if (second < 0)
        {
          second = 59;
        }
      }
      break;
    }
  }
  rtc.adjust(DateTime(year, month, day, hour, minute, second));
}
void setDate()
{
  int lastyear, lastmonth, lastday;
  int digitMod = 0;
  lastBlinkTime = millis();
  while (digitMod < 3)
  {
    lastyear = year;
    lastmonth = month;
    lastday = day;
    delay(5);
    if (readRotButton())
    {
      lastBlinkTime = millis(); //reset timer
      digitMod++;
    }
    if (digitMod == 2)
    {
      writeToNixie(255, year / 100, year % 100, 6);
    }
    else
    {
      if ((millis() - lastBlinkTime) < blinkTime)
      {

        writeToNixie(month, day, year, 6);
      }
      else if (((millis() - lastBlinkTime) > blinkTime * 2))
      {
        writeToNixie(month, day, year, 6);
        lastBlinkTime = millis(); //reset timer
      }
      else
      {
        //switch off
        switch (digitMod)
        {
        case 0:
          writeToNixie(255, day, year, 6); //255 will blank the display
          break;
        case 1:
          writeToNixie(month, 255, year, 6);
          break;
        }
      }
    }

    switch (digitMod)
    {
    case 0:
      month = readRotEncoder(month);
      if (month != lastmonth)
      {
        lastBlinkTime = millis(); //reset timer
        if (month > 12)
        {
          month = 1;
        }
        else if (month < 1)
        {
          month = 12;
        }
      }
      break;
    case 1:
      day = readRotEncoder(day);
      if (day != lastday)
      {
        lastBlinkTime = millis(); //reset timer
        if (day > 31)
        {
          day = 1;
        }
        else if (day < 1)
        {
          day = 31;
        }
      }
      break;
    case 2:
      year = readRotEncoder(year);
      if (year != lastyear)
      {
        lastBlinkTime = millis(); //reset timer
        if (year > 2100)
        {
          year = 2100;
        }
        else if (year < 2020)
        {
          year = 2020;
        }
      }
      break;
    }
  }
  DateTime now = rtc.now();
  hour = now.hour();
  minute = now.minute();
  second = now.second();
  rtc.adjust(DateTime(year, month, day, hour, minute, second));
}
void setHourDisplay()
{
  boolean origBool = twelveHourMode;
  int counter = 0;
  if (twelveHourMode)
  {
    counter = 1;
  }
  while (!readRotButton())
  {
    delay(10);
    counter = readRotEncoder(counter);
    if (counter % 2 == 0)
    {
      twelveHourMode = false;
    }
    else
    {
      twelveHourMode = true;
    }
    if (twelveHourMode)
    {
      writeToNixie(255, 255, 12, 0);
    }
    else
    {
      writeToNixie(255, 255, 24, 0);
    }
  }
  if (origBool != twelveHourMode) //thi sis not working
  {
    EEPROM.write(twelveHourModeADDRESS, (boolean)twelveHourMode); //commmit change to EEPROM
  }
}
void setRotationSpeed()
{
  int origRotateSpeed = rotatespeed;
  while (!readRotButton())
  {
    delay(5);
    rotatespeed = readRotEncoder(rotatespeed);
    if (rotatespeed > 59)
    {
      rotatespeed = 59;
    }
    writeToNixie(255, 255, rotatespeed, 0);
  }
  if (origRotateSpeed != rotatespeed)
  {
    EEPROM.write(ROTATETIMEADDRESS, rotatespeed); //commit change to EEPROM
  }
}

void setTransitionMode()
{
  int origTransMode = TransMode;
  while (!readRotButton())
  {
    delay(10);
    TransMode = readRotEncoder(TransMode);
    if (TransMode > 1)
    {
      TransMode = 0;
    }
    if (TransMode < 0)
    {
      TransMode = 1;
    }
    writeToNixie(255, 255, TransMode, 0);
  }
  if (origTransMode != TransMode)
  {
    EEPROM.write(TRANSMODEADDRESS, TransMode); //commit change to EEPROM
  }
}
void setOnOffTime() ////////////////////////////////////////////////////////////////////////
{
  int origOffHour = offHour; //
  int origOnHour = onHour;
  int lastOnHour, lastOffHour;
  int varMod = 0;
  lastBlinkTime = millis();
  while (varMod < 2)
  {
    delay(5);
    lastOffHour = offHour;
    lastOnHour = onHour;
    if (readRotButton())
    {
      varMod++;
    }
    if ((millis() - lastBlinkTime) < blinkTime)
    {
      writeToNixie(255, offHour, onHour, 8);
    }
    else if (((millis() - lastBlinkTime) > blinkTime * 2))
    {
      writeToNixie(255, offHour, onHour, 8);
      lastBlinkTime = millis(); //reset timer
    }
    else
    {
      switch (varMod)
      {
      case 0:
        writeToNixie(255, 255, onHour, 8);
        break;
      case 1:
        writeToNixie(255, offHour, 255, 8);
        break;
      }
    }
    switch (varMod)
    {
    case 0:
      offHour = readRotEncoder(offHour); //
      if (offHour != lastOffHour)
      {
        lastBlinkTime = millis(); //reset timer
        if (offHour > 23)
        {
          offHour = 0;
        }
        if (offHour < 0)
        {
          offHour = 23;
        }
      }
      break;
    case 1:
      onHour = readRotEncoder(onHour); //
      if (onHour != lastOnHour)
      {
        lastBlinkTime = millis(); //reset timer
        if (onHour > 23)
        {
          onHour = 0;
        }
        if (onHour < 0)
        {
          onHour = 23;
        }
      }
      break;
    }
  }
  if (origOffHour != offHour)
  {
    EEPROM.write(OFFHOURADDRESS, offHour); //commit change to EEPROM
  }
  if (origOnHour != onHour)
  {
    EEPROM.write(ONHOURADDRESS, onHour); //commit change to EEPROM
  }
}
void setAntiPoisonMinute()
{
  int origpoisonTimeSpan = poisonTimeSpan; //
  int origpoisonTimeStart = poisonTimeStart;
  int lastPoisonTimeStart, lastPoisonTimeSpan;
  int varMod = 0;
  lastBlinkTime = millis();
  while (varMod < 2)
  {
    delay(5);
    lastPoisonTimeStart = poisonTimeStart;
    lastPoisonTimeSpan = poisonTimeSpan;
    if (readRotButton())
    {
      varMod++;
    }
    if ((millis() - lastBlinkTime) < blinkTime)
    {
      writeToNixie(255, poisonTimeStart, poisonTimeSpan, 8);
    }
    else if (((millis() - lastBlinkTime) > blinkTime * 2))
    {
      writeToNixie(255, poisonTimeStart, poisonTimeSpan, 8);
      lastBlinkTime = millis(); //reset timer
    }
    else
    {
      switch (varMod)
      {
      case 0:
        writeToNixie(255, 255, poisonTimeSpan, 8);
        break;
      case 1:
        writeToNixie(255, poisonTimeStart, 255, 8);
        break;
      }
    }
    switch (varMod)
    {
    case 0:
      poisonTimeStart = readRotEncoder(poisonTimeStart); //
      if (poisonTimeStart != lastPoisonTimeStart)
      {
        lastBlinkTime = millis(); //reset timer
        if (poisonTimeStart > 59)
        {
          poisonTimeStart = 0;
        }
        if (poisonTimeStart < 0)
        {
          poisonTimeStart = 59;
        }
      }
      break;
    case 1:
      poisonTimeSpan = readRotEncoder(poisonTimeSpan); //
      if (poisonTimeSpan != lastPoisonTimeSpan)
      {
        lastBlinkTime = millis(); //reset timer
        if (poisonTimeSpan > 59)
        {
          poisonTimeSpan = 0;
        }
        if (poisonTimeSpan < 0)
        {
          poisonTimeSpan = 59;
        }
      }
      break;
    }
  }
  if (origpoisonTimeStart != poisonTimeStart)
  {
    EEPROM.write(poisonTimeStartADDRESS, poisonTimeStart); //commit change to EEPROM
  }
  if (origpoisonTimeSpan != poisonTimeSpan)
  {
    EEPROM.write(poisonTimeSpanADDRESS, poisonTimeSpan); //commit change to EEPROM
  }
}

void getDateTime()
{
  DateTime now = rtc.now();
  hour = now.hour();
  minute = now.minute();
  second = now.second();
  year = now.year();
  month = now.month();
  day = now.day();
}

void writeToNixie(int hours, int minutes, int seconds, int divider) //write data to nixie tubes
{
  //printNixieToSerial(hours, minutes, seconds, divider); //degug
  digitalWrite(LATCHPIN, LOW);
  shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, divider);
  shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, convertToNixe(seconds));
  shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, convertToNixe(minutes));
  shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, convertToNixe(hours));
  digitalWrite(LATCHPIN, HIGH);
}

void writeToNixieRAW(int hours, int minutes, int seconds, int divider) //write data to nixie tubes RAW
{
  //printNixieToSerial(hours, minutes, seconds, divider);
  digitalWrite(LATCHPIN, LOW);
  shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, divider);
  shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, seconds);
  shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, minutes);
  shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, hours);
  digitalWrite(LATCHPIN, HIGH);
}

void writeToNixieScroll(int hours, int minutes, int seconds, int divider) //this will be annoying i need an efficiant way to do this. or brute force it
{                                                                         //this is so messy I hate it

  HourTen = (hours / 10) % 10;
  HourOne = hours % 10;
  MinuteTen = (minutes / 10) % 10;
  MinuteOne = minutes % 10;
  SecondTen = (seconds / 10) % 10;
  SecondOne = seconds % 10;
  ///////

  boolean scrollDigitBool[6] = {SecondOne != lastSecondOne, SecondTen != lastSecondTen, MinuteOne != lastMinuteOne, MinuteTen != lastMinuteTen, HourOne != lastHourOne, HourTen != lastHourTen};
  if (scrollDigitBool[0])
  {
    for (int i = 0; i < 11; i++)
    {
      lastSecondOne++;
      if (scrollDigitBool[1])
      {
        lastSecondTen++;
      }
      if (scrollDigitBool[2])
      {
        lastMinuteOne++;
      }
      if (scrollDigitBool[3])
      {
        lastMinuteTen++;
      }
      if (scrollDigitBool[4])
      {
        lastHourOne++;
      }
      if (scrollDigitBool[5])
      {
        lastHourTen++;
      }
      secondsCombined = (lastSecondTen % 10) | ((lastSecondOne % 10) << 4);
      minutesCombined = (lastMinuteTen % 10) | ((lastMinuteOne % 10) << 4);
      hoursCombined = (lastHourTen % 10) | ((lastHourOne % 10) << 4);
      writeToNixieRAW(hoursCombined, minutesCombined, secondsCombined, divider);
      delay(15);
    }
    if (lastSecondTen == 16)
    {
      lastSecondTen = 0;
      if (lastMinuteTen == 16)
      {
        lastMinuteTen = 0;
        if (twelveHourMode)
        {
          if (lastHourOne == 13)
          {
            lastHourOne = 1;
            lastHourTen = 0;
          }
        }
        else
        {
          if (lastHourOne == 14)
          {
            lastHourOne = 0;
            lastHourTen = 0;
          }
        }
      }
      secondsCombined = (lastSecondTen % 10) | ((lastSecondOne % 10) << 4);
      minutesCombined = (lastMinuteTen % 10) | ((lastMinuteOne % 10) << 4);
      hoursCombined = (lastHourTen % 10) | ((lastHourOne % 10) << 4);
      writeToNixieRAW(hoursCombined, minutesCombined, secondsCombined, divider);
    }
  }

  lastHourTen = (hours / 10) % 10;
  lastHourOne = hours % 10;
  lastMinuteTen = (minutes / 10) % 10;
  lastMinuteOne = minutes % 10;
  lastSecondTen = (seconds / 10) % 10;
  lastSecondOne = seconds % 10;
}

int convertToNixe(int num) //convert 2 digit number to display on nixie tubes
{
  int nixienum;
  if (num == 255) //blank the display
  {
    nixienum = num;
  }
  else
  {
    nixienum = ((num / 10) % 10) | ((num % 10) << 4); //hours (tens digit) or-ed with (ones place shifted 4 left)
  }
  return nixienum;
}

void printNixieToSerial(int hours, int minutes, int seconds, int divider)
{

  Serial.print(hours, DEC);
  Serial.print(':');
  Serial.print(minutes, DEC);
  Serial.print(':');
  Serial.print(seconds, DEC);
  Serial.print('-');
  Serial.print(divider, DEC);
  Serial.print('-');
  Serial.print(displayMode, DEC);
  Serial.println();
}
void printtime()
{ //debug
  Serial.print(year, DEC);
  Serial.print('/');
  Serial.print(month, DEC);
  Serial.print('/');
  Serial.print(day, DEC);
  Serial.println();
  Serial.print(hour, DEC);
  Serial.print(':');
  Serial.print(minute, DEC);
  Serial.print(':');
  Serial.print(second, DEC);
  Serial.println();
}
