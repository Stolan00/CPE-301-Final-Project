#include <LiquidCrystal.h>
#include <RTClib.h>
#include <DHT.h>
#include <Stepper.h>

#define DHTPIN 2
#define DHTTYPE DHT11

#define RDA 0x80
#define TBE 0x20  
// Define constants and pins
const int ButtonOnOff = 2;
const int ButtonReset = 3;
const int ButtonStepperUp = 4;
const int ButtonStepperDown = 5;
const int MotorPin = 6;
const int DhtPin = 7;
const int WaterLevelPin = A0;
const int DHT_Type = DHT11;

const int waterLevelMin = 100;
const int tempMin = 10;
//----------------------------------
const int redLED = 47;
const int blueLED = 48;
const int greenLED = 49;
const int yellowLED = 50;

const int toggleDisabled = 0;
const int toggleReset = 1;
//----------------------------------

volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

// Define a struct to hold sensor readings and time
struct SensorReadings {
    int waterLevel;
    float temperature;
    float humidity;
    DateTime currentTime;
};

// State information
struct SystemState {
    bool fanOn = false;
    bool displayReadings = false;
    bool stepperAllowed = false;
    bool monitorWaterLevel = false;
    int ledColorCode = -1; // Using -1 as a default 'off' state
} systemState;

//--FUNCTION PROTOTYPES-----------------------------
void U0init(unsigned long U0baud);
unsigned int adc_read(unsigned char adc_channel);
void activateLED(int ledPin);
void displayTempAndHumidity(SensorReadings curReadings);
int readPin(int pin);
void logStateTransition(const char* state);

//--INITIALIZATION----------------------------------
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
RTC_DS1307 rtc;
DHT dht(DhtPin, DHT_Type);
Stepper stepper(2048, 13, 14, 15, 16);

void setup() {
  U0init(9600);

  rtc.begin();
  dht.begin();
  lcd.begin(16, 2);
  // Set ButtonOnOff and ButtonReset as INPUT
  DDRE &= ~(1 << DDE4);  // Clear the bit for pin 2 in DDRE
  DDRE &= ~(1 << DDE5);  // Clear the bit for pin 3 in DDRE

  PORTE |= (1 << PE4);   // Set the bit for pin 2 in PORTE
  PORTE |= (1 << PE5);   // Set the bit for pin 3 in PORTE

  // Set MotorPin as OUTPUT
  DDRH |= (1 << DDH3);   // Set the bit for pin 6 in DDRH


  // Set initial time on RTC
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  void handleIdleState();

  // attach an interrupt to the button pin
  attachInterrupt(digitalPinToInterrupt(ButtonOnOff), handleOnOff, RISING);
  stepper.setSpeed(10);
}

void loop() {
  // State machine logic
    // Get the current sensor readings
    SensorReadings currentReadings = getSensorReadings();
    displayTempAndHumidity(currentReadings);

    // Now use currentReadings to check conditions and decide state
    if (currentReadings.waterLevel <= waterLevelMin) {
        handleErrorState();
    }

    else if (currentReadings.temperature > tempMin) {
        handleRunningState();
    }

    else if (currentReadings.temperature <= tempMin || readPin(toggleReset)) {
        handleIdleState();
    }

    else if (readPin(toggleDisabled)) {
      handleDisabledState();
    }

    int checkStep;
    if (readPin(ButtonStepperUp)) {
      checkStep = 1;
    }
    else if (readPin(ButtonStepperDown)) {
      checkStep = -1;
    }
    else checkStep = 0;

    int newStep = 2000 * checkStep; 

    setStep(newStep);

    delay(1000);
}


SensorReadings getSensorReadings() {
    SensorReadings readings;
    if (systemState.monitorWaterLevel == true) {
        readings.waterLevel = adc_read(WaterLevelPin);
    }
    else readings.waterLevel = 9999;

    readings.temperature = dht.readTemperature();
    readings.humidity = dht.readHumidity();
    readings.currentTime = rtc.now();
    return readings;
}


void handleErrorState() {
  lcd.clear();
  lcd.print("Water level too low");
  systemState.fanOn = false;
  systemState.displayReadings = true;
  systemState.monitorWaterLevel = true;
  activateLED(redLED);
  logStateTransition("Error");
}
void handleIdleState() {
  // Display idle message on LCD
  systemState.fanOn = true;
  activateLED(greenLED);
  lcd.print("System Idle");
  systemState.monitorWaterLevel = true;
  logStateTransition("Idle");
}
void handleRunningState() {
  // Display idle message on LCD
  systemState.fanOn = true;
  activateLED(blueLED);
  lcd.print("System Idle");
  systemState.monitorWaterLevel = true;
  logStateTransition("Running");
}
void handleDisabledState() {
  // Display idle message on LCD
  systemState.fanOn = false;
  systemState.monitorWaterLevel = false;
  systemState.displayReadings = false;
  activateLED(yellowLED);
  lcd.print("System Disabled");
  logStateTransition("Disabled");
}

void activateLED(int whichLED) {
  // turn off all of the LEDs using a bitwise AND operation
  PORTE &= ~(0x01 << greenLED);
  PORTE &= ~(0x01 << yellowLED);
  PORTH &= ~(0x01 << redLED);
  PORTG &= ~(0x01 << blueLED);
 

  // turn on the specified LED using a bitwise OR operation
  switch (whichLED) {
    case 0:
      PORTH |= 0x01 << redLED;
      break;
    case 1:
      PORTG |= 0x01 << blueLED;
      break;
    case 2:
      PORTE |= 0x01 << greenLED;
      break;
    case 3:
      PORTE |= 0x01 << yellowLED;
      break;
  }
}

void handleOnOff(){

  bool p = readPin(ButtonOnOff);
  if(p){
    handleIdleState();
  }
}

//stepper function
void setStep(int newS){
  stepper.step(newS);
}


void displayTempAndHumidity(SensorReadings readings) {
    // Clear the LCD and set the cursor to the beginning
    lcd.clear();
    lcd.setCursor(0, 0);

    // Display temperature and humidity
    lcd.print("Temp: ");
    lcd.print(readings.temperature);
    lcd.print("C ");
    lcd.setCursor(0, 1); // Move to the second line of the LCD
    lcd.print("Hum: ");
    lcd.print(readings.humidity);
    lcd.print("%");
}


int readPin(int pin) {
  return PINA & (0x01 << pin);
}

void adc_init(){
     ADCSRA = 0x80;
     ADCSRB = 0x00;
     ADMUX = 0x40;
}

unsigned int adc_read(unsigned char adc_channel){
     ADCSRB &= 0xF7; // Reset MUX5.
     ADCSRB |= (adc_channel & 0x08); // Set MUX5.
     ADMUX &= 0xF8; // Reset MUX2:0.
     ADMUX |= (adc_channel & 0x07); // Set MUX2:0.

     ADCSRA |= 0x40; // Start the conversion.
     while (ADCSRA & 0x40) {} // Converting...
     return ADC; // Return the converted number.
}

void U0init(unsigned long U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}

unsigned char U0kbhit()
{
  return (RDA & *myUCSR0A);
}

unsigned char U0getchar()
{
  return *myUDR0;
}

void U0putchar(unsigned char U0pdata)
{
  while(!(TBE & *myUCSR0A));
  *myUDR0 = U0pdata;
}
// Function to reverse a string
void reverse(char str[], int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

// Function to convert integer to string
int intToStr(int num, char str[], int d) {
    int i = 0;
    do {
        str[i++] = (num % 10) + '0';
        num = num / 10;
    } while (num);

    // If number of digits required is more, then add 0s at the beginning
    while (i < d)
        str[i++] = '0';

    reverse(str, i);
    str[i] = '\0';
    return i;
}

void logStateTransition(const char* state) {
    DateTime now = rtc.now();
    char buffer[4];

    // Send "Time: "
    const char* timePrefix = "Time: ";
    for (int i = 0; timePrefix[i] != '\0'; i++) {
        U0putchar(timePrefix[i]);
    }

    // Send hour
    intToStr(now.hour(), buffer, 2);
    U0putchar(buffer[0]);
    U0putchar(buffer[1]);
    U0putchar(':');

    // Send minute
    intToStr(now.minute(), buffer, 2);
    U0putchar(buffer[0]);
    U0putchar(buffer[1]);
    U0putchar(':');

    // Send second
    intToStr(now.second(), buffer, 2);
    U0putchar(buffer[0]);
    U0putchar(buffer[1]);
    U0putchar(' ');

    // Send "State Transition to: "
    const char* transitionMsg = "State Transition to: ";
    for (int i = 0; transitionMsg[i] != '\0'; i++) {
        U0putchar(transitionMsg[i]);
    }

    // Send state
    for (int i = 0; state[i] != '\0'; i++) {
        U0putchar(state[i]);
    }

    U0putchar('\r');
    U0putchar('\n');
}

void logStepperPosition(int steps) {
    DateTime now = rtc.now();
    char buffer[4];

    // Send "Time: "
    const char* timePrefix = "Time: ";
    for (int i = 0; timePrefix[i] != '\0'; i++) {
        U0putchar(timePrefix[i]);
    }

    // Send hour
    intToStr(now.hour(), buffer, 2);
    U0putchar(buffer[0]);
    U0putchar(buffer[1]);
    U0putchar(':');

    // Send minute
    intToStr(now.minute(), buffer, 2);
    U0putchar(buffer[0]);
    U0putchar(buffer[1]);
    U0putchar(':');

    // Send second
    intToStr(now.second(), buffer, 2);
    U0putchar(buffer[0]);
    U0putchar(buffer[1]);
    U0putchar(' ');

    // Send "Stepper Position Changed: "
    const char* stepperMsg = "Stepper Position Changed: ";
    for (int i = 0; stepperMsg[i] != '\0'; i++) {
        U0putchar(stepperMsg[i]);
    }

    // Send stepper steps
    intToStr(steps, buffer, 0);  // Assuming steps is not prefixed with zeros
    for (int i = 0; buffer[i] != '\0'; i++) {
        U0putchar(buffer[i]);
    }

    U0putchar('\r');
    U0putchar('\n');
}
