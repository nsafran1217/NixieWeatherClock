#include <Arduino.h>

// test
const uint32_t testStatic[4] = {0x0001c700, 0x000f4104, 0x8000e380, 0x003c0820};

const uint32_t testAnimation[1 + (22 * 4)] = {
    0x001601F4, //22 frames, 500 ms delay
    0x00000000, 0x00000000, 0x41041041, 0x00410410,
    0x00000000, 0x00000000, 0x82082082, 0x00820820,
    0x00000000, 0x00000000, 0x04104104, 0x01041041,
    0x00000000, 0x00000000, 0x08208208, 0x02082082,
    0x00000000, 0x00000000, 0x10410410, 0x04104104,
    0x00000000, 0x00000000, 0x20820820, 0x08208208,
    0x41041041, 0x00410410, 0x00000000, 0x00000000,
    0x82082082, 0x00820820, 0x00000000, 0x00000000,
    0x04104104, 0x01041041, 0x00000000, 0x00000000,
    0x08208208, 0x02082082, 0x00000000, 0x00000000,
    0x10410410, 0x04104104, 0x00000000, 0x00000000,
    0x20820820, 0x08208208, 0x00000000, 0x00000000,
    0x00000000, 0x0fc00000, 0x00000000, 0x0fc00000,
    0x00000000, 0x003f0000, 0x00000000, 0x003f0000,
    0x00000000, 0x0000fc00, 0x00000000, 0x0000fc00,
    0x00000000, 0x000003f0, 0x00000000, 0x000003f0,
    0xc0000000, 0x0000000f, 0xc0000000, 0x0000000f,
    0x3f000000, 0x00000000, 0x3f000000, 0x00000000,
    0x00fc0000, 0x00000000, 0x00fc0000, 0x00000000,
    0x0003f000, 0x00000000, 0x0003f000, 0x00000000,
    0x00000fc0, 0x00000000, 0x00000fc0, 0x00000000,
    0x0000003f, 0x00000000, 0x0000003f, 0x00000000};

// day
const uint32_t icon_01d[4] = {}; // clear sky
const uint32_t icon_02d[4] = {}; // few clouds
const uint32_t icon_03d[4] = {}; // scattered clouds
const uint32_t icon_04d[4] = {}; // broken clouds
const uint32_t icon_09d[4] = {}; // shower rain
const uint32_t icon_10d[4] = {}; // rain
const uint32_t icon_11d[4] = {}; // thunderstorm
const uint32_t icon_13d[4] = {}; // snow
const uint32_t icon_50d[4] = {}; // mist

// night
const uint32_t icon_01n[4] = {};    // clear sky
const uint32_t *icon_02n = icon_02d; // few clouds
const uint32_t *icon_03n = icon_03d; // scattered clouds
const uint32_t *icon_04n = icon_04d; // broken clouds
const uint32_t *icon_09n = icon_09d; // shower rain
const uint32_t icon_10n[4] = {};    // rain
const uint32_t *icon_11n = icon_11d; // thunderstorm
const uint32_t *icon_13n = icon_13d; // snow
const uint32_t *icon_50n = icon_50d; // mist
