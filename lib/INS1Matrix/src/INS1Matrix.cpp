/*
Library for my INS-1 Nixie Tube Matrix.
Nathan Safran - 9/10/2023
*/

#include "Arduino.h"
#include "INS1Matrix.h"
#include <vector>

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
    _lastDataSentToDisplay = new uint32_t[2 * displays];
}
INS1Matrix::~INS1Matrix()
{
    // Release dynamically allocated memory in the destructor
    delete[] _lastDataSentToDisplay;
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
    memcpy(_lastDataSentToDisplay, imgData, sizeof(uint32_t) * _displays * 2); // for fancy transition
}
void INS1Matrix::setAnimationToDisplay(const uint32_t *animationImgData) // write 2 uint32_t to display * the num of displays, how many frames are in the animation data, the delay between frames in ms. imgData should be a const array of frames (array of uint32_t 2*displays big)
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
        writeStaticImgToDisplay(const_cast<uint32_t *>(_animationImgData + 1 + (_lastFrame * _displays * 2)));
        _lastFrame++;
        if (_lastFrame >= _frames)
        {
            _lastFrame = 0;
        }

        _timeOfLastFrame = currentMillis;
    }
}
void INS1Matrix::fancyTransitionFrame(const uint32_t *FirstFrameImgData, TransitionMode mode, int delayms) // transition between the old frame and a new frame
{
    std::vector<std::vector<uint32_t>> Pattern1stMask;
    std::vector<std::vector<uint32_t>> Pattern2ndMask;

    switch (mode)
    {
    case VERTICAL_BOUNCE:
        // const std::vector<uint32_t>
        Pattern1stMask = {
            {0xffffffff, 0x0fffffff, 0xffffffff, 0x0fffffff},
            {0xfffff000, 0x0fffffff, 0xfffff000, 0x0fffffff},
            {0xff000000, 0x0fffffff, 0xff000000, 0x0fffffff},
            {0x00000000, 0x0ffffff0, 0x00000000, 0x0ffffff0},
            {0x00000000, 0x0fff0000, 0x00000000, 0x0fff0000},
            {0x00000000, 0x00000000, 0x00000000, 0x00000000}};
        Pattern2ndMask = Pattern1stMask;
        break;
    case VERTICAL_DROP:
        Pattern1stMask = {
            {0xffffffff, 0x0fffffff, 0xffffffff, 0x0fffffff},
            {0xfffff000, 0x0fffffff, 0xfffff000, 0x0fffffff},
            {0xff000000, 0x0fffffff, 0xff000000, 0x0fffffff},
            {0x00000000, 0x0ffffff0, 0x00000000, 0x0ffffff0},
            {0x00000000, 0x0fff0000, 0x00000000, 0x0fff0000},
            {0x00000000, 0x00000000, 0x00000000, 0x00000000}};
        Pattern2ndMask = {
            {0xffffffff, 0x0fffffff, 0xffffffff, 0x0fffffff},
            {0xffffffff, 0x0000ffff, 0xffffffff, 0x0000ffff},
            {0xffffffff, 0x0000000f, 0xffffffff, 0x0000000f},
            {0x00ffffff, 0x00000000, 0x00ffffff, 0x00000000},
            {0x00000fff, 0x00000000, 0x00000fff, 0x00000000},
            {0x00000000, 0x00000000, 0x00000000, 0x00000000}};
        break;
    case ROTATE:
        Pattern1stMask = {
            {0xffffffff, 0x0fffffff, 0xffffffff, 0x0fffffff},
            {0xffffffff, 0x0c38f3ef, 0xffffffff, 0x0fffffff},
            {0x3fffffff, 0x0020c38f, 0xffffffff, 0x0fffffff},
            {0x3fffffff, 0x00000000, 0xffffffff, 0x0fffffff},
            {0x073dffff, 0x00000000, 0xffffffff, 0x0fffffff},
            {0x010c73df, 0x00000000, 0xffffffff, 0x0fffffff},
            {0x00000000, 0x00000000, 0xffffffff, 0x0fffffff},
            {0x00000000, 0x00000000, 0xfffdf3c7, 0x0fffffff},
            {0x00000000, 0x00000000, 0xdf3c70c1, 0x0fffffff},
            {0x00000000, 0x00000000, 0xcf1c1000, 0x0fffffff},
            {0x00000000, 0x00000000, 0xc0000000, 0x0fffffff},
            {0x00000000, 0x00000000, 0x00000000, 0x0ffffbce},
            {0x00000000, 0x00000000, 0x00000000, 0x0fbce308},
            {0x00000000, 0x00000000, 0x00000000, 0x0e38c200},
            {0x00000000, 0x00000000, 0x00000000, 0x00000000}};
        Pattern2ndMask = Pattern1stMask;
        break;
    }

    for (int bitMaskPos = 0; bitMaskPos < Pattern1stMask.size(); bitMaskPos++)
    {
        uint8_t bitValue;
        for (int i = 0; i < _displays * 2; i++)
        {
            uint32_t ImgData = _lastDataSentToDisplay[i] & Pattern1stMask[bitMaskPos][i];
            for (int j = 0; j < 32; j++)
            {
                bitValue = _inverted ? !(ImgData & (1ul << j)) : !!(ImgData & (1ul << j));
                digitalWrite(_dataPin, bitValue);
                digitalWrite(_clockPin, _highValue);
                delayMicroseconds(7);
                digitalWrite(_clockPin, _lowValue);
            }
        }
        digitalWrite(_latchPin, _highValue);
        delayMicroseconds(7);
        digitalWrite(_latchPin, _lowValue);
        delay(delayms);
    }

    for (int bitMaskPos = Pattern2ndMask.size() - 1; bitMaskPos >= 0; bitMaskPos--)
    {
        uint8_t bitValue;
        for (int i = 0; i < _displays * 2; i++) // displays*2 becuase each display needs two uint32_t's
        {

            uint32_t ImgData = FirstFrameImgData[i] & Pattern2ndMask[bitMaskPos][i];
            for (int j = 0; j < 32; j++)
            {
                bitValue = _inverted ? !(ImgData & (1ul << j)) : !!(ImgData & (1ul << j));
                digitalWrite(_dataPin, bitValue);
                digitalWrite(_clockPin, _highValue);
                delayMicroseconds(7);
                digitalWrite(_clockPin, _lowValue);
            }
        }
        digitalWrite(_latchPin, _highValue);
        delayMicroseconds(7);
        digitalWrite(_latchPin, _lowValue);
        delay(delayms);
    }
}
