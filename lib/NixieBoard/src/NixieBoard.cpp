/*
Library for my custom Nixie board. Not portable
Nathan Safran - 6/30/2023
*/

#include "Arduino.h"
#include "NixieBoard.h"

NixieBoard::NixieBoard(uint8_t dataPin, uint8_t clockPin, uint8_t latchPin)
{
    pinMode(dataPin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(latchPin, OUTPUT);
    _dataPin = dataPin;
    _clockPin = clockPin;
    _latchPin = latchPin;
}

void NixieBoard::writeToNixie(uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t divider) // write data to nixie tubes
{
    digitalWrite(_latchPin, LOW);
    shiftOut(_dataPin, _clockPin, MSBFIRST, divider);
    shiftOut(_dataPin, _clockPin, MSBFIRST, convertToNixe(seconds));
    shiftOut(_dataPin, _clockPin, MSBFIRST, convertToNixe(minutes));
    shiftOut(_dataPin, _clockPin, MSBFIRST, convertToNixe(hours));
    digitalWrite(_latchPin, HIGH);
}

void NixieBoard::writeToNixieRAW(uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t divider) // write data to nixie tubes RAW
{
    digitalWrite(_latchPin, LOW);
    shiftOut(_dataPin, _clockPin, MSBFIRST, divider);
    shiftOut(_dataPin, _clockPin, MSBFIRST, seconds);
    shiftOut(_dataPin, _clockPin, MSBFIRST, minutes);
    shiftOut(_dataPin, _clockPin, MSBFIRST, hours);
    digitalWrite(_latchPin, HIGH);
}

// I want to change this so instead of counting up 11 times, it just counts up to the desired number, passing by it ones.
// so to go from 5 to 0 -> 6,7,8,9,0
// This would make it inconsistant
// It would have to be a rolling affect from right to left
// since each digit will take a different amount of time
// current from 5 to 0 it goes -> 6,7,8,9,0,1,2,3,4,5,0
// If this was a real machine, it wouldnt do that
void NixieBoard::writeToNixieScroll(uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t divider) 
{

    uint8_t digitsToDisplay[6] = {
        (hours / 10) % 10, hours % 10, (minutes / 10) % 10, minutes % 10, (seconds / 10) % 10, seconds % 10};

    uint8_t lastDigitsDisplayed[6] = {
        (_currentHours / 10) % 10, _currentHours % 10, (_currentMinutes / 10) % 10, _currentMinutes % 10, (_currentSeconds / 10) % 10, _currentSeconds % 10};
    boolean hasDigitChanged[6] = {};
    for (int i = 0; i < 6; i++)
    {
        hasDigitChanged[i] = digitsToDisplay[i] != lastDigitsDisplayed[i];
    }

    // Attempt to do it digit by digit. only counts up one tube at a time. If all 6 change, it will take longer
    // Want to start at seconds
    //I think this meathod just counts until it hits 10. Need to experiment with it.
    for (int i = 5; i > -1; i--)
    {
        if (digitsToDisplay[i] != lastDigitsDisplayed[i])
        { // We need to scroll
            do
            {
                lastDigitsDisplayed[i]++;
                writeToNixieRAW((lastDigitsDisplayed[0] % 10) | ((lastDigitsDisplayed[1] % 10) << 4), //TODO: Is the mod 10 needed here? I think its already modded at this point
                                (lastDigitsDisplayed[2] % 10) | ((lastDigitsDisplayed[3] % 10) << 4),
                                (lastDigitsDisplayed[4] % 10) | ((lastDigitsDisplayed[5] % 10) << 4),
                                divider);
                delay(15);  //hardcoded scroll time
            } while (lastDigitsDisplayed[i] % 10 != digitsToDisplay[i] || lastDigitsDisplayed[i] < 11); //have to see what this looks like.
            //if its changed to 11, it will count way more
        }
    }

    // uint8_t workingScrollBuffer[6] = {(_currentHours / 10) % 10, _currentHours % 10, (_currentMinutes / 10) % 10, _currentMinutes % 10, (_currentSeconds / 10) % 10, _currentSeconds % 10};
    //This is a diffent way of doing the old meathod.should count up to the incorrect number then immeditaly clean up with the final writetonixie call
    /*
    for (int j = 0; j < 11; j++)
    {
        for (int i = 0; i < 6; i++)
        {

            if (hasDigitChanged[i])
            {
                lastDigitsDisplayed[i]++;
                if (lastDigitsDisplayed[i] > 9)
                {
                    lastDigitsDisplayed[i] = 0;
                }
            }
        }
        writeToNixieRAW((lastDigitsDisplayed[0] % 10) | ((lastDigitsDisplayed[1] % 10) << 4),
                        (lastDigitsDisplayed[2] % 10) | ((lastDigitsDisplayed[3] % 10) << 4),
                        (lastDigitsDisplayed[4] % 10) | ((lastDigitsDisplayed[5] % 10) << 4),
                        divider);
        delay(15);
    }
    writeToNixie(hours, minutes, seconds, divider); // fix any messy ones from bad logic above
    */
    _currentHours = hours;
    _currentMinutes = minutes;
    _currentSeconds = seconds;
}
void NixieBoard::antiPoison() // Rotate all digits. Will block the execution of the program
{
    int j;
    for (int i = 0; i < 10; i++)
    {
        j = i | ((i) << 4);
        writeToNixieRAW(j, j, j, 15);
        delay(1000);
    }
}

int NixieBoard::convertToNixe(uint8_t num) // convert 2 digit number to display on nixie tubes
{
    return num == 255 ? num : ((num / 10) % 10) | ((num % 10) << 4);
}
