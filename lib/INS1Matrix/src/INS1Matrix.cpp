/*
Library for my INS-1 Nixie Tube Matrix.
Nathan Safran - 9/10/2023
*/

#include "Arduino.h"
#include "INS1Matrix.h"

INS1Matrix::INS1Matrix(uint8_t dataPin, uint8_t clockPin, uint8_t latchPin, uint8_t blankPin, boolean inverted, uint8_t displays)
{
    pinMode(dataPin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(latchPin, OUTPUT);
    pinMode(blankPin, OUTPUT);
    digitalWrite(blankPin, LOW);
    _dataPin = dataPin;
    _clockPin = clockPin;
    _latchPin = latchPin;
    _blankPin = blankPin;
    _inverted = inverted;
    _displays = displays;
    _highValue = inverted ? LOW : HIGH; // if signals are inverted, swap the high and low levels we are going to use.
    _lowValue = inverted ? HIGH : LOW;
}

void INS1Matrix::writeStaticImgToDisplay(uint32_t imgData[]) // write 2 uint32_t to the display * the num of displays. imgData should be an array of uint32_t 2*displays big
{
    uint8_t bitValue;
    for (int i = 0; i < _displays * 2; i++) // displays*2 becuase each display needs two uint32_t's
    {
        for (int j = 0; j < 32; j++)
        {
            bitValue = _inverted ? !(imgData[i] & (1ul << j)) : !!(imgData[i] & (1ul << j));
            digitalWrite(_dataPin, bitValue);
            digitalWrite(_clockPin, _highValue);
            delayMicroseconds(7);
            digitalWrite(_clockPin, _lowValue);
        }
    }
    digitalWrite(_latchPin, _highValue);
    delayMicroseconds(7);
    digitalWrite(_latchPin, _lowValue);
    
}
void INS1Matrix::setAnimationToDisplay(const uint32_t* animationImgData) // write 2 uint32_t to display * the num of displays, how many frames are in the animation data, the delay between frames in ms. imgData should be a const array of frames (array of uint32_t 2*displays big)
{
    _animationImgData = animationImgData;

    _frameDelay = (animationImgData[0] & 0xFFFF);
    _frames = (animationImgData[0] >> 16) & 0xFFFF;
    _timeOfLastFrame = millis();
    _lastFrame = 0;

}
void INS1Matrix::animateDisplay()
{
    
    uint32_t currentMillis = millis();
    if ((currentMillis - _timeOfLastFrame) >= _frameDelay)
    {
        writeStaticImgToDisplay(const_cast<uint32_t *>(_animationImgData + 1 + (_lastFrame * _displays*2)));
		_lastFrame++;
		if (_lastFrame >= _frames)
		{
			_lastFrame = 0;
		}

        _timeOfLastFrame = currentMillis;
    }
}
