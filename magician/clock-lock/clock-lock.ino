#include <Automaton.h>
#include <Adafruit_NeoPixel.h>

typedef struct programState {
  unsigned long encoderCounter;
  unsigned long openMillis;
  bool isOpen;
} ProgramState;

ProgramState programState = {
  .encoderCounter = 0,
  .openMillis = 0,
  .isOpen = false
};

/**
   Rotary encoder pins:
   A and B: white and green wires
   Red wire: 5V
   Black wire: GND
*/

const int ENC_PIN_A = A5;
const int ENC_PIN_B = A4;

const byte RELAY_PIN = 7;

const int ENCODER_TIMER_MS = 1000;
const int ENCODER_COUNTER_THRESHOLD = 150;

const int ENC_RANGE_LO = 0;
const int ENC_RANGE_HI = 100;

Atm_controller encoderController;
Atm_encoder rotEncoder;
Atm_timer encoderLevelTimer;

unsigned long rotCounter = 1;

void onMaxEncoderLevel(int idx, int v, int up) {
  if (programState.isOpen) {
    return;
  }

  Serial.println("Max encoder level reached");

  programState.isOpen = true;
  programState.openMillis = millis();

  openRelay();
}

void onRotEncoderChange(int idx, int v, int up) {
  if (programState.isOpen) {
    return;
  }

  Serial.print("onRotEncoderChange :: idx=");
  Serial.print(idx);
  Serial.print(" v=");
  Serial.print(v);
  Serial.println();
  Serial.flush();

  programState.encoderCounter++;
}

bool isEncoderLevelOverThreshold() {
  return programState.encoderCounter >= ENCODER_COUNTER_THRESHOLD;
}

bool isEncoderLevelOverThreshold(int idx) {
  return isEncoderLevelOverThreshold();
}

void onEncoderTimer(int idx, int v, int up) {
}

void initMachines() {
  rotEncoder
  .begin(ENC_PIN_A, ENC_PIN_B)
  .range(ENC_RANGE_LO, ENC_RANGE_HI, true)
  .onChange(onRotEncoderChange);

  encoderLevelTimer
  .begin(ENCODER_TIMER_MS)
  .repeat(-1)
  .onTimer(onEncoderTimer)
  .start();

  encoderController
  .begin()
  .IF(isEncoderLevelOverThreshold)
  .onChange(true, onMaxEncoderLevel);
}

void lockRelay() {
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_BUILTIN, LOW);
}

void openRelay() {
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);
}

void initRelay() {
  pinMode(RELAY_PIN, OUTPUT);
  lockRelay();
}

void setup() {
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);
  initMachines();
  initRelay();

  Serial.println(">> Starting encoder lock program");
}

void loop() {
  automaton.run();
}
