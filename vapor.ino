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
#define btnPin 4
AdaEncoder encoder = AdaEncoder('x', encoderAPin, encoderBPin);
Button btn = Button(btnPin, LOW);

/* Timings (ms) */
#define messageDelay 1000
#define maxHeatDelay 5000
#define overHeatDelay 3000
#define overPressDelay 10000
#define shortClickDelay 250
#define multiClickDelay 500
#define autooffDelay 600000
#define autolockDelay 10000
#define capRefillDelay 5000

float batVoltage;
float heatResistance;
float batResistance;

boolean locked = true;
boolean powered = false;
boolean handleLCD_firstTime = true;
float desiredPower = 0;
float maxPower = 0;
byte heatLevel = 0;
byte pwmValue = 0;
unsigned long heatStartedTime = 0;
unsigned long overHeatTill = 0;
unsigned long btnPressedSince;
unsigned long lastBtnReleaseTime;
unsigned long autolockAt = 0;
unsigned long autooffAt = 0;

#define omega (byte)0
byte omegaGlyph[8] = {
  B00000,
  B00000,
  B01110,
  B10001,
  B10001,
  B01010,
  B11011,
};

byte glyphs[7][8] = {{
	B10000,
	B10000,
	B11111,
	B10001,
	B10001,
	B11111,
	B00000,
}, {
	B00000,
	B00000,
	B10001,
	B10001,
	B10001,
	B11111,
	B00000,
}, {
	B00000,
	B00000,
	B11111,
	B10001,
	B10001,
	B10001,
	B00000,
}, {
	B00000,
	B00000,
	B11111,
	B10000,
	B10000,
	B11111,
	B00000,
}, {
	B10000,
	B10000,
	B11111,
	B10001,
	B10001,
	B10001,
	B00000,
}, {
	B00000,
	B00000,
	B11111,
	B10001,
	B10011,
	B11101,
	B00000,
}, {
	B00000,
	B10000,
	B11000,
	B11100,
	B11000,
	B10000,
	B00000,
}};

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
	pinMode(btnPin, INPUT_PULLUP);

	lcd.createChar(omega, omegaGlyph);
	lcd.createChar(1, glyphs[0]);
	lcd.createChar(2, glyphs[1]);
	lcd.createChar(3, glyphs[2]);
	lcd.createChar(4, glyphs[3]);
	lcd.createChar(5, glyphs[4]);
	lcd.createChar(6, glyphs[5]);
	lcd.createChar(7, glyphs[6]);
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
			if (heatStartedTime + maxHeatDelay < now) {
				heat(0);
				overHeatTill = now + overHeatDelay;
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
	delay(messageDelay);
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
	delay(messageDelay);
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
		autolockAt = millis() + autolockDelay;
		locked = false;
		digitalWrite(backlightPin, true);
	}
}

void lockOn()
{
	autolockAt = 0;
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
	return (byte)(power / maxPower * 255.0 + 0.5); // 0.5 is here to "ceil" value
}

void loop()
{
	boolean acted = false;
	unsigned long now = millis();

	btn.listen();
	
	AdaEncoder *enc = NULL;
	enc = AdaEncoder::genie();
	if (enc != NULL) {
		int clicks = enc->getClearClicks();
		if (!locked) {
			if (btn.isPressed()) {
				heat(0);
				softReset();
			} else {
				desiredPower += clicks;
			}
		}
		acted = true;
	}

	handleMaxPower();
	if (desiredPower > maxPower) {
		desiredPower = (int)maxPower;
	} else if (desiredPower < 0) {
		desiredPower = 0;
	}
	pwmValue = convertPowerToPwm(desiredPower);

	if (powered && btn.onPress()) {
		btnPressedSince = now;
	}
	if (powered && btn.isPressed()) {
		if (btnPressedSince > 0 && btnPressedSince + overPressDelay <= now) {
			powerOff();
		}
		if (btnPressedSince + shortClickDelay <= now) {
			heat(pwmValue);
		}
		acted = true;
	} else if(btn.onRelease()) {
		heat(0);
		lastBtnReleaseTime = millis();
		if (btn.getReleaseCount() == 5) {
			btn.clearReleaseCount();
			powerToggle();
		}
		if (powered && btnPressedSince + shortClickDelay > now) {
			lockOff();
		}
		btnPressedSince = 0;
		acted = true;
	}

	if (lastBtnReleaseTime + multiClickDelay < now) {
		btn.clearReleaseCount();
	}

	if (acted) {
		autolockAt = now + autolockDelay;
		autooffAt = now + autooffDelay;
	}

	if (powered) {
		handleHeat();
		handleBatVoltage(now);
		handleLCD();

		if (autolockAt > 0 && autolockAt <= now) {
			lockOn();
		}

		if (autooffAt > 0 && autooffAt <= now) {
			powerOff();
		}
	}
}

void handleBatVoltage(unsigned long now)
{
	static long old_secs = 0;

	long secs = now / 1000;
	if (secs != old_secs && heatLevel == 0 && lastBtnReleaseTime + capRefillDelay < now) {
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
		lcd.setCursor(0, 1);
		lcd.print("     ");
		lcd.setCursor(0, 1);
		lcd.print(heatResistance);
		lcd.write(byte(0));
	}

	if (handleLCD_firstTime || old_batResistance != batResistance) {
		lcd.setCursor(6, 1);
		lcd.print("      ");
		lcd.setCursor(6, 1);
		lcd.print((int)(batResistance * 1000));
		lcd.print('m');
		lcd.write(byte(0));
	}

	boolean showWatts = millis() / 1000 % 4 >= 2;
	if (handleLCD_firstTime || old_pwmValue != pwmValue || old_batVoltage != batVoltage || old_heatResistance != heatResistance || old_batResistance != batResistance) {
		lcd.setCursor(8, 0);
		lcd.print("         ");
		lcd.setCursor(desiredPower >= 10 ? 8 : 9, 0);
		lcd.print((int)desiredPower);
		lcd.print("W ");
		float vheat = sqrt(pwmValue / 255.0 * maxPower * heatResistance);
		lcd.print((int)vheat);
		lcd.print('.');
		lcd.print((int)(10 * vheat) % 10);
		lcd.print('V');
	}

	if (handleLCD_firstTime || old_batVoltage != batVoltage) {
		lcd.setCursor(12, 1);
		lcd.print((int)batVoltage);
		lcd.print('.');
		lcd.print((int)(10 * batVoltage) % 10);
		lcd.print('V');
	}

	if (handleLCD_firstTime) {
		lcd.setCursor(0, 0);
		lcd.write(1);
		lcd.write(2);
		lcd.write(3);
		lcd.write(4);
		lcd.write(5);
		lcd.write(6);
		lcd.write(7);
	}

	old_heatResistance = heatResistance;
	old_batResistance = batResistance;
	old_pwmValue = pwmValue;
	old_batVoltage = batVoltage;
	handleLCD_firstTime = false;
}