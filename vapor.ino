#include <LiquidCrystal.h>
#include <ooPinChangeInt.h>
#include <AdaEncoder.h>
#include <Button.h>

LiquidCrystal lcd(8, 7, 10, 11, 12, 13);

const int batDivPin = A3;
const float batDivK = 0.5;
const float refVoltage = 5.0;
const int heatPin = 3;
const int buttonPin = 2;
const int restestEnablePin = 9;
const int restestDivPin = A2;
const float restestDivK = 0.5;
const float restestRefResistance = 29.0;
const float heatWireResistance = 0.2;
const int backlightPin = A1;

const int encoderAPin = 6;
const int encoderBPin = 5;
const int encoderBtnPin = 4;
AdaEncoder encoder = AdaEncoder('x', encoderAPin, encoderBPin);
Button encoderBtn = Button(encoderBtnPin, LOW);
Button button = Button(buttonPin, LOW);

float vbat;
float rheat;
float rbat;

void setup()
{
	pinMode(buttonPin, INPUT_PULLUP);
	pinMode(restestEnablePin, OUTPUT);
	pinMode(backlightPin, OUTPUT);
	digitalWrite(backlightPin, LOW);
	pinMode(encoderBtnPin, INPUT_PULLUP);

	rheat = readHeatResistance();
	lcd.begin(16, 2);
	lcd.setCursor(0, 0);
	lcd.print(rheat);
	lcd.setCursor(3, 0);
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

byte levelValue = 0;
boolean backlight = 1;

void loop()
{
	encoderBtn.listen();
	if (encoderBtn.onRelease()) {
		backlight = !backlight;
		digitalWrite(backlightPin, backlight);
	}

	AdaEncoder *enc = NULL;
	boolean changed = false;
	enc = AdaEncoder::genie();
	if (enc != NULL) {
		levelValue += enc->getClearClicks();
		levelValue = (levelValue + 100) % 100;
	}
	
	button.listen();
	if (button.isPressed()) {
		heat(map(levelValue, 0, 100, 0, 255));
	} else if(button.onRelease()) {
		heat(0);
	}
	
	handleBatVoltage();

	// handleLPS();

	handleLCD();
}

long handleBatVoltage_old_secs = 0;
void handleBatVoltage()
{
	long secs = floor(millis() / 100.0);
	if (secs != handleBatVoltage_old_secs) {
		vbat = readBatVoltage();
		handleBatVoltage_old_secs = secs;
	}
}


// int loops = 0;
// int LPS = 0;
// long handleLPS_old_secs = 0;
// void handleLPS()
// {
// 	loops++;
// 	long secs = floor(millis() / 1000.0);
// 	if (secs != handleLPS_old_secs) {
// 		LPS = loops;
// 		handleLPS_old_secs = secs;
// 		loops = 0;
// 	}
// }


// int handleLCD_old_LPS;
byte handleLCD_old_levelValue;
float handleLCD_old_vbat;
boolean handleLCD_firstTime = true;
void handleLCD()
{
	// if (handleLCD_firstTime || handleLCD_old_LPS != LPS) {
	// 	lcd.setCursor(6, 1);
	// 	lcd.print("     ");
	// 	lcd.setCursor(6, 1);
	// 	lcd.print(LPS);
	// }

	if (handleLCD_firstTime || handleLCD_old_levelValue != levelValue) {
		lcd.setCursor(0, 1);
		lcd.print("   ");
		lcd.setCursor(0, 1);
		lcd.print(map(levelValue, 0, 100, 0, 100));
		lcd.print('%');	
	}

	boolean showWatts = millis() / 1000 % 4 >= 2;
	if (handleLCD_firstTime || handleLCD_old_levelValue != levelValue || handleLCD_old_vbat != vbat) {
		lcd.setCursor(5, 1);
		lcd.print("           ");
		lcd.setCursor(5, 1);
		float ipeak = vbat / (rbat + rheat + heatWireResistance);
		float pheatavg = levelValue / 100.0 * sq(ipeak) * rheat;
		lcd.print(pheatavg);
		lcd.print("W ");
		lcd.print(sqrt(pheatavg * rheat));
		lcd.print('V');
	}

	if (handleLCD_firstTime || handleLCD_old_vbat != vbat) {
		lcd.setCursor(11, 0);
		lcd.print(vbat);
		lcd.print('V');
	}

	// handleLCD_old_LPS = LPS;
	handleLCD_old_levelValue = levelValue;
	handleLCD_old_vbat = vbat;
	handleLCD_firstTime = false;
}