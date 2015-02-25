const int analogInPin = A0;
const int batteryPin = A3;
const float batDivider = 2.0;
const float vcc = 4.7;
const int analogOutPin = 9;
const int buttonPin = 2;

const int dataPin = 4;
const int latchPin = 5;
const int clockPin = 6;

byte segments[10] = {
  0b11111100,
  0b01100000,
  0b11011010,
  0b11110010,
  0b01100110,
  0b10110110,
  0b10111110,
  0b11100000,
  0b11111110,
  0b11110110
};

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);

  pinMode(dataPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
}

int sensorValue;
boolean buttonPressed;
int outputValue;

void loop() {
  sensorValue = analogRead(analogInPin);
  
  buttonPressed = !digitalRead(buttonPin);
  if (buttonPressed) {
    outputValue = map(sensorValue, 0, 1023, 0, 255);
    analogWrite(analogOutPin, outputValue);
  } else {
    analogWrite(analogOutPin, 0);
  }

  int bat = analogRead(batteryPin);  
  print7(bat / 1023.0 * batDivider * vcc);
  
  delay(50);
}

void print7(double number)
{
  boolean canShow = true;
  int preNumber;
  byte p1 = 0;
  
  if (number < 100.0) {
    if (number >= 10.0) {
      preNumber = (int) floor(number);    
    } else if (number >= 0.0) {
      preNumber = (int) floor(number * 10.0);
      p1 = 1;
    } else {
      canShow = false;
    }
  } else {
    canShow = false;
  }

  digitalWrite(latchPin, LOW);  
  if (canShow) {
    if (preNumber == 100) {
      shiftOut(dataPin, clockPin, LSBFIRST, 0b10000000);
      shiftOut(dataPin, clockPin, LSBFIRST, 0b10000000);
    } else {
      shiftOut(dataPin, clockPin, LSBFIRST, segments[preNumber%10]);
      shiftOut(dataPin, clockPin, LSBFIRST, segments[preNumber/10] | p1);
    }
  } else {
    shiftOut(dataPin, clockPin, LSBFIRST, 0b00000010);
    shiftOut(dataPin, clockPin, LSBFIRST, 0b00000010);
  }
  digitalWrite(latchPin, HIGH);
}
