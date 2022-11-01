#include <Wire.h>

#define DEBUG 1

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.println(F(x))
#else
#define DEBUG_PRINT(x)
#endif

/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a, b) ((a) |= (1ULL << (b)))
#define BIT_CLEAR(a, b) ((a) &= ~(1ULL << (b)))
#define BIT_FLIP(a, b) ((a) ^= (1ULL << (b)))
#define BIT_CHECK(a, b) (!!((a) & (1ULL << (b)))) // '!!' to make sure this returns 0 or 1

#define BITMASK_SET(x, mask) ((x) |= (mask))
#define BITMASK_CLEAR(x, mask) ((x) &= (~(mask)))
#define BITMASK_FLIP(x, mask) ((x) ^= (mask))
#define BITMASK_CHECK_ALL(x, mask) (!(~(x) & (mask)))
#define BITMASK_CHECK_ANY(x, mask) ((x) & (mask))

#define NUMBER_OF_COLORS 5

#define BUTTON_PIN 2 // the number of the pushbutton pin

int btnState = 0;
int btnCount = 0;
long btnLastPressed = 0;

unsigned long debounceDuration = 50; // millis

byte mode = 0;

void setup()
{
	if (DEBUG)
	{
		Serial.begin(9600);
	}
	pinMode(BUTTON_PIN, INPUT_PULLUP);
	pinMode(LED_BUILTIN, OUTPUT);
	Wire.begin(); // join i2c bus (address optional for master)
	DEBUG_PRINT("setup complete");
}

void loop()
{
  if (millis() - btnLastPressed < debounceDuration) {
    return;
  }
  
	int newState = digitalRead(BUTTON_PIN);

	if (newState != btnState)
	{
		btnState = newState;
		if (btnState == HIGH)
		{
			btnCount++;
			btnLastPressed = millis();
		}
	}

	if (btnCount > 0 && btnLastPressed < millis() - 500)
	{
		handleBtnPress(btnCount);
		btnCount = 0;
	}
}

void handleBtnPress(int buttonPressed)
{
	switch (buttonPressed)
	{
		case 0:
		{
			DEBUG_PRINT("zero press");
		}
		break;
		case 1: // togle blade (on/off)
		{
			DEBUG_PRINT("single press");
			BIT_FLIP(mode, 7);
		}
		break;
		case 2: // iterate through colors
		{
			DEBUG_PRINT("double press");
			int color = BITMASK_CHECK_ANY(mode, 0x0F);
			color++;
			if (color >= NUMBER_OF_COLORS)
			{
				color = 0;
			}
			BITMASK_CLEAR(mode, 0x0F);
			BITMASK_SET(mode, color);
		}
		break;
		case 3: // toggle saber flickering
		{
			DEBUG_PRINT("triple press");
			BIT_FLIP(mode, 6);
		}
		break;
		case 4: // rainbow mode
		default:
		{
			DEBUG_PRINT("quad press");
			BITMASK_CLEAR(mode, 0x0F);
			BITMASK_SET(mode, 0x0F);
		}
		break;
	}
	sendByte(mode);
}

void printBinary(byte inByte)
{
	for (int b = 7; b >= 0; b--)
	{
		Serial.print(bitRead(inByte, b));
	}
	Serial.println();
}

void sendByte(byte byte)
{
	printBinary(byte);
	Wire.beginTransmission(4); // transmit to device #4
	Wire.write(byte);		   // sends one byte
	Wire.endTransmission();	   // stop transmitting
	digitalWrite(LED_BUILTIN, HIGH);
	delay(300);
	digitalWrite(LED_BUILTIN, LOW);
}
