/******************************************************************************
Heart_Rate_Display.ino
Demo Program for AD8232 Heart Rate sensor.
Casey Kuhns @ SparkFun Electronics
6/27/2014
https://github.com/sparkfun/AD8232_Heart_Rate_Monitor

The AD8232 Heart Rate sensor is a low cost EKG/ECG sensor.  This example shows
how to create an ECG with real time display.  The display is using Processing.
This sketch is based heavily on the Graphing Tutorial provided in the Arduino
IDE. http://www.arduino.cc/en/Tutorial/Graph

Resources:
This program requires a Processing sketch to view the data in real time.

Development environment specifics:
        IDE: Arduino 1.0.5
        Hardware Platform: Arduino Pro 3.3V/8MHz
        AD8232 Heart Monitor Version: 1.0

This code is beerware. If you see me (or any other SparkFun employee) at the
local pub, and you've found our code helpful, please buy us a round!

Distributed as-is; no warranty is given.
******************************************************************************/

// - The AD8232 is designed to be powered directly from a single 3V battery, such as CR2032 type.
// - Attention using Lithium-ion rechargable batteries, as the voltage during charge cycle may exceed the absolute max
// of AD8232
// - As in all linear circuits, bypass capacitors must be used to decouple the chip power supplies. Place a 0.1 μF
// capacitor close to the supply pin. A 1 μF capacitor can be used farther away from the part. In most cases, the
// capacitor can be shared by other integrated circuits. Keep in mind that excessive decoupling capacitance increases
// power dissipation during power cycling. (Figure 61 of datasheet. Check ADC's datasheet recommendation)
// - Output voltage range: 0.1 to Source - 0.1 (source is normally 3.3v). Rl 50k
// - AC leads off detection delay 110us (~9khz) (2 electrodes)
// - DC leads off, triggered when a lead is < source -0.5v. Right leg must be connected. LOD+ and LOD- identifies which
// Arm is off LOD+ and LOD- voltages: low 0.05; high 2.95 supply range: 2-3.5v
// - Analog ref of AD8232 is Source / 2 using a voltage divider
// - The Sparkfun circuit is designed as the Figure 66 of datasheet. "CARDIAC MONITOR CONFIGURATION" This configuration
// is designed for monitoring the shape of the ECG waveform. It assumes that the patient remains relatively still during
// the measurement, and therefore, motion artifacts are less of an issue.
// -- To obtain an ECG waveform with minimal distortion, the AD8232 is configured with a 0.5 Hz two-pole high-pass
// filter followed by a two-pole, 40 Hz, low-pass filter. A third electrode is driven for optimum common-mode rejection.
// -- In addition to 40 Hz filtering, the op amp stage is configured for a gain of 11, resulting in a total system gain
// of 1100. To optimize the dynamic range of the system, the gain level is adjustable, depending on the input signal
// amplitude (which may vary with electrode placement) and ADC input range.

//  Quantizing a sequence of numbers produces a sequence of quantization errors which is sometimes modeled as an
//  additive random signal called quantization noise because of its stochastic behavior. The more levels a quantizer
//  uses, the lower is its quantization noise power.

#include <Arduino.h>

void setup() {
  // initialize the serial communication:

  analogReference(EXTERNAL); // Maximum reading input (the top value of 1023, 10-bits)
  Serial.begin(9600);
  pinMode(10, INPUT); // Setup for leads off detection LO +
  pinMode(11, INPUT); // Setup for leads off detection LO -
}

void loop() {

  if ((digitalRead(10) == 1) || (digitalRead(11) == 1)) {
    Serial.println('!');
  } else {
    // send the value of analog input 0:
    Serial.println(analogRead(A0));
  }
  // Wait for a bit to keep serial data from saturating
  delay(1);
}
