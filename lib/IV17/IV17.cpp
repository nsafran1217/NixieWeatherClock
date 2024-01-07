/*
Library for IV-17 and IV-4 Sixteen segment alphanumeric soviet VFD tube
Nathan Safran - 9/9/2023
*/

#include "Arduino.h"
#include "IV17.h"

IV17::IV17(uint8_t dataPin, uint8_t clockPin, uint8_t latchPin, uint8_t blankPin, uint8_t numOfTubes)
{
    pinMode(dataPin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(latchPin, OUTPUT);
    pinMode(blankPin, OUTPUT);
    _dataPin = dataPin;
    _clockPin = clockPin;
    _latchPin = latchPin;
    _blankPin = blankPin;
    _numOfTubes = numOfTubes;
    _timeSinceLastScroll = millis();
    _scrollIndex = 0;
    _lastStringTransitionedBits = new uint32_t[numOfTubes];
}
IV17::~IV17()
{
    // Release dynamically allocated memory in the destructor
    delete[] _lastStringTransitionedBits;
}
void IV17::shiftOutChar(char c, boolean latch)
{
    if (c < 0x80)
    {                                                             // check if valid ASCII
        shiftOut20Bits(MSBFIRST, _asciiLookupIV17[c] | _gridPin); // always make sure the grid pin is on
    }
    else
    {
        shiftOut20Bits(MSBFIRST, _asciiLookupIV17['?'] | _gridPin);
    }
    if (latch)
    {
        digitalWrite(_latchPin, LOW);
        digitalWrite(_latchPin, HIGH);
    }
}

void IV17::shiftOutCyrillicChar(const char c[], boolean latch)
{
    if (((c[0] & 0xE0) == 0xC0) && // check if valid UTF-8 chars that we have in the table
        ((c[1] & 0xC0) == 0x80 &&
         (c[1] < 0xb0)))
    {
        shiftOut20Bits(MSBFIRST, _cyrillicLookupIV17[(c[1] - 0x80)] | _gridPin); // always make sure the grid pin is on
    }
    else
    {
        shiftOut20Bits(MSBFIRST, _asciiLookupIV17['?'] | _gridPin);
    }
    if (latch)
    {
        digitalWrite(_latchPin, LOW);
        digitalWrite(_latchPin, HIGH);
    }
}
void IV17::shiftOutString(String s)
{
    digitalWrite(_latchPin, LOW);

    char cc[3] = {0, 0, 0};
    for (int i = s.length() - 1; i >= 0; i--) // need to do reverse order because hardware shifts in from left to right.
    {
        if (s[i] > 0x80)
        { // UTF-8 char
            cc[0] = s[i - 1];
            cc[1] = s[i];
            i--;
            shiftOutCyrillicChar(cc, false);
        }
        else
        {
            shiftOutChar(s[i], false);
        }
    }
    digitalWrite(_latchPin, HIGH);
}

void IV17::fancyTransitionString(String s, int TransitionMode, int delayms)
{                                             // This needs to have been called once before the transition works
    uint32_t stringToTransitionBits[6] = {0}; // array to store bits to be shifted for each char. Will mask this for trasnisiton effect
    char cc[3] = {0, 0, 0};
    int charPos = 0;
    for (int i = s.length() - 1; i >= 0; i--) // need to do reverse order because hardware shifts in from left to right.
    {
        if (s[i] > 0x80)
        { // UTF-8 char
            cc[0] = s[i - 1];
            cc[1] = s[i];
            i--;

            if (((cc[0] & 0xE0) == 0xC0) && // check if valid UTF-8 chars that we have in the table
                ((cc[1] & 0xC0) == 0x80 &&
                 (cc[1] < 0xb0)))
            {
                stringToTransitionBits[charPos] = _cyrillicLookupIV17[(cc[1] - 0x80)];
            }
            else
            {
                stringToTransitionBits[charPos] = _asciiLookupIV17['?'];
            }
        }
        else
        {
            stringToTransitionBits[charPos] = _asciiLookupIV17[s[i]];
        }
        charPos++;
    }
    const uint32_t *pattern1stMask;
    const uint32_t *pattern2ndMask;
    int patternSize = 0;
    if (TransitionMode == IV17_VERTICAL_BOUNCE_TRANSITION)
    {
        patternSize = _verticalBouncePatternSize;
        pattern1stMask = _verticalBounceMask;
        pattern2ndMask = pattern1stMask;
    }
    else if (TransitionMode == IV_17_ROTATE_TRANSITION)
    {
        patternSize = _rotatePatternSize;
        pattern1stMask = _rotatePattern1stMask;
        pattern2ndMask = _rotatePattern2ndMask;
    }
    else
    {
    }
    for (int i = 0; i < patternSize; i++)
    {
        digitalWrite(_latchPin, LOW);
        for (charPos = 0; charPos < _numOfTubes; charPos++)
        {
            shiftOut20Bits(MSBFIRST, _lastStringTransitionedBits[charPos] & pattern1stMask[i] | _gridPin);
        }
        digitalWrite(_latchPin, HIGH);
        delay(delayms);
    }

    for (int i = patternSize - 1; i >= 0; i--)
    {
        digitalWrite(_latchPin, LOW);
        for (charPos = 0; charPos < _numOfTubes; charPos++)
        {
            shiftOut20Bits(MSBFIRST, stringToTransitionBits[charPos] & pattern2ndMask[i] | _gridPin);
        }
        digitalWrite(_latchPin, HIGH);
        delay(delayms);
    }
    memcpy(_lastStringTransitionedBits, stringToTransitionBits, sizeof(uint32_t) * _numOfTubes);
}

void IV17::setScrollingString(String s, int delay)
{
    _scrollingString = s;
    _delayToScroll = delay;
    _scrollIndex = 0;
    _spaceIndex = 0;
}

void IV17::scrollStringSync()
{ // synchronous scroll display. AKA blocking until entire string scrolls.
    scrollString();
    while (_scrollIndex <= (_scrollingString.length()) && _scrollIndex != 0)
    {
        scrollString();
    }
}
void IV17::scrollString()
{ // call in main loop for async scroll.

    unsigned long currentMillis = millis();
    if ((currentMillis - _timeSinceLastScroll) > _delayToScroll)
    {
        digitalWrite(_latchPin, LOW);
        int endIndex = _scrollIndex;
        int startIndex = _scrollIndex;
        int charCount = 0;
        String spaces = "";
        if (_spaceIndex <= _numOfTubes)
        {
            for (int i = 0; i < _numOfTubes - _spaceIndex; i++)
            {
                spaces += ' ';
                charCount++;
            }
            _spaceIndex++;
            _scrollIndex = 0;
        }

        while (charCount < _numOfTubes && endIndex < _scrollingString.length())
        {
            if (_scrollingString[endIndex] < 0x80) // ascii
            {
                charCount++;
                endIndex++;
            }
            else
            {
                if (((_scrollingString[endIndex] & 0xE0) == 0xC0) &&
                    (endIndex + 1 < _scrollingString.length()) &&
                    ((_scrollingString[endIndex + 1] & 0xC0) == 0x80 &&
                     (_scrollingString[endIndex + 1] < 0xb0)))
                {
                    charCount++;   // Increment character count
                    endIndex += 2; // Move to the next character
                }
                else
                {
                    // Invalid UTF-8 sequence, skip it
                    endIndex++;
                }
            }
        }

        String substringToDisplay = spaces + _scrollingString.substring(_scrollIndex, endIndex);
        while (charCount < _numOfTubes)
        {
            substringToDisplay += ' '; // Add spaces to fill the remaining characters
            charCount++;
        }
        shiftOutString(substringToDisplay);
        if (_scrollingString[_scrollIndex] == 0xd0)
        {
            _scrollIndex++;
        }
        _scrollIndex++;

        // Reset to the beginning of the string when the end is reached
        if (_scrollIndex >= _scrollingString.length())
        {
            _scrollIndex = 0;
            _spaceIndex = 0;
        }
        digitalWrite(_latchPin, HIGH);
        _timeSinceLastScroll = millis();
    }
}

void IV17::shiftOut20Bits(uint8_t bitOrder, uint32_t val)
{
    uint8_t i;
    for (i = 0; i < 20; i++)
    {
        if (bitOrder == LSBFIRST)
            digitalWrite(_dataPin, !!(val & (1ul << i))); // 1ul tells it to use an unisgned long. If we use just 1, its only 16 bits
        else
            digitalWrite(_dataPin, !!(val & (1ul << (19 - i))));

        digitalWrite(_clockPin, HIGH);
        digitalWrite(_clockPin, LOW);
    }
}
