#include <LiquidCrystal.h>
#include <ooPinChangeInt.h>
#include <AdaEncoder.h>
#include <Button.h>

LiquidCrystal lcd(8, 7, 10, 11, 12, 13);

#define batDivPin A3
#define batDivK 0.5
#define refVoltage 5.0
#define heatPin 3
#define restestEnablePin A0
#define restestDivPin A2
#define restestDivK 0.5
#define restestRefResistance 4.0
#define restestFetResistance 0.05
#define heatFetResistance 0.05
#define backlightPin 9

#define encoderAPin 5
#define encoderBPin 6
#define encoderBtnPin 4
AdaEncoder encoder = AdaEncoder('x', encoderAPin, encoderBPin);
Button encoderBtn = Button(encoderBtnPin, LOW);

float batVoltage;
float heatResistance;
float batResistance;

boolean locked = true;
boolean powered = false;
boolean handleLCD_firstTime = true;
float desiredPower = 0;

byte heatLevel = 0;
byte pwmValue = 0;
unsigned long heatStartedTime = 0;
unsigned long overHeatTill = 0;
unsigned long btnPressedSince;
unsigned long lastBtnReleaseTime;
unsigned long lastEncBtnReleaseTime;
unsigned long autooffAt = 0;

boolean dontToggleLock = false;

float maxPower = 0;

void clearLcd()
{
	lcd.clear();
	handleLCD_firstTime = true;
}

void(* softReset) (void) = 0; //declare reset function at address 0

void setup()
{
	pinMode(heatPin, OUTPUT);
	digitalWrite(heatPin, 0);
	pinMode(restestEnablePin, OUTPUT);
	digitalWrite(restestEnablePin, 0);
	pinMode(backlightPin, OUTPUT);
	digitalWrite(backlightPin, 0);
	pinMode(encoderBtnPin, INPUT_PULLUP);
	
	lcd.begin(16, 2);

	powerOn();

	lcd.setCursor(2, 0);
	lcd.print("Initializing");

	heatResistance = readHeatResistance();

	delay(1000); // wait for supply cap refill

	batVoltage = readBatVoltage();
	batResistance = readBatResistance();

	powerOff();
}

float readBatResistance()
{
	heat(0);

	float unloadedVoltage = readBatVoltage();
	heat(255);
	float loadedVoltage = readBatVoltage();
	heat(0);
	
	float resistance = (unloadedVoltage / loadedVoltage - 1) * (heatResistance + heatFetResistance);

	return resistance;
}

float readHeatResistance()
{
	heat(0);

	digitalWrite(restestEnablePin, HIGH);
	float vb = readBatVoltage();
	float vr = readRestestVoltage();
	digitalWrite(restestEnablePin, LOW);

	float resistance = (vb / vr - 1) * (restestRefResistance + restestFetResistance);

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

void powerToggle()
{
	powered ? powerOff() : powerOn();
}

void powerOff()
{
	heat(0);
	clearLcd();
	digitalWrite(backlightPin, true);
	lcd.setCursor(3, 0);
	lcd.print("Power OFF");
	delay(1000);
	clearLcd();
	lockOn();
	powered = false;
	autooffAt = 0;
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
	autooffAt = 0;
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

void handleMaxPower()
{
	float rsum = batResistance + heatResistance + heatFetResistance;
	maxPower = sq(batVoltage / rsum) * heatResistance;
}

byte convertPowerToPwm(float power)
{
	if (power > maxPower) {
		power = maxPower;
	} else if (power < 0) {
		power = 0;
	}
	return (byte)(power / maxPower * 255.0 + 0.5);
}

void loop()
{
	boolean acted = false;
	unsigned long now = millis();

	encoderBtn.listen();
	if (encoderBtn.onRelease()) {
		lastEncBtnReleaseTime = now;
		if (dontToggleLock) {
			dontToggleLock = false;
		} else {
			lockToggle();
		}
		acted = true;
	}

	if (lastEncBtnReleaseTime + 500 < now) {
		encoderBtn.clearReleaseCount();
	}
	
	AdaEncoder *enc = NULL;
	enc = AdaEncoder::genie();
	if (enc != NULL) {
		if (locked) {
			enc->getClearClicks();
			if (powered && encoderBtn.isPressed()) {
				heat(0);
				softReset();
			}
		} else {
			desiredPower += enc->getClearClicks() * 0.5;
		}
		acted = true;
	}

	handleMaxPower();
	if (desiredPower > maxPower) {
		desiredPower = (int)(maxPower * 2) * 0.5;
	} else if (desiredPower < 0) {
		desiredPower = 0;
	}
	pwmValue = convertPowerToPwm(desiredPower);

	if (powered && encoderBtn.onPress()) {
		btnPressedSince = now;
	}
	if (powered && encoderBtn.isPressed()) {
		if (btnPressedSince > 0 && btnPressedSince + 10000 <= now) {
			powerOff();
		}
		heat(pwmValue);
		acted = true;
	} else if(encoderBtn.onRelease()) {
		heat(0);
		btnPressedSince = 0;
		lastBtnReleaseTime = millis();
		if (encoderBtn.getReleaseCount() == 5) {
			encoderBtn.clearReleaseCount();
			powerToggle();
			if (powered) {
				dontToggleLock = true;
			}
		}
		acted = true;
	}

	if (lastBtnReleaseTime + 500 < now) {
		encoderBtn.clearReleaseCount();
	}

	handleHeat();
	
	handleBatVoltage();

	handleLCD();

	if (acted) {
		autooffAt = now + 300000;
	}
	if (autooffAt > 0 && autooffAt <= now) {
		powerOff();
	}
}

void handleBatVoltage()
{
	static long old_secs = 0;

	long secs = floor(millis() / 100.0);
	if (secs != old_secs && heatLevel == 0) {
		batVoltage = readBatVoltage();
		old_secs = secs;
	}
}

void handleLCD()
{
	static byte old_pwmValue;
	static float old_batVoltage;
	static float old_heatResistance;
	static float old_batResistance;

	if (handleLCD_firstTime || old_heatResistance != heatResistance) {
		lcd.setCursor(0, 0);
		lcd.print("     ");
		lcd.setCursor(0, 0);
		lcd.print(heatResistance);
		lcd.setCursor(3, 0);
		lcd.print((char)0xF4);
	}

	if (handleLCD_firstTime || old_batResistance != batResistance) {
		lcd.setCursor(5, 0);
		lcd.print("      ");
		lcd.setCursor(5, 0);
		lcd.print((int)(batResistance * 1000));
		lcd.print('m');
		lcd.print((char)0xF4);
	}

	boolean showWatts = millis() / 1000 % 4 >= 2;
	if (handleLCD_firstTime || old_pwmValue != pwmValue || old_batVoltage != batVoltage || old_heatResistance != heatResistance || old_batResistance != batResistance) {
		lcd.setCursor(0, 1);
		lcd.print("                ");
		lcd.setCursor(0, 1);
		lcd.print((int)desiredPower);
		lcd.print('.');
		lcd.print((int)(10 * desiredPower) % 10);
		lcd.print("W ");
		float vheat = sqrt(pwmValue / 255.0 * maxPower * heatResistance);
		lcd.print((int)vheat);
		lcd.print('.');
		lcd.print((int)(10 * vheat) % 10);
		lcd.print("V (");
		lcd.print(pwmValue);
		lcd.print(')');	
	}

	if (handleLCD_firstTime || old_batVoltage != batVoltage) {
		lcd.setCursor(11, 0);
		lcd.print(batVoltage);
		lcd.print('V');
	}

	old_heatResistance = heatResistance;
	old_batResistance = batResistance;
	old_pwmValue = pwmValue;
	old_batVoltage = batVoltage;
	handleLCD_firstTime = false;
}