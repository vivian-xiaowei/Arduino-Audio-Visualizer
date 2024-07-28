/**
 * Audio volume visualizer
 * The shield takes audio analog input and display the current volume on to the 2 led matrixes. The height of the led 
 * lit up represents the loudness of volume and the columns represent the time passed. The right most column represents
 * the volume of the most current volume from the audio signal.
 * Author: Vivian Ji
 * Version: Jun 2024
 */
 

// button pin
#define BUTTON 39
// right audio channel input
#define AUDIO_R A6
// left audio channel input
#define AUDIO_T A7

#define LED_OFF 255
#define LED_ON 0

#define SIZE 8

// rows pins for matrix M2 (closer to the connector)
const byte rowRight[8] = {47, 44, 45, 46, 2, 3, 4, 5}; 
// rows pins for matrix M1
const byte rowLeft[8] = {9, 8, 7, 6, 13, 12, 11, 10};

/**
 * since the rows are output (anode), the port is the cathode
 * when the port writes high on a pin, that pin does not light up
 * example: PORTA = B00000001 (1), the eighth pin does not light up
 * example: PORTA = B11111110 (254), only the last pin lights up
 * so in the array, the number is the sum of the pins that does not light up in binary
 *
 * first 8 is for matrix farther from connecter, last 8 is for matrix closer to connecter
 * each number is the on/off state of the led of 1 row
 */

unsigned char red[16]={255, 255, 255, 255, 255, 255, 255, 255,255, 255, 255, 255, 255, 255, 255, 255};
// ex: red[0] = B11111110 (254), left most red led is on since the pins are reversed:
// before reverse: B01111111 = 255 - 128, left most led is on
// reverse: B01111111 -> B11111110
unsigned char green[16]={255, 255, 255, 255, 255, 255, 255, 255,255, 255, 255, 255, 255, 255, 255, 255};
// green[0] = B01111111, left most green led is on, 255-128 = 127 = 64+32+16+8+4+2+1
unsigned char blue[16]={255, 255, 255, 255, 255, 255, 255, 255,255, 255, 255, 255, 255, 255, 255, 255};
// blue[0] = B01111111, left most blue led is on, 255-128 = 127 = 64+32+16+8+4+2+1

// the state of led display, 0 is starting from the bottom, 1 is starting in the middle and going both up and down
char state = 0;
// previous state of button
bool buttonPre = 1;
// when the state is just changed
bool stateChange = 0;

// the constant to scale the volume into the number of led lighting up for different states
int ledScale[2] = {128, 256};

// the average volume for 1 cycle of volume reading
int volume = 0;
// variable that store the temporary sum of the reading
long int temp = 0;
// count how many readings are done to calculate average
int count = 0;
// count how many readings are zero because of negative values to get more accurate average
int zero = 0;
// control the pattern be updated every 2 times the led refreshed
bool update = false;


void setup(){

  // set up ports for red, green, blue - outputs
  DDRK = B11111111; // red
  DDRA = B11111111; // green
  DDRC = B11111111; // blue
  // set all leds to be off
  PORTK = B11111111;
  PORTA = B11111111;
  PORTC = B11111111;

  // setup both matrix rows as outputs
  for(byte i = 0; i < 8; i++) {
    pinMode(rowRight[i], OUTPUT);
    pinMode(rowLeft[i], OUTPUT);
  }

  // setup audio inputs
  pinMode(AUDIO_R, INPUT);
  pinMode(AUDIO_T, INPUT);
  analogReference(INTERNAL1V1);

  // setup button
  pinMode(BUTTON, INPUT_PULLUP);

  // set up arduino internal interrupt for led matrix refresh
  setupInterrupt();
}

// -------------------------------------
// Arduino clock is 16 MHz. A prescaler is used to slow down the counting.
// Timer0 is a 8-bit counter (max value is 255).
// When Timer/Counter Register(TCNT) value reaches Output Compare Register (OCR) value, an interrupt is generated.
// The OCR value is calculated according to the formula:
// OCR =  [ (16MHz / prescaler_value) * desired_time_in_seconds ] - 1
void setupInterrupt() {
  cli();                                // disable all interrupts
  
  TCCR1A = 0;                           // clear the two Timer/Counter Control Registers (TCR) for Timer1
  TCCR1B = 0;                           // 
  TCCR1B |= (1 << WGM12);               // turn on Clear Timer on Compare (CTC) mode
  TCCR1B |= (1 << CS12) | (1 << CS10);  // set CS12 and CS10 bits for prescaler_value = 1024
  
  TCNT1  = 0;                           // initialize the Timer/Counter value to 0
  OCR1A = 332;                          // set OCR = [ 16,000,000/1024 * 0.020 ] - 1 
                                        // thus, the interrupt will occur every 20 ms
                                        
  TIMSK1 |= (1 << OCIE1A);              // enable interrupts from Timer1 when TCNT1 reaches OCR1A value

  sei();                                // enable interrupts
}

// -------------------------------------
ISR(TIMER1_COMPA_vect){                 // Interrup Service Routine for Timer1 OCRA event

  int button = digitalRead(BUTTON);
  if (button == 0 && buttonPre != button) {
    state = 1-state;
    stateChange = 1;
    buttonPre = 0;
  } else if (button == 1) {
    buttonPre = 1;
  }
  // Serial.println(count);
  // update the volume to be represented by led
  if (update) {
    calculateVol();

    // calculate how many leds are on for current volume
    int numLedOn = (volume/ledScale[state]);
    // due to volume is average and analog reading has a maximum of 1023, give some offset for top row to light up
    if (volume > 1024-ledScale[state]/2) {
      numLedOn = 1024/ledScale[state];
    }
    // use the current volume to set the values to be outputted from ports
    convert(numLedOn);
    update = false;
  } else {
    update = true;
  }
  ledDisplay();
}

void calculateVol() {
  // calculate average volume
  volume = temp/(count-zero); 
  
  // handle exception when temp is 0, otherwise volume becomes -1
  if (volume < 0) {
    volume = 0;
  }
  // reset the analog reading of audio
  temp = 0;
  count = 0;
  zero = 0;
}

// -------------------------------------
void loop(){                            // main program
  int read = analogRead(AUDIO_T); // read audio signal
  temp += read; // add to temporary audio sum
  count++;      // increase count of reading
  if (read == 0) {
    zero++;     // count the number of negative voltage which reads as 0
  }
}

// converts the volume into values of port
void convert(char num) {
  if (stateChange) {
    clearMatrix();    // clear the values (turn off led) when state is changed
    stateChange = 0;
  }
  else if (state == 0) {
    bottomConvert(num);
  } else {
    middleConvert(num);
  }
}

void clearMatrix() {
  // reset all values in red, green and blue arrays
  for (int i = 0; i < 16; i++) {
    red[i] = LED_OFF;
    blue[i] = LED_OFF;
    green[i] = LED_OFF;
  }
}

void bottomConvert(char num) {
  for (int i = 0; i < 16; i++) {
    
    if (i < SIZE) {
      red[i] = red[i]/2;
      if (red[i+SIZE] %2 != 0) {
        red[i] += 128;
      }

      blue[i] = blue[i] * 2;
      if (blue[i+SIZE] >= 128) {
        blue[i] += 1;
      }

      green[i] = green[i] * 2;
      if (green[i+SIZE] >= 128) {
        green[i] += 1;
      }

    } else {
      int index = i % SIZE;

      red[i] = red[i]/2;
      blue[i] = blue[i]*2;
      green[i] = green[i]*2;

      if (index < SIZE-num) {
        red[i] += 128;
        blue[i] += 1;
        green[i] += 1;
      }
    }
    
  }
}

void middleConvert(char num) {
  for (int i = 0; i < 16; i++) {
    red[i] = red[i]/2;
    blue[i] = blue[i] * 2;
    green[i] = green[i] * 2;

    if (i < 8) {
      if (red[i+8] %2 != 0) {
        red[i] += 128;
      }
      if (blue[i+8] >= 128) {
        blue[i] += 1;
      }
      if (green[i+8] >= 128) {
        green[i] += 1;
      }
    } else {
      int index = i % 8;

      if (index < 4 - num || index >= 4+num) {
        red[i] += 128;
        blue[i] += 1;
        green[i] += 1;
      }
    }
  }
}

void ledDisplay() {
  if (state == 0) {
    bottomUpBlend();
  } else {
    middleBlend();
  }
}

void bottomUpBlend() {
  for (int i = 4; i < 8; i++) {
    PORTK = red[i];
    digitalWrite(rowLeft[i], 1);
    delay(1);
    digitalWrite(rowLeft[i], 0);
    PORTK = red[i+8];
    digitalWrite(rowRight[i], 1);
    delay(1);
    digitalWrite(rowRight[i], 0);
  }
  PORTK = LED_OFF;
  for (int i = 0; i < 6; i++) {
    PORTA = LED_OFF;
    PORTC = blue[i];
    if (i < 2) {
      PORTA = green[i];
    }
    digitalWrite(rowLeft[i], 1);
    delay(2);
    digitalWrite(rowLeft[i], 0);
    PORTC = blue[i+8];
    if (i < 2) {
      PORTA = green[i+8];
    }
    digitalWrite(rowRight[i], 1);
    delay(2);
    digitalWrite(rowRight[i], 0);
  }
  PORTC = LED_OFF;
  PORTA = LED_OFF;
}

void middleBlend() {
  for (int i = 0; i < 3; i+=2) {
    PORTK = red[i];
    digitalWrite(rowLeft[i], 1);
    digitalWrite(rowLeft[7-i], 1);
    delay(1);
    digitalWrite(rowLeft[i], 0);
    digitalWrite(rowLeft[7-i], 0);

    PORTK = red[i+8];
    digitalWrite(rowRight[i], 1);
    digitalWrite(rowRight[7-i], 1);
    delay(1);
    digitalWrite(rowRight[i], 0);
    digitalWrite(rowRight[7-i], 0);
  }

  PORTK = LED_OFF;
  for (int i = 0; i < 8; i++) {
    PORTA = LED_OFF;
    if (i < 2 || i > 5) {
      PORTA = green[i];
    }
    PORTC = blue[i];
    digitalWrite(rowLeft[i], 1);
    delay(1);
    digitalWrite(rowLeft[i], 0);
    if (i < 2 || i > 5) {
      PORTA = green[i+8];
    }
    PORTC = blue[i+8];
    digitalWrite(rowRight[i], 1);
    delay(1);
    digitalWrite(rowRight[i], 0);
  }
  PORTA = LED_OFF;
  PORTC = LED_OFF;
}

unsigned char reverse(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}
