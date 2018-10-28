#include <SerialRFID.h>
#include <SoftwareSerial.h>
#include <Adafruit_NeoPixel.h>

/**
   Misc.
*/

const byte NUM_STAGES = 3;

/**
  Structs.
*/

typedef struct programState {
  bool isStageLit[NUM_STAGES];
  bool isRelayOpen[NUM_STAGES];
  short currentActiveStage;
} ProgramState;

ProgramState progState = {
  .isStageLit = {false, false, false},
  .isRelayOpen = {false, false, false},
  .currentActiveStage = -1
};

/**
   Pins.
*/

const byte PIN_RFID_RX = 2;
const byte PIN_RFID_TX = 12;

const byte PIN_LEDS = 3;

const byte PIN_AUDIO_T0 = 4;
const byte PIN_AUDIO_T1 = 5;
const byte PIN_AUDIO_RST = 6;
const byte PIN_AUDIO_ACT = 7;

const byte RELAY_PINS[NUM_STAGES] = {
  8, 9, 10
};

/**
   LED strips.
*/

const byte DEFAULT_BRIGHTNESS = 120;

const uint32_t LED_COLORS[NUM_STAGES] = {
  Adafruit_NeoPixel::Color(255, 0, 0),
  Adafruit_NeoPixel::Color(0, 255, 0),
  Adafruit_NeoPixel::Color(0, 0, 255)
};

// LED_MAIN_PATCH_SIZE + sum(LED_STAGE_PATCH_SIZES) == NUM_LEDS

const byte NUM_LEDS = 11;

const byte LED_MAIN_PATCH_SIZE = 2;

const byte LED_STAGE_PATCH_SIZES[NUM_STAGES] = {
  3, 3, 3
};

Adafruit_NeoPixel ledStrip = Adafruit_NeoPixel(NUM_LEDS, PIN_LEDS, NEO_GRB + NEO_KHZ800);

/**
   RFID reader.
*/

SoftwareSerial rfidSerial(PIN_RFID_RX, PIN_RFID_TX);
SerialRFID rfid(rfidSerial);

char validStageTags[NUM_STAGES][SIZE_TAG_ID] = {
  "1D00277FBDF8",
  "1D00278D53E4",
  "1D0027B80A88"
};

const char RESET_TAG[SIZE_TAG_ID] = "112233445566";

char tagId[SIZE_TAG_ID];
unsigned long tagIdMillis;

/**
   LED functions.
*/

void initLedStrip() {
  ledStrip.begin();
  ledStrip.setBrightness(DEFAULT_BRIGHTNESS);
  ledStrip.clear();
  ledStrip.show();
}

void showMainLeds() {
  uint32_t color = (progState.currentActiveStage < 0) || (progState.currentActiveStage >= NUM_STAGES) ?
                   Adafruit_NeoPixel::Color(0, 0, 0) :
                   LED_COLORS[progState.currentActiveStage];

  for (int i = 0; i < LED_MAIN_PATCH_SIZE; i++) {
    ledStrip.setPixelColor(i, color);
  }

  ledStrip.show();
}

void showStageLeds(byte idxStage) {
  int iniIdx = LED_MAIN_PATCH_SIZE;

  for (int i = 0; i < idxStage; i++) {
    iniIdx += LED_STAGE_PATCH_SIZES[i];
  }

  int endIdx = iniIdx + LED_STAGE_PATCH_SIZES[idxStage];

  uint32_t color = (progState.isStageLit[idxStage]) ?
                   LED_COLORS[idxStage] :
                   Adafruit_NeoPixel::Color(0, 0, 0);

  for (int i = iniIdx; i < endIdx; i++) {
    ledStrip.setPixelColor(i, color);
  }

  ledStrip.show();
}

void showLeds() {
  showMainLeds();

  for (int i = 0; i < NUM_STAGES; i++) {
    showStageLeds(i);
  }
}

/**
   Audio FX board functions
*/

void playTrack(byte trackPin) {
  digitalWrite(trackPin, LOW);
  delay(500);
  digitalWrite(trackPin, HIGH);
}

void initAudioPins() {
  pinMode(PIN_AUDIO_T0, OUTPUT);
  digitalWrite(PIN_AUDIO_T0, HIGH);

  pinMode(PIN_AUDIO_T1, OUTPUT);
  digitalWrite(PIN_AUDIO_T1, HIGH);

  pinMode(PIN_AUDIO_ACT, INPUT);
  pinMode(PIN_AUDIO_RST, INPUT);
}

bool isTrackPlaying() {
  return digitalRead(PIN_AUDIO_ACT) == LOW;
}

void resetAudio() {
  digitalWrite(PIN_AUDIO_RST, LOW);
  pinMode(PIN_AUDIO_RST, OUTPUT);
  delay(10);
  pinMode(PIN_AUDIO_RST, INPUT);
  delay(1000);
}

/**
   Relay functions.
*/

void lockRelay(byte idx) {
  digitalWrite(RELAY_PINS[idx], LOW);
  progState.isRelayOpen[idx] = false;
}

void openRelay(byte idx) {
  digitalWrite(RELAY_PINS[idx], HIGH);
  progState.isRelayOpen[idx] = true;
}

void initRelay(byte idx) {
  pinMode(RELAY_PINS[idx], OUTPUT);
  lockRelay(idx);
}

void initRelays() {
  for (int i = 0; i < NUM_STAGES; i++) {
    initRelay(i);
  }
}

/**
   Entrypoint.
*/

void setup() {
  Serial.begin(9600);

  initAudioPins();
  initRelays();
  initLedStrip();
  resetAudio();

  Serial.println(">> Starting crystal lamp program");
  Serial.flush();
}

void loop() {
  // put your main code here, to run repeatedly:
}
