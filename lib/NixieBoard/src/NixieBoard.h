/*
Library for my custom Nixie board. Not portable
Nathan Safran - 6/30/2023
*/

#ifndef NixieBoard_h
#define NixieBoard_h
#include "Arduino.h"

#define ALLON 15
#define BOTON 6
#define ALLOFF 0

class NixieBoard
{
  public:
    NixieBoard(uint8_t dataPin, uint8_t clockPin, uint8_t latchPin);
    void writeToNixieScroll(uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t divider);
    void writeToNixie(uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t divider);  //write numbers to the nixie tubes
    void writeToNixieRAW(uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t divider); //write arbitary data to the nixie tubes
    void antiPoison();

  private:
    uint8_t _dataPin;
    uint8_t _clockPin;
    uint8_t _latchPin;
    uint8_t _currentHours;
    uint8_t _currentMinutes;
    uint8_t _currentSeconds;
    int convertToNixe(uint8_t num); //convert 2 digit number to display on nixie tubes
  
    unsigned long _timeSinceLastScroll;

};

#endif
