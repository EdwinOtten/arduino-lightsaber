#include <Wire.h>

#define DEBUG 1

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.println(x)
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

const int led = LED_BUILTIN; // the pin with a LED

unsigned int i = 0;
byte mode = 0;
byte newMode = 0;
bool newModeAvailable = false;

// colors in R, G, B
const uint8_t colors[5][3] = { 
  { 10, 90, 180 }, // blue
  { 180, 0, 0 }, // red
  { 160, 0, 180 }, // purple
  { 10, 200, 0 }, // green
  { 180, 30, 0 }, // amber
};
uint8_t color[3] = { 0, 0, 0 };

/*
 This is an example of how simple driving a Neopixel can be
 This code is optimized for understandability and changability rather than raw speed
 More info at http://wp.josh.com/2014/05/11/ws2812-neopixels-made-easy/
*/

// Change this to be at least as long as your pixel string (too long will work fine, just be a little slower)
#define PIXELS 144  // Number of pixels in the string

// These values depend on which pin your string is connected to and what board you are using 
// More info on how to find these at http://www.arduino.cc/en/Reference/PortManipulation

// These values are for the pin that connects to the Data Input pin on the LED strip. They correspond to...

// Arduino Yun:     Digital Pin 8
// DueMilinove/UNO: Digital Pin 12
// Arduino MeagL    PWM Pin 4

// You'll need to look up the port/bit combination for other boards. 

// Note that you could also include the DigitalWriteFast header file to not need to to this lookup.

#define PIXEL_PORT  PORTD  // Port of the pin the pixels are connected to
#define PIXEL_DDR   DDRD   // Port of the pin the pixels are connected to
#define PIXEL_BIT   6      // Bit of the pin the pixels are connected to

// These are the timing constraints taken mostly from the WS2812 datasheets 
// These are chosen to be conservative and avoid problems rather than for maximum throughput 

#define T1H  900    // Width of a 1 bit in ns
#define T1L  600    // Width of a 1 bit in ns

#define T0H  400    // Width of a 0 bit in ns
#define T0L  900    // Width of a 0 bit in ns

// Older pixels would reliabily reset with a 6us delay, but some newer (>2019) ones
// need 250us. This is set to the longer delay here to make sure things work, but if
// you want to get maximum refresh speed, you can try decreasing this until your pixels
// start to flicker. 

#define RES 250000    // Width of the low gap between bits to cause a frame to latch

// Here are some convience defines for using nanoseconds specs to generate actual CPU delays

#define NS_PER_SEC (1000000000L)          // Note that this has to be SIGNED since we want to be able to check for negative values of derivatives

#define CYCLES_PER_SEC (F_CPU)

#define NS_PER_CYCLE ( NS_PER_SEC / CYCLES_PER_SEC )

#define NS_TO_CYCLES(n) ( (n) / NS_PER_CYCLE )

// Actually send a bit to the string. We must to drop to asm to enusre that the complier does
// not reorder things and make it so the delay happens in the wrong place.

inline void sendBit( bool bitVal ) {
  
    if (  bitVal ) {        // 0 bit
      
    asm volatile (
      "sbi %[port], %[bit] \n\t"        // Set the output bit
      ".rept %[onCycles] \n\t"                                // Execute NOPs to delay exactly the specified number of cycles
      "nop \n\t"
      ".endr \n\t"
      "cbi %[port], %[bit] \n\t"                              // Clear the output bit
      ".rept %[offCycles] \n\t"                               // Execute NOPs to delay exactly the specified number of cycles
      "nop \n\t"
      ".endr \n\t"
      ::
      [port]    "I" (_SFR_IO_ADDR(PIXEL_PORT)),
      [bit]   "I" (PIXEL_BIT),
      [onCycles]  "I" (NS_TO_CYCLES(T1H) - 2),    // 1-bit width less overhead  for the actual bit setting, note that this delay could be longer and everything would still work
      [offCycles]   "I" (NS_TO_CYCLES(T1L) - 2)     // Minimum interbit delay. Note that we probably don't need this at all since the loop overhead will be enough, but here for correctness

    );
                                  
    } else {          // 1 bit

    // **************************************************************************
    // This line is really the only tight goldilocks timing in the whole program!
    // **************************************************************************


    asm volatile (
      "sbi %[port], %[bit] \n\t"        // Set the output bit
      ".rept %[onCycles] \n\t"        // Now timing actually matters. The 0-bit must be long enough to be detected but not too long or it will be a 1-bit
      "nop \n\t"                                              // Execute NOPs to delay exactly the specified number of cycles
      ".endr \n\t"
      "cbi %[port], %[bit] \n\t"                              // Clear the output bit
      ".rept %[offCycles] \n\t"                               // Execute NOPs to delay exactly the specified number of cycles
      "nop \n\t"
      ".endr \n\t"
      ::
      [port]    "I" (_SFR_IO_ADDR(PIXEL_PORT)),
      [bit]   "I" (PIXEL_BIT),
      [onCycles]  "I" (NS_TO_CYCLES(T0H) - 2),
      [offCycles] "I" (NS_TO_CYCLES(T0L) - 2)

    );
      
    }
    
    // Note that the inter-bit gap can be as long as you want as long as it doesn't exceed the 5us reset timeout (which is A long time)
    // Here I have been generous and not tried to squeeze the gap tight but instead erred on the side of lots of extra time.
    // This has thenice side effect of avoid glitches on very long strings becuase 

    
}  

  
inline void sendByte( unsigned char byte ) {
    
    for( unsigned char bit = 0 ; bit < 8 ; bit++ ) {
      
      sendBit( bitRead( byte , 7 ) );                // Neopixel wants bit in highest-to-lowest order
                                                     // so send highest bit (bit #7 in an 8-bit byte since they start at 0)
      byte <<= 1;                                    // and then shift left so bit 6 moves into 7, 5 moves into 6, etc
      
    }           
} 

/*

  The following three functions are the public API:
  
  ledSetup() - set up the pin that is connected to the string. Call once at the begining of the program.  
  sendPixel( r g , b ) - send a single pixel to the string. Call this once for each pixel in a frame.
  show() - show the recently sent pixel on the LEDs . Call once per frame. 
  
*/


// Set the specified pin up as digital out

void ledsetup() {
  
  bitSet( PIXEL_DDR , PIXEL_BIT );
  
}

inline void sendPixel( unsigned char r, unsigned char g , unsigned char b )  {  
  
  sendByte(r/10);
  sendByte(g/10);
  sendByte(b/10);          // Neopixel wants colors in green then red then blue order
  
}


// Just wait long enough without sending any bots to cause the pixels to latch and display the last sent frame

void show() {
  _delay_us( (RES / 1000UL) + 1);       // Round up since the delay must be _at_least_ this long (too short might not work, too long not a problem)
}


/*

  That is the whole API. What follows are some demo functions rewriten from the AdaFruit strandtest code...
  
  https://github.com/adafruit/Adafruit_NeoPixel/blob/master/examples/strandtest/strandtest.ino
  
  Note that we always turn off interrupts while we are sending pixels becuase an interupt
  could happen just when we were in the middle of somehting time sensitive.
  
  If we wanted to minimize the time interrupts were off, we could instead 
  could get away with only turning off interrupts just for the very brief moment 
  when we are actually sending a 0 bit (~1us), as long as we were sure that the total time 
  taken by any interrupts + the time in our pixel generation code never exceeded the reset time (5us).
  
*/


// Display a single color on the whole string

void showColor( unsigned char r , unsigned char g , unsigned char b ) {
  
  cli();  
  for( int p=0; p<PIXELS; p++ ) {
    sendPixel( r , g , b );
  }
  sei();
  show();
  
}

void saberOn()
{
  DEBUG_PRINT("Saber on");
  colorWipe(5);
}

void saberOff()
{
  DEBUG_PRINT("Saber off");
  colorRetract(5);
}

void rainbow()
{
  DEBUG_PRINT("Rainbow on");
  unsigned char r = 250;
  unsigned char g = 0;
  unsigned char b = 0;

  while (!newModeAvailable)
  {
    cli();

    sendPixel(r, g, b);
    if (g == 250 && r > 0)
    {
      r -= 10;
    }
    if (r == 250 && g < 250 && b == 0)
    {
      g += 10;
    }
    if (r == 0 && g == 250 && b < 250)
    {
      b += 10;
    }
    if (b == 250 & g > 0)
    {
      g -= 10;
    }
    if (b == 250 && r < 250 && g == 0)
    {
      r += 10;
    }
    if (r == 250 && b > 0)
    {
      b -= 10;
    }

    sei();
    show();
    delay(PIXELS / 1.4);
  }
}

// Fill the dots one after the other with a color
// rewrite to lift the compare out of the loop
void colorWipe(unsigned char wait)
{
  float multiplier = getFlickerMultiplier();
  for (signed int i = 0; i < PIXELS; i++)
  {
    unsigned int p = 0;
    if (i%10 == 0) {
      multiplier = getFlickerMultiplier();
    }
    uint8_t r = min(255, color[0]*multiplier);
    uint8_t g = min(255, color[1]*multiplier);
    uint8_t b = min(255, color[2]*multiplier);
    cli();

    while (p < i)
    {
      sendPixel(r,g,b);
      p++;
    }

    while (p < PIXELS)
    {
      sendPixel(0, 0, 0);
      p++;
    }

    sei();
    show();
    delay(wait);

    if (newModeAvailable)
    {
      DEBUG_PRINT("break out of colorWipe");
      return;
    }
  }
}

// Fill the dots one after the other with a color (reversed)
void colorRetract(unsigned char wait)
{
  float multiplier = getFlickerMultiplier();
  for (signed int i = PIXELS - 1; i >= 0; i--)
  {
    unsigned int p = 0;
    if (i%5 == 0) {
      multiplier = getFlickerMultiplier();
    }
    uint8_t r = min(255, color[0]*multiplier);
    uint8_t g = min(255, color[1]*multiplier);
    uint8_t b = min(255, color[2]*multiplier);
    cli();

    while (p < i)
    {
      sendPixel(r,g,b);
      p++;
    }

    while (p < PIXELS)
    {
      sendPixel(0, 0, 0);
      p++;
    }

    sei();
    show();
    delay(wait);

    if (newModeAvailable)
    {
      return;
    }
  }
}

float getFlickerMultiplier()
{
  bool saberFlickerDisabled = BIT_CHECK(mode, 6);
  if (saberFlickerDisabled)
    return 1.0f;
  else
    return random(9, 16) * 0.1f;
}

void saberLoop()
{
  DEBUG_PRINT("Saber loop");

  while (!newModeAvailable)
  {
    unsigned int p = 0;
    float multiplier = getFlickerMultiplier();
    uint8_t r = min(255, color[0]*multiplier);
    uint8_t g = min(255, color[1]*multiplier);
    uint8_t b = min(255, color[2]*multiplier);
    cli();

    while (p < PIXELS)
    {
      sendPixel(r,g,b);
      p++;
    }

    sei();
    show();
    delay(50);
  }
  DEBUG_PRINT("break out of saberLoop");
}

void setup()
{
  if (DEBUG)
  {
    Serial.begin(9600);
    Serial.println(F("setup start"));
  }
  pinMode(led, OUTPUT);
  Wire.begin(4);                // join i2c bus with address #4
  Wire.onReceive(receiveEvent); // register event
  ledsetup();
  DEBUG_PRINT("setup complete");
}

void loop()
{
  if (newModeAvailable)
  {
    newModeAvailable = false;
	  printBinary(newMode);

    bool onOff = BIT_CHECK(mode, 7);
    bool newOnOff = BIT_CHECK(newMode, 7);
    int t = BITMASK_CHECK_ANY(newMode, 0x0F);
    DEBUG_PRINT(String(t));
    color[0] = colors[t][0];
    color[1] = colors[t][1];
    color[2] = colors[t][2];

    mode = newMode;
    if (newOnOff != onOff) // if on/off state has changed
    {
      if (newOnOff)
        saberOn();
      else 
        saberOff();
    }

    if (newOnOff) // if saber is on
      saberLoop();
  }
  else
  {
    DEBUG_PRINT("delay");
    delay(100);
  }
}

void receiveEvent(int howMany)
{
  while (Wire.available())
  {
    newMode = Wire.read();
    if (newMode != mode)
    {
      newModeAvailable = true;
    }
  }
}

void printBinary(byte inByte)
{
	for (int b = 7; b >= 0; b--)
	{
		Serial.print(bitRead(inByte, b));
	}
	Serial.println();
}
