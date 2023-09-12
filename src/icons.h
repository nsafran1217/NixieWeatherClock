#include <Arduino.h>



//day
const uint32_t icon_01d[64] = {};  //clear sky
const uint32_t icon_02d[64] = {};   //few clouds
const uint32_t icon_03d[64] = {};   //scattered clouds
const uint32_t icon_04d[64] = {};   //broken clouds
const uint32_t icon_09d[64] = {};   //shower rain
const uint32_t icon_10d[64] = {};   //rain
const uint32_t icon_11d[64] = {};   //thunderstorm
const uint32_t icon_13d[64] = {};   //snow
const uint32_t icon_50d[64] = {};   //mist

//night
const uint32_t icon_01n[64] = {};  //clear sky
const uint32_t *icon_02n = icon_02d;   //few clouds
const uint32_t *icon_03n = icon_03d;   //scattered clouds
const uint32_t *icon_04n = icon_04d;   //broken clouds
const uint32_t *icon_09n = icon_09d;   //shower rain
const uint32_t icon_10n[64] = {};   //rain
const uint32_t *icon_11n = icon_11d;   //thunderstorm
const uint32_t *icon_13n = icon_13d;   //snow
const uint32_t *icon_50n = icon_50d;   //mist
