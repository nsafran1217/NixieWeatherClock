#include <Arduino.h>

// test
const uint32_t testStatic[4] = {0x0001c700, 0x000f4104, 0x8000e380, 0x003c0820};
uint32_t matrixAllOn[4] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
uint32_t matrixAllOff[4] = {0, 0, 0, 0};

const uint32_t loadingAnimation[] =
    {0x00110032,
     0x820873cf, 0x03c81020, 0x10438f3c, 0x0f042104,
     0x830872cf, 0x03c81020, 0x10c38d3c, 0x0f042104,
     0x8308724f, 0x03c81020, 0x10c38d3c, 0x0f04210c,
     0x8308724f, 0x03c81030, 0x10c3893c, 0x0f04210c,
     0x8308720f, 0x03c81030, 0x10c3893c, 0x0f04a10c,
     0x8308720f, 0x03c91030, 0x10c3813c, 0x0f04a10c,
     0x8308720f, 0x03cb1030, 0x10c2813c, 0x0f04a10c,
     0x8308520f, 0x03cb1030, 0x10c2813c, 0x0f24a10c,
     0x8308420f, 0x03cb1030, 0x10c2813c, 0x0f34a10c,
     0x8308420f, 0x03cb1030, 0x10c0813c, 0x0f3ca10c,
     0x8308420f, 0x03cf1030, 0x1040813c, 0x0f3ca10c,
     0x8208420f, 0x03cf1030, 0x1040813c, 0x0f3ce10c,
     0x8208420f, 0x03cf1430, 0x1040813c, 0x0f3ce104,
     0x8208420f, 0x03cf1c20, 0x1040813c, 0x0f3ce104,
     0x10204081, 0x004107f8, 0x8ffe0820, 0x0830e3c7,
     0x209e8c00, 0x0030a278, 0xc7fc70c0, 0x00031ff1,
     0x20fc1041, 0x00421084, 0xdef38c20, 0x082083f3}; // hourglass

const uint32_t checkMarkIcon[] = {0x84210800, 0x00000010, 0x80000000, 0x0010a040};

const uint32_t testAnimation[] = {
    0x001600F4, // 22 frames, 500 ms delay
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

const uint32_t defaultIcon[] = {
    0x00010ffa,
    0x03104103, 0x00000000, 0x00008230, 0x08008208}; // Default icon (in case the code doesn't match any icon)

// day
const uint32_t icon_01d[] = {
    0x000300fa,
    0xc70c5200, 0x00081431, 0x38c28100, 0x0004a30e,
    0xc70c5210, 0x04081431, 0x38c28102, 0x0084a30e,
    0xc70c5200, 0x00081431, 0x38c28100, 0x0004a30e}; // clear sky
const uint32_t icon_02d[] = {
    0x0015012c,
    0x323040c2, 0x01c40d04, 0x00000800, 0x0fc185e0,
    0x323040c2, 0x03c81d04, 0x00000800, 0x0f820bc0,
    0x323040c2, 0x07d03914, 0x00000800, 0x0f041380,
    0x323040c2, 0x0fe07134, 0x00000800, 0x0e082300,
    0x323040c2, 0x0fc0e174, 0x00000800, 0x0c104200,
    0x323040c2, 0x0fc0c1f4, 0x00000800, 0x08208000,
    0x323040c2, 0x0fc187e4, 0x00000800, 0x00000000,
    0x323040c2, 0x0f820bc4, 0x00000800, 0x00000000,
    0x323040c2, 0x0f041384, 0x00000800, 0x00000000,
    0x323040c2, 0x0e082304, 0x00000800, 0x00000000,
    0x323040c2, 0x0c104304, 0x00000800, 0x00000000,
    0x323040c2, 0x08208104, 0x00000800, 0x00000000,
    0x323040c2, 0x00000104, 0x00000800, 0x00000000,
    0x323040c2, 0x00000104, 0x00000800, 0x00410000,
    0x323040c2, 0x00000104, 0x00000800, 0x00c20400,
    0x323040c2, 0x00000104, 0x00000800, 0x01c40c00,
    0x323040c2, 0x00000104, 0x00000800, 0x03c81c00,
    0x323040c2, 0x00000104, 0x00000800, 0x07d03810,
    0x323040c2, 0x00000104, 0x00000800, 0x0fe07030,
    0x323040c2, 0x00410104, 0x00000800, 0x0fc0e070,
    0x323040c2, 0x00c20504, 0x00000800, 0x0fc0c0f0}; // few clouds
const uint32_t icon_03d[] = {
    0x00010ffa,
    0x08107000, 0x0000fe04, 0x4e420000, 0x0000fc10}; // scattered clouds
const uint32_t icon_04d[] = {
    0x000800fa,
    0x3f862700, 0x000f2070, 0x00000000, 0x003e0a27,
    0x1f411380, 0x000f2070, 0x20800000, 0x003e0a27,
    0x1f411380, 0x000f2070, 0x20800000, 0x003e0a27,
    0x1f411380, 0x00071030, 0xa0800000, 0x003f0713,
    0x1f411380, 0x00071030, 0xa0800000, 0x003f0713,
    0x1f411380, 0x00071030, 0xa0800000, 0x003f0713,
    0x1f411380, 0x000f2070, 0x20800000, 0x003e0a27,
    0x3f862700, 0x000f2070, 0x00000000, 0x003e0a27}; // broken clouds
const uint32_t icon_09d[] = {
    0x00040096,
    0x823c8107, 0x00020410, 0x04f84330, 0x01140124,
    0x823c8107, 0x00020410, 0x04f84330, 0x05100901,
    0x803c8107, 0x00810420, 0x10f84330, 0x04120141,
    0x003c8107, 0x00410820, 0x12f84330, 0x05005044,
    0x413c8107, 0x00420800, 0x82f84330, 0x05101104}; // shower rain
const uint32_t icon_10d[] = {
    0x00040096,
    0x823c8107, 0x00020410, 0x04f84330, 0x01140124,
    0x823c8107, 0x00020410, 0x04f84330, 0x05100901,
    0x803c8107, 0x00810420, 0x10f84330, 0x04120141,
    0x003c8107, 0x00410820, 0x12f84330, 0x05005044,
    0x413c8107, 0x00420800, 0x82f84330, 0x05101104}; // rain
const uint32_t icon_11d[] = {
    0x00010ffa,
    0x087d03c0, 0x02022021, 0x30f8293c, 0x040830c6}; // thunderstorm
const uint32_t icon_13d[] = {
    0x000b00fa,
    0x08708000, 0x00000010, 0x0010e100, 0x00008308,
    0x1c200000, 0x00000402, 0x04384000, 0x0020c200,
    0x08000000, 0x00010087, 0x0e100000, 0x08308001,
    0x00000000, 0x004021c2, 0x84000000, 0x0c200043,
    0x00000001, 0x00c87080, 0x00000830, 0x0c0010e1,
    0x00000040, 0x02dc2000, 0x00020c20, 0x0c043840,
    0x00000040, 0x07c80000, 0x00020c20, 0x0d0e1000,
    0x00001000, 0x0fc00000, 0x00830800, 0x0f840000,
    0x0004021c, 0x0fc00000, 0x20c20000, 0x0fc00000,
    0x01008708, 0x0fc00000, 0x3080010e, 0x0fc00008,
    0x4021c200, 0x0fc00000, 0x20004384, 0x0fc0020c}; // snow
const uint32_t icon_50d[] = {
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
const uint32_t icon_01n[] = {
    0x000300fa,
    0x02000100, 0x08040100, 0x00100880, 0x00020100,
    0x02000100, 0x08040100, 0x00100880, 0x00020100,
    0x87080100, 0x08040100, 0x00100880, 0x00020100}; // clear sky
const uint32_t icon_02n[] = {
    0x0014012c,
    0x03010100, 0x08001d41, 0x30100888, 0x0100f823,
    0x07010100, 0x08003d82, 0x20100888, 0x0100f046,
    0x0f010100, 0x08007d04, 0x00100888, 0x0100e08c,
    0x5e010100, 0x0800fe08, 0x00100888, 0x0100c108,
    0xfc010100, 0x0800fc00, 0x00100888, 0x01008200,
    0xb8010100, 0x0800fc11, 0x00100888, 0x01000000,
    0x30010100, 0x0800f823, 0x00100888, 0x01000000,
    0x20010100, 0x0800f046, 0x00100888, 0x01000000,
    0x00010100, 0x0800e08c, 0x00100888, 0x01000000,
    0x00010100, 0x0800c108, 0x00100888, 0x01000000,
    0x00010100, 0x08008300, 0x00100888, 0x01000000,
    0x00010100, 0x08000100, 0x00100888, 0x01000000,
    0x00010100, 0x08000100, 0x40100888, 0x01000410,
    0x00010100, 0x08000100, 0x81100888, 0x01000c20,
    0x00010100, 0x08000100, 0x03100888, 0x01001c41,
    0x00010100, 0x08000100, 0x07100888, 0x01003c82,
    0x00010100, 0x08000100, 0x0f100888, 0x01007d04,
    0x00010100, 0x08000100, 0x5e100888, 0x0100fe08,
    0x40010100, 0x08000510, 0xfc100888, 0x0100fc00,
    0x81010100, 0x08000d20, 0xb8100888, 0x0100fc11}; // few clouds
const uint32_t *icon_03n = icon_03d;                 // scattered clouds
const uint32_t *icon_04n = icon_04d;                 // broken clouds
const uint32_t *icon_09n = icon_09d;                 // shower rain
const uint32_t *icon_10n = icon_10d;                 // rain
// const uint32_t icon_10n[4] = {};     // rain
const uint32_t *icon_11n = icon_11d; // thunderstorm
const uint32_t *icon_13n = icon_13d; // snow
const uint32_t *icon_50n = icon_50d; // mist
