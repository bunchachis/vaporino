#include <LiquidCrystal.h>

LiquidCrystal lcd(12, 11, 8, 7, 6, 5);

const int levelPin = A0;
const int batDivPin = A3;
const float batDivK = 0.5;
const float refVoltage = 5.0;
const int heatPin = 9;
const int buttonPin = 2;
const int restestEnablePin = 3;
const int restestDivPin = A2;
const float restestDivK = 0.5;
const float restestRefResistance = 29.0;
const float heatWireResistance = 0.2;

float vbat;
float rheat;
float rbat;

void setup()
{
	pinMode(buttonPin, INPUT_PULLUP);

	pinMode(restestEnablePin, OUTPUT);

	rheat = readHeatResistance();
	lcd.begin(16, 2);
	lcd.setCursor(0, 0);
	lcd.print(rheat);
	lcd.print((char)0xF4);

	lcd.print(' ');
	vbat = readBatVoltage();
	heat(255);
	float vl = readBatVoltage();
	heat(0);
	float rbat = (vbat / vl - 1) * rheat;
	lcd.print(rbat);
	lcd.print((char)0xF4);
}

float readHeatResistance()
{
	heat(0);

	digitalWrite(restestEnablePin, HIGH);
	float resistance = (readBatVoltage() / readRestestVoltage() - 1) * restestRefResistance - heatWireResistance;
	digitalWrite(restestEnablePin, LOW);

	return resistance;
}

float readBatVoltage()
{
	return readVoltage(batDivPin) / batDivK;
}

float readRestestVoltage()
{
	return readVoltage(restestDivPin) / restestDivK;
}

float readVoltage(int pin)
{
	analogRead(pin);
	delay(50);
	return analogRead(pin) * refVoltage / 1023.0;
}

void heat(int level)
{
	analogWrite(heatPin, level);
}

void loop()
{
	int levelValue = analogRead(levelPin);
	lcd.setCursor(12, 0);
	lcd.print("    ");
	lcd.setCursor(12, 0);
	lcd.print(map(levelValue, 0, 1023, 0, 100));
	lcd.print('%');
	lcd.setCursor(10, 1);
	lcd.print("      ");
	lcd.setCursor(10, 1);
	float i = vbat / (rbat + rheat + heatWireResistance);
	if (millis() / 1000 % 4 >= 2) {
		lcd.print(levelValue / 1023.0 * i * i * rheat);
		lcd.print('W');
	} else {
		lcd.print(levelValue / 1023.0 * i * rheat);
		lcd.print('V');
	}
	
	boolean buttonPressed = !digitalRead(buttonPin);
	if (buttonPressed) {
		int outputValue = map(levelValue, 0, 1023, 0, 255);
		heat(outputValue);
	} else {
		heat(0);
		vbat = readBatVoltage();
	}

	lcd.setCursor(0, 1);
	lcd.print(readBatVoltage());
	lcd.print('V');

	delay(50);
}
