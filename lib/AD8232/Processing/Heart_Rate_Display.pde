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

import processing.serial.*;

Serial myPort; // The serial port
int window_sizex, window_sizey;
int xPos_i = 1; // X increment
int xPos = xPos_i;  // horizontal position of the graph
float height_old = 0;
float height_new = 0;
float inByte = 0;
int BPM = 0;
int beat_old = 0;
int array_size = 10; // how many "beats" to store to calculate the average
float[] beats = new float[array_size]; // Used to calculate average BPM
int beatIndex;
int timeIndex, timeIndex_old, timeBase, currTime;
float threshold = 620.0; // Threshold at which BPM calculation occurs
boolean belowThreshold = true;
PFont font;
// A Table object to save data
Table dataframe;
// Save data, button
int rectX, rectY;      // Position of square button
int rectXsize, rectYsize;
color rectColor;
color rectHighlight;
color currentColor;
boolean rectOver = false;

// This program is capturing data at 300hz with 9600 and 500hz with 19200 920hz with 38400 and 930hz 57600 115200 230400

void setup() {
  window_sizex = 1500;
  window_sizey = 400;
  // set the window size:
  size(1500, 400); // increased width a little
  // List all the available serial ports
  println(Serial.list());
  // Open whatever port is the one you're using.
  myPort = new Serial(this, Serial.list()[1], 9600);
  // don't generate a serialEvent() unless you get a newline character:
  myPort.bufferUntil('\n');
  // set inital background:
  background(0xff);
  font = createFont("Arial", 12, true);

  timeBase = 0;
  timeIndex = 0;
  timeIndex_old = 0;

  dataframe = new Table();
  dataframe.addColumn("time");
  dataframe.addColumn("value");

  // setup save button
  rectColor = color(240);
  rectHighlight = color(0, 155, 155);
  rectXsize = 60;
  rectYsize = 30;
  rectX = window_sizex - rectXsize - 60;
  rectY = 10;
}

void draw() {
  // Map and draw the line for new data point
  float mapByte = map(inByte, 0, 1023, 0, height);
  height_new = height - mapByte;

  // at the edge of the screen, go back to the beginning:
  if (xPos >= width) {
    xPos = xPos_i;
    background(0xff);
  } else {
    // increment the horizontal position:
    xPos = xPos + xPos_i;
  }

  line(xPos - xPos_i, height_old, xPos, height_new);
  height_old = height_new;

  // draw save button periodically
  if (millis() % 2 == 0) {
    update(mouseX, mouseY);

    if (rectOver) {
      fill(rectHighlight);
    } else {
      fill(rectColor);
    }
    rect(rectX, rectY, rectXsize, rectYsize);
    fill(0x00);
    text("SAVE", rectX + rectXsize / 4, rectY + rectYsize / 1.5);
  }

  // draw text for BPM periodically
  if (millis() % 2 == 0) {
    fill(0xFF);
    rect(0, 0, 200, 20);
    fill(0x00);
    currTime = millis() - timeBase;
    text("Rate: " + (timeIndex / ((currTime / 1000) + 1)) + "hz" + " BPM: " + BPM, 15, 10);
  }
}

void update(int x, int y) {
  if (overRect(rectX, rectY, rectXsize, rectYsize)) {
    rectOver = true;
  } else {
    rectOver = false;
  }
}

void mousePressed() {
  if (rectOver) {
    saveTable(dataframe, "lastframe_" + timeIndex_old + "-" + (timeIndex - 1) + ".csv");
    timeIndex_old = timeIndex;
    dataframe.clearRows();
    rectOver = false;
  }
}

boolean overRect(int x, int y, int width, int height)  {
  if (mouseX >= x && mouseX <= x + width &&
    mouseY >= y && mouseY <= y + height) {
    return true;
  } else {
    return false;
  }
}

void serialEvent(Serial myPort) {
  // get the ASCII string:
  String inString = myPort.readStringUntil('\n');

  if (inString != null) {
    // trim off any whitespace:
    inString = trim(inString);

    // If leads off detection is true notify with blue line
    if (inString.equals("!")) {
      stroke(0, 0, 0xff); // Set stroke to blue ( R, G, B)
      inByte = 512.0;     // middle of the ADC range (Flat Line)
    }
    // If the data is good let it through
    else {
      stroke(0xff, 0, 0); // Set stroke to red ( R, G, B)
      inByte = float(inString);
      if (Float.isNaN(inByte))
        inByte = 0.0;

      // BPM calculation check
      if (inByte > threshold && belowThreshold == true) {
        calculateBPM();
        belowThreshold = false;
      } else if (inByte < threshold) {
        belowThreshold = true;
      }
    }

    // Save data
    if (timeIndex == 0) {
      timeBase = millis();
    }
    TableRow row = dataframe.addRow();
    // Set the values of that row
    row.setInt("time", timeIndex++);
    row.setFloat("value", inByte);
  }
}

void calculateBPM() {
  int beat_new = millis();         // get the current millisecond
  int diff = beat_new - beat_old;  // find the time between the last two beats
  if (diff > 0) { // solves the bug when diff equals zero
    float currentBPM = 60000 / diff; // convert to beats per minute
    beats[beatIndex] = currentBPM;   // store to array to convert the average
    float total = 0.0;
    for (int i = 0; i < array_size; i++) {
      total += beats[i];
    }
    BPM = int(total / array_size);
    beat_old = beat_new;
    beatIndex = (beatIndex + 1) % array_size; // cycle through the array instead of using FIFO queue
  }
}
