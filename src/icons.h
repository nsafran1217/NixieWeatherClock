#include <Arduino.h>

// test
const uint32_t testStatic[4] = {0x0001c700, 0x000f4104, 0x8000e380, 0x003c0820};
uint32_t allOn[4] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
uint32_t allOff[4] = {0, 0, 0, 0};

const uint32_t testAnimation[1 + (22 * 4)] = {
    0x001601F4, // 22 frames, 500 ms delay
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

const uint32_t defaultIcon[4] = {}; // Default icon (in case the code doesn't match any icon)

// day
const uint32_t icon_01d[4] = {}; // clear sky
const uint32_t icon_02d[4] = {}; // few clouds
const uint32_t icon_03d[4] = {}; // scattered clouds
const uint32_t icon_04d[4] = {}; // broken clouds
const uint32_t icon_09d[4] = {}; // shower rain
const uint32_t icon_10d[1 + (5 * 4)] = {
    0x00040096,
    0x823c8107, 0x00020410, 0x04f84330, 0x01140124,
    0x823c8107, 0x00020410, 0x04f84330, 0x05100901,
    0x803c8107, 0x00810420, 0x10f84330, 0x04120141,
    0x003c8107, 0x00410820, 0x12f84330, 0x05005044,
    0x413c8107, 0x00420800, 0x82f84330, 0x05101104}; // rain
const uint32_t icon_11d[4] = {};                     // thunderstorm
const uint32_t icon_13d[4] = {};                     // snow
const uint32_t icon_50d[1 + (8 * 4)] = {
    0x00070096,
    0x3f003007, 0x003f03f0, 0x3c03f030, 0x003c0300,
    0x3f007003, 0x001f03f0, 0x3e03f038, 0x003c0200,
    0x3f007003, 0x001f03f0, 0x3e03f038, 0x003c0200,
    0x3f00f001, 0x000f03f0, 0x3f03f03c, 0x003e0000,
    0x3f007003, 0x001f03f0, 0x3e03f038, 0x003e0200,
    0x3f003007, 0x003f03f0, 0x3c03f038, 0x003e0300,
    0x3f003007, 0x003f03f0, 0x3c03f038, 0x003e0300,
    0x3f003007, 0x003f03f0, 0x3c03f030, 0x003e0300}; // mist

// night
const uint32_t icon_01n[4] = {};     // clear sky
const uint32_t *icon_02n = icon_02d; // few clouds
const uint32_t *icon_03n = icon_03d; // scattered clouds
const uint32_t *icon_04n = icon_04d; // broken clouds
const uint32_t *icon_09n = icon_09d; // shower rain
const uint32_t *icon_10n = icon_10d; // rain
// const uint32_t icon_10n[4] = {};     // rain
const uint32_t *icon_11n = icon_11d; // thunderstorm
const uint32_t *icon_13n = icon_13d; // snow
const uint32_t *icon_50n = icon_50d; // mist
