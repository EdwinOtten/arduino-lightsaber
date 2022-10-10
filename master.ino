#include <Wire.h>

#define DEBUG 1

#ifdef DEBUG
  #define DEBUG_PRINT(x)  Serial.println (F(x))
#else
  #define DEBUG_PRINT(x)
#endif

const int buttonPin = 2;     // the number of the pushbutton pin

int btnState = 0;
int btnCount = 0;
long btnLastPressed = 0;

const int numberOfModes = 3;
byte mode = 0;


void setup()
{
	if (DEBUG)
    {
		Serial.begin(9600);
    }
  	pinMode(buttonPin, INPUT);
  	Wire.begin(); // join i2c bus (address optional for master)
	DEBUG_PRINT("setup complete");
}

void loop()
{
  	int newState = digitalRead(buttonPin);
  
	if (newState != btnState)
  	{
    	btnState = newState;
    	if (btnState == HIGH)
    	{
        	btnCount++;
    		btnLastPressed = millis();
    	}
  	}
  
	if (btnCount > 0 && btnLastPressed < millis()-500)
	{
    	handleBtnPress();          		
    	btnCount = 0;
	}
}

void handleBtnPress()
{
    switch (btnCount)
    {
      case 1:
        DEBUG_PRINT("single press");
      	sendMode(0);
        break;
      case 2:
        DEBUG_PRINT("double press");
      	sendMode(1);
        break;
      case 3:
        DEBUG_PRINT("triple press");
      	sendMode(2);
        break;
      case 4:
        DEBUG_PRINT("quad press");
      	sendMode(3);
        break;
    }
}

void sendMode(byte mode)
{
	Wire.beginTransmission(4); // transmit to device #4
	Wire.write(mode);          // sends one byte  
	Wire.endTransmission();    // stop transmitting
}
