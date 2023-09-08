#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

const char* ssid = "LANOfTheFree";
const char* password = "nathanisthebestwifiadmin";





void setup() {
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
  pinMode(13,OUTPUT);
}

void loop() {
  digitalWrite(13, HIGH);
  delay(1000);
  digitalWrite(13, LOW);
  
  // put your main code here, to run repeatedly:
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}