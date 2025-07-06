//
//    FILE: ina226_usb_power_monitor.ino
//  AUTHOR: Prashant Kumar
// PURPOSE: Usbc Power Monitor using INA226
//     url: https://github.com/pk17r/INA226
// 
//  atmega328p Fuse settings to use internal clock and flash MiniCore Atmega328 using CP2102
//  L 0xE2
//  H 0xD6
//  E 0xFD
//  LB 0xFF


#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <INA226.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

INA226 INA(0x40, &Wire);

// #define RUN_CALIBRATION         // turn this off after calibration!

void INA226Setup();
const uint8_t y0 = 0, y1 = 18;

const uint8_t RotatePin = 2;

bool show_voltage_not_power = true;
uint8_t count_until_dim = 0;
const uint8_t kDimDisplayCount = 120;
bool invertDisplay = false;
const float kPowerLimitmW = 2500;

const uint8_t CURRENT=0, VOLTAGE=1, POWER=2;

volatile bool rotateDisplayFlag = false;
uint8_t rotation = 0;
unsigned long last_rotation_ms = 0;
unsigned long kMinRotationGapMs = 250;
void RotateDisplayISR() {
  if(!rotateDisplayFlag) {
    unsigned long now_ms = millis();
    if(now_ms - last_rotation_ms > kMinRotationGapMs) {
      last_rotation_ms = now_ms;
      rotateDisplayFlag = true;
    }
  }
}

void setup()
{
  #ifdef RUN_CALIBRATION
  Serial.begin(115200);
  #endif

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    // Serial.println(F("SSD1306 allocation failed"));
    delay(10);
  }
  display.clearDisplay();
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, y0);
  display.print("* INA226 *");
  display.setCursor(0, y1);
  display.print("VI MONITOR");
  display.dim(false);
  display.display();
  delay(1000);
  // Serial.println(F("SSD1306 allocation success"));

  pinMode(RotatePin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RotatePin), RotateDisplayISR, FALLING);

  INA226Setup();
  // display.setCursor(0, y0);
  // for(int i = 0; i < 30; i++)
  //   display.print((char)(i+65));
  // display.display();
  // delay(5000);
}

void displayValue(float value, uint8_t value_type) {
  int left_space = 2;
  bool milli = true;
  if(value >= 1000) {
    left_space += 3;
    value /= 1000;
    milli = false;
  }
  else {
    if(value >= 100)
      left_space += 1;
    else if(value >= 10)
      left_space += 2;
    else if(value >= 0)
      left_space += 3;
    else if(value > -10)
      left_space += 2;
    else if(value > -100)
      left_space += 1;
  }
  // suffix
  display.setCursor(SCREEN_WIDTH - (milli ? 26 : 13), (value_type == CURRENT ? y0 : y1));
  if(milli)
    display.print("m");
  display.print((value_type == CURRENT ? "A" : (value_type == VOLTAGE ? "V" : "W")));
  // prefix
  display.setCursor(0, (value_type == CURRENT ? y0 : y1));
  display.print((value_type == CURRENT ? "I" : (value_type == VOLTAGE ? "V" : "P")));
  // left gap/space
  display.setCursor(0, (value_type == CURRENT ? y0 : y1));
  while(left_space > 0) {
    display.print(" ");
    left_space--;
  }
  // value
  display.print(value, (milli ? 1 : 2));
}

void loop()
{

  /* MEASUREMENTS */

  // Serial.println("\nLOADV(V) CURRENT(mA) POWER(mW)");
  for (int i = 0; i < 40; i++)
  {
    float voltage_V = INA.getBusVoltage() - (float)INA.getShuntVoltage_mV() / 1000;
    float current_mA = INA.getCurrent_mA();
    float power_mW = voltage_V * current_mA;

    if(rotateDisplayFlag) {
      if(rotation == 0)
        rotation = 2;
      else
        rotation = 0;
      display.setRotation(rotation);
      count_until_dim = 0;
      display.dim(false);
      rotateDisplayFlag = false;
    }
    if(power_mW < 1) {
      show_voltage_not_power = true;
    }

    if(!invertDisplay && (power_mW >= kPowerLimitmW)) {
      invertDisplay = true;
      display.invertDisplay(invertDisplay);
    }
    else if(invertDisplay && (power_mW < kPowerLimitmW)) {
      invertDisplay = false;
      display.invertDisplay(invertDisplay);
    }

    // Serial.print(voltage_V, 3);
    // Serial.print("\t");
    // Serial.print(current_mA, 2);
    // Serial.print("\t");
    // Serial.print(power_mW, 1);
    // Serial.println();
    // Clear the buffer
    display.clearDisplay();
    displayValue(current_mA, CURRENT);

    if(show_voltage_not_power)
      displayValue(voltage_V*1000, VOLTAGE);
    else
      displayValue(power_mW, POWER);
    display.display();
    delay(10);

  }

  if(count_until_dim > 0)
    show_voltage_not_power = !show_voltage_not_power;

  if(count_until_dim == kDimDisplayCount) {
    display.dim(true);
  }
  else {
    count_until_dim++;
  }
}

void INA226Setup() {
  if (!INA.begin()) {
    #ifdef RUN_CALIBRATION
    Serial.println("could not connect. Fix and Reboot");
    #endif
  }

  /* STEPS TO CALIBRATE INA226
   * 1. Set shunt equal to shunt resistance in ohms. This is the shunt resistance between IN+ and IN- pins of INA226 in your setup.
   * 2. Set current_LSB_mA (Current Least Significant Bit mA) equal to your desired least count_until_dim resolution for IOUT in milli amps. Expected value to be in multiples of 0.050 o 0.010. Recommended values: 0.050, 0.100, 0.250, 0.500, 1, 2, 2.5 (in milli Ampere units).
   * 3. Set current_zero_offset_mA = 0, bus_V_scaling_e4 = 10000.
   * 4. Build firmware and flash microcontroller.
   * 5. Attach a power supply with voltage 5-10V to INA226 on VBUS/IN+ and GND pins, without any load.
   * 6. Start Serial Monitor and note Current values. Update current_zero_offset_mA = current_zero_offset_mA + average of 10 Current values in milli Amperes.
   * NOTE: Following adjustments shouldn't change values by more than 15-20%.
   * 7. Now measure Bus Voltage using a reliable Digital MultiMeter (DMM). Update bus_V_scaling_e4 = bus_V_scaling_e4 / (Displayed Bus Voltage on Serial Monitor) * (DMM Measured Bus Voltage). Can only be whole numbers.
   * 8. Now set DMM in current measurement mode. Use a resistor that will generate around 50-100mA IOUT measurement between IN- and GND pins with DMM in series with load. Note current measured on DMM.
   * 9. Update shunt = shunt * (Displayed IOUT on Serial Monitor) / (DMM Measured IOUT).
   * 10. Build firmware and flash microcontroller. Your INA 226 is now calibrated. It should have less than 1% error in Current and Voltage measurements over a wide range like [5mA, 1A] and [5V, 20V].
   */

  /* USER SET VALUES */

  const float shunt = 0.02122;                      /* shunt (Shunt Resistance in Ohms). Lower shunt gives higher accuracy but lower current measurement range. Recommended value 0.020 Ohm. Min 0.001 Ohm */
  const float current_LSB_mA = 0.10;              /* current_LSB_mA (Current Least Significant Bit in milli Amperes). Recommended values: 0.050, 0.100, 0.250, 0.500, 1, 2, 2.5 (in milli Ampere units) */
  const float current_zero_offset_mA = -0.100;         /* current_zero_offset_mA (Current Zero Offset in milli Amperes, default = 0) */
  const uint16_t bus_V_scaling_e4 = 9863;        /* bus_V_scaling_e4 (Bus Voltage Scaling Factor, default = 10000) */

  if(INA.configure(shunt, current_LSB_mA, current_zero_offset_mA, bus_V_scaling_e4)) {
    #ifdef RUN_CALIBRATION
    Serial.println("\n***** Configuration Error! Chosen values outside range *****\n");
    #endif
  }

  INA.setAverage(INA226_16_SAMPLES);

  /* CALIBRATION */

  #ifdef RUN_CALIBRATION
  display.clearDisplay();
  display.setCursor(0, y0);
  display.print("RS ");
  display.print(shunt, 4);
  display.setCursor(0, y1);
  display.print("bVs ");
  display.print(bus_V_scaling_e4);
  display.display();
  delay(1000);

  Serial.print("Shunt:\t");
  Serial.print(shunt, 4);
  Serial.println(" Ohm");
  Serial.print("current_LSB_mA:\t");
  Serial.print(current_LSB_mA * 1e+3, 1);
  Serial.println(" uA / bit");
  Serial.print("\nMax Measurable Current:\t");
  Serial.print(INA.getMaxCurrent(), 3);
  Serial.println(" A");

  float bv = 0, cu = 0;
  for (int i = 0; i < 10; i++) {
    bv += INA.getBusVoltage();
    cu += INA.getCurrent_mA();
    delay(150);
  }
  bv /= 10;
  cu /= 10;
  Serial.println("\nAverage Bus and Current values for use in Shunt Resistance, Bus Voltage and Current Zero Offset calibration:");
  bv = 0;
  for (int i = 0; i < 10; i++) {
    bv += INA.getBusVoltage();
    delay(100);
  }
  bv /= 10;
  Serial.print("\nAverage of 10 Bus Voltage values = ");
  Serial.print(bv, 3);
  Serial.println("V");
  cu = 0;
  for (int i = 0; i < 10; i++) {
    cu += INA.getCurrent_mA();
    delay(100);
  }
  cu /= 10;
  Serial.print("Average of 10 Current values = ");
  Serial.print(cu, 3);
  Serial.println("mA");

  // show calibration values on display
  display.clearDisplay();
  display.setCursor(0, y0);
  display.print(cu, 3);
  display.print(" mA");
  display.setCursor(0, y1);
  display.print(bv, 3);
  display.print(" V");
  display.display();
  delay(4000);
  #endif

  // Serial.println("\nCALIBRATION VALUES TO USE:\t(DMM = Digital MultiMeter)");
  // Serial.println("Step 5. Attach a power supply with voltage 5-10V to INA226 on VBUS/IN+ and GND pins, without any load.");
  // Serial.print("\tcurrent_zero_offset_mA = ");
  // Serial.print(current_zero_offset_mA + cu, 3);
  // Serial.println("mA");
  // if(cu > 5)
  //   Serial.println("********** NOTE: No resistive load needs to be present during current_zero_offset_mA calibration. **********");
  // Serial.print("\tbus_V_scaling_e4 = ");
  // Serial.print(bus_V_scaling_e4);
  // Serial.print(" / ");
  // Serial.print(bv, 3);
  // Serial.println(" * (DMM Measured Bus Voltage)");
  // Serial.println("Step 8. Set DMM in current measurement mode. Use a resistor that will generate around 50-100mA IOUT measurement between IN- and GND pins with DMM in series with load. Note current measured on DMM.");
  // Serial.print("\tshunt = ");
  // Serial.print(shunt);
  // Serial.print(" * ");
  // Serial.print(cu, 3);
  // Serial.println(" / (DMM Measured IOUT)");
  // if(cu < 40)
  //   Serial.println("********** NOTE: IOUT needs to be more than 50mA for better shunt resistance calibration. **********");
  // delay(1000);
}

//  -- END OF FILE --
