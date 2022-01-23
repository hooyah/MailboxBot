#include "battery.h"
#include "Arduino.h"

// IMPORTANT: has to run before initialising wifi!
float measure_battery_voltage()
{
const int adcpin = 13;
const float measure_min = 2296;
const float measure_max = 2590;
const float voltage_min = 3.45f;
const float voltage_max = 4.08f;

  // throw-away value
  analogRead(adcpin);
  delay(500);

  float val = 0;
  for(int c=0; c < 10; c++)
  {
    val += analogRead(adcpin);
    delay(200);
  }
  
  Serial.printf("m: %d\n", int(val));
  return (((val /= 10) - measure_min)/(measure_max-measure_min)) * (voltage_max-voltage_min) + voltage_min;
}
