#include <LiquidCrystal.h>
#include <ooPinChangeInt.h>
#include <AdaEncoder.h>
#include <Button.h>

//#define DEBUG

LiquidCrystal lcd(8, 7, 10, 11, 12, 13);

const int batDivPin = A3;
const float batDivK = 0.5;
const float refVoltage = 5.0;
const int heatPin = 3;
const int buttonPin = 2;
const int restestEnablePin = A0;
const int restestDivPin = A2;
const float restestDivK = 0.5;
const float restestRefResistance = 4.0;
const float restestFetResistance = 0.05;
const float heatWireResistance = 0.320;
const float heatFetResistance = 0.05;
const int backlightPin = 9;

const int encoderAPin = 5;
const int encoderBPin = 6;
const int encoderBtnPin = 4;
AdaEncoder encoder = AdaEncoder('x', encoderAPin, encoderBPin);
Button encoderBtn = Button(encoderBtnPin, LOW);
Button button = Button(buttonPin, LOW);

float vbat;
float rheat;
float rbat;

boolean locked = true;
boolean powered = false;
boolean handleLCD_firstTime = true;
void clearLcd()
{
	lcd.clear();
	handleLCD_firstTime = true;
}

void(* softReset) (void) = 0; //declare reset function at address 0

void setup()
{
	pinMode(buttonPin, INPUT_PULLUP);
	pinMode(restestEnablePin, OUTPUT);
	pinMode(backlightPin, OUTPUT);
	pinMode(encoderBtnPin, INPUT_PULLUP);
	
	#ifdef DEBUG
	Serial.begin(115200);
	Serial.println("---------- Booted ----------");
	#endif
	lcd.begin(16, 2);

	powerOn();

	lcd.setCursor(2, 0);
	lcd.print("Initializing");

	rheat = readHeatResistance();
	#ifdef DEBUG
	Serial.print("Rheat = ");
	Serial.println(rheat);
	#endif

	delay(1000);

	vbat = readBatVoltage();
	#ifdef DEBUG
	Serial.print("VBatUnload = ");
	Serial.println(vbat);
	#endif
	heat(255);
	float vl = readBatVoltage();
	heat(0);
	#ifdef DEBUG
	Serial.print("VBatLoad = ");
	Serial.println(vl);
	#endif
	rbat = (vbat / vl - 1) * (rheat + heatWireResistance + heatFetResistance);
	#ifdef DEBUG
	Serial.print("RBat = ");
	Serial.println(rbat);
	#endif

	powerOff();
}

float readHeatResistance()
{
	heat(0);

	digitalWrite(restestEnablePin, HIGH);
	float vb = readBatVoltage();
	float vr = readRestestVoltage();
	digitalWrite(restestEnablePin, LOW);
	#ifdef DEBUG
	Serial.print("Restest: vbat = ");
	Serial.println(vb);
	Serial.print("Restest: vres = ");
	Serial.println(vr);
	#endif
	float resistance = (vb / vr - 1) * (restestRefResistance + restestFetResistance) - heatWireResistance;

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

int heatLevel = 0;
unsigned long heatStartedTime = 0;
unsigned long overHeatTill = 0;
void heat(int level)
{
	if (overHeatTill > millis()) {
		level = 0;
	}
	if (level == 0 || powered) {
		heatLevel = level;
		analogWrite(heatPin, level);
	}
}
void handleHeat()
{
	if (heatLevel == 0) {
		heatStartedTime = 0;
	} else {
		unsigned long now = millis();
		if (heatStartedTime == 0) {
			heatStartedTime = now;
		} else if (heatLevel > 0 && heatStartedTime > 0) {
			if (heatStartedTime + 5000 < now) {
				heat(0);
				overHeatTill = now + 3000;
				heatStartedTime = 0;
			}
		}	
	}
}


byte levelValue = 0;
unsigned long lastBtnReleaseTime;
unsigned long lastEncBtnReleaseTime;

void powerToggle()
{
	powered ? powerOff() : powerOn();
}

void powerOff()
{
	heat(0);
	clearLcd();
	lcd.setCursor(3, 0);
	lcd.print("Power OFF");
	delay(1000);
	clearLcd();
	lockOn();
	powered = false;
}

void powerOn()
{
	powered = true;
	clearLcd();
	lcd.setCursor(3, 0);
	lcd.print("Power ON");
	lockOff();
	delay(1000);
	clearLcd();
}

void lockToggle()
{
	locked ? lockOff() : lockOn();
}

void lockOff()
{
	if (powered) {
		locked = false;
		digitalWrite(backlightPin, true);
	}
}

void lockOn()
{
	locked = true;
	digitalWrite(backlightPin, false);
}

boolean dontToggleLock = false;

void loop()
{
	encoderBtn.listen();
	if (encoderBtn.onRelease()) {
		lastEncBtnReleaseTime = millis();
		if (encoderBtn.getReleaseCount() == 5) {
			softReset();
		}
		if (dontToggleLock) {
			dontToggleLock = false;
		} else {
			lockToggle();
		}
	}

	if (lastEncBtnReleaseTime + 500 < millis()) {
		encoderBtn.clearReleaseCount();
	}
	
	AdaEncoder *enc = NULL;
	enc = AdaEncoder::genie();
	if (enc != NULL) {
		if (locked) {
			enc->getClearClicks();
		} else {
			levelValue = (levelValue + 101 + enc->getClearClicks()) % 101;
		}
	}

	button.listen();
	if (button.isPressed()) {
		heat(map(levelValue, 0, 100, 0, 255));
	} else if(button.onRelease()) {
		heat(0);

		lastBtnReleaseTime = millis();
		if (button.getReleaseCount() == 3 && encoderBtn.isPressed()) {
			button.clearReleaseCount();
			powerToggle();
			if (powered) {
				dontToggleLock = true;
			}
		}
	}

	if (lastBtnReleaseTime + 500 < millis()) {
		button.clearReleaseCount();
	}

	handleHeat();
	
	handleBatVoltage();

	// handleLPS();

	handleLCD();
}

long handleBatVoltage_old_secs = 0;
void handleBatVoltage()
{
	long secs = floor(millis() / 100.0);
	if (secs != handleBatVoltage_old_secs && heatLevel == 0) {
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
float handleLCD_old_rheat;
float handleLCD_old_rbat;
void handleLCD()
{
	// if (handleLCD_firstTime || handleLCD_old_LPS != LPS) {
	// 	lcd.setCursor(6, 1);
	// 	lcd.print("     ");
	// 	lcd.setCursor(6, 1);
	// 	lcd.print(LPS);
	// }

	if (handleLCD_firstTime || handleLCD_old_rheat != rheat) {
		lcd.setCursor(0, 0);
		lcd.print("     ");
		lcd.setCursor(0, 0);
		lcd.print(rheat);
		lcd.setCursor(3, 0);
		lcd.print((char)0xF4);
	}

	if (handleLCD_firstTime || handleLCD_old_rbat != rbat) {
		lcd.setCursor(5, 0);
		lcd.print("      ");
		lcd.setCursor(5, 0);
		lcd.print((int)(rbat * 1000));
		lcd.print('m');
		lcd.print((char)0xF4);
	}

	if (handleLCD_firstTime || handleLCD_old_levelValue != levelValue) {
		lcd.setCursor(0, 1);
		lcd.print("    ");
		lcd.setCursor(0, 1);
		lcd.print(map(levelValue, 0, 100, 0, 100));
		lcd.print('%');	
	}

	boolean showWatts = millis() / 1000 % 4 >= 2;
	if (handleLCD_firstTime || handleLCD_old_levelValue != levelValue || handleLCD_old_vbat != vbat || handleLCD_old_rheat != rheat || handleLCD_old_rbat != rbat) {
		lcd.setCursor(5, 1);
		lcd.print("           ");
		lcd.setCursor(5, 1);
		float rsum = rbat + rheat + heatWireResistance + heatFetResistance;
		#ifdef DEBUG
		Serial.println("Watts:");
		Serial.print("vbat = ");
		Serial.println(vbat);
		Serial.print("rsum = ");
		Serial.println(rsum);
		#endif
		float ipeak = vbat / rsum;
		#ifdef DEBUG
		Serial.print("ipeak = ");
		Serial.println(ipeak);
		#endif
		float pheatavg = levelValue / 100.0 * sq(ipeak) * rheat;
		#ifdef DEBUG
		Serial.print("pheatavg = ");
		Serial.println(pheatavg);
		Serial.println("/ Watts");
		#endif
		lcd.print(sqrt(pheatavg * rheat));
		lcd.print("V ");
		lcd.print(pheatavg);
		lcd.print('W');
	}

	if (handleLCD_firstTime || handleLCD_old_vbat != vbat) {
		lcd.setCursor(11, 0);
		lcd.print(vbat);
		lcd.print('V');
	}

	// handleLCD_old_LPS = LPS;
	handleLCD_old_rheat = rheat;
	handleLCD_old_rbat = rbat;
	handleLCD_old_levelValue = levelValue;
	handleLCD_old_vbat = vbat;
	handleLCD_firstTime = false;
}