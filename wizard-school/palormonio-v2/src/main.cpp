#include <Automaton.h>
#include <Adafruit_NeoPixel.h>
#include <CircularBuffer.h>
#include "limits.h"

/**
  Program state.
*/

typedef struct programState
{
  bool isAudioPatternOk;
  bool hasPlayedFinalAudio;
  unsigned long lastKnock;
  bool isRelayOpen;
  bool isLedPatternRunning;
  unsigned long ledPatternStartMillis;
  unsigned long lastLedPatternMillis;
  unsigned int ledKnockClearCountdown;
} ProgramState;

ProgramState progState = {
    .isAudioPatternOk = false,
    .hasPlayedFinalAudio = false,
    .lastKnock = 0,
    .isRelayOpen = false,
    .isLedPatternRunning = false,
    .ledPatternStartMillis = 0,
    .lastLedPatternMillis = 0,
    .ledKnockClearCountdown = 0};

Atm_timer timerState;

const int STATE_TIMER_MS = 60;

/**
   Relay.
*/

const byte RELAY_PIN = 12;

/**
   Piezo knock sensor.
*/

const int KNOCK_PIN = A5;
const int KNOCK_RANGE_MIN = 0;
const int KNOCK_RANGE_MAX = 100;
const int KNOCK_THRESHOLD = 20;
const int KNOCK_SAMPLERATE = 50;
const int KNOCK_BUF_SIZE = 30;
const int KNOCK_TRAINING_SIZE = 5;
const int KNOCK_PATTERN_SIZE = 5;
const float KNOCK_TOLERANCE = 0.6;
const unsigned int KNOCK_DELAY_MS = 150;

unsigned int knockTrainingSet[KNOCK_TRAINING_SIZE][KNOCK_PATTERN_SIZE] = {
    {0, 478, 1279, 1609, 2292},
    {0, 595, 1448, 1775, 2354},
    {0, 596, 1446, 1751, 2409},
    {0, 621, 1549, 1840, 2462},
    {0, 556, 1542, 1846, 2477}};

Atm_analog knockAnalog;
Atm_controller knockController;
CircularBuffer<unsigned long, KNOCK_BUF_SIZE> knockHistory;
float knockPattern[KNOCK_PATTERN_SIZE];
float meanKnockPatternDiff;

/**
 * Proximity sensors.
 */

const int PROX_SENSORS_NUM = 4;
const int PROX_SENSORS_PINS[PROX_SENSORS_NUM] = {A0, A1, A2, A3};
Atm_button proxSensorsBtn[PROX_SENSORS_NUM];

/**
   Solution key.
*/

const int SOLUTION_SIZE = 5;

CircularBuffer<byte, SOLUTION_SIZE> triggerHistory;

const byte SOLUTION_KEY[SOLUTION_SIZE] = {
    0, 1, 3, 2, 1};

/**
   Audio FX.
*/

const byte PIN_AUDIO_RST = 9;
const byte PIN_AUDIO_ACT = 3;
const byte AUDIO_PINS[PROX_SENSORS_NUM] = {8, 6, 5, 4};
const byte PIN_AUDIO_FINAL = 10;

/**
   LED strips.
*/

const int LED_BRIGHTNESS = 150;
const int LED_PIN = 11;
const int LED_NUM = 30;
const int LED_DIVISION_SIZE = 15;
const int LED_PATTERN_CLEAR_MS = 200;
const int LED_PATTERN_DELAY_MS = 5000;
const int LED_KNOCK_SHOW_ITERS = 2;

const uint32_t LED_PATTERN_COLOR = Adafruit_NeoPixel::Color(100, 255, 0);
const uint32_t LED_KNOCK_COLOR = Adafruit_NeoPixel::Color(255, 105, 97);

Adafruit_NeoPixel ledStrip = Adafruit_NeoPixel(LED_NUM, LED_PIN, NEO_GRB + NEO_KHZ800);

/**
   Audio FX functions.
*/

bool isTrackPlaying()
{
  return digitalRead(PIN_AUDIO_ACT) == LOW;
}

void playTrack(byte trackPin)
{
  if (isTrackPlaying())
  {
    Serial.println(F("Skipping: Audio still playing"));
    return;
  }

  Serial.print(F("Playing track on pin: "));
  Serial.println(trackPin);

  digitalWrite(trackPin, LOW);
  pinMode(trackPin, OUTPUT);
  delay(300);
  pinMode(trackPin, INPUT);
}

void initAudioPins()
{
  for (int i = 0; i < PROX_SENSORS_NUM; i++)
  {
    pinMode(AUDIO_PINS[i], INPUT);
  }

  pinMode(PIN_AUDIO_ACT, INPUT);
  pinMode(PIN_AUDIO_RST, INPUT);
}

void resetAudio()
{
  Serial.println(F("Audio FX reset"));

  digitalWrite(PIN_AUDIO_RST, LOW);
  pinMode(PIN_AUDIO_RST, OUTPUT);
  delay(10);
  pinMode(PIN_AUDIO_RST, INPUT);

  Serial.println(F("Waiting for Audio FX startup"));

  delay(2000);
}

/**
   Knock sensor functions.
*/

void setKnockPattern()
{
  for (int i = 0; i < KNOCK_PATTERN_SIZE; i++)
  {
    float val = 0;

    for (int j = 0; j < KNOCK_TRAINING_SIZE; j++)
    {
      val = val + knockTrainingSet[j][i];
    }

    val = val / KNOCK_TRAINING_SIZE;
    knockPattern[i] = val;
  }

  Serial.print(F("Knock pattern: "));

  for (int i = 0; i < KNOCK_PATTERN_SIZE; i++)
  {
    Serial.print(knockPattern[i]);
    Serial.print(F(", "));
  }

  Serial.println(F(""));
}

void setMeanKnockPatternDiff()
{
  float val = 0;

  for (int i = 1; i < KNOCK_PATTERN_SIZE; i++)
  {
    val = val + (knockPattern[i] - knockPattern[i - 1]);
  }

  val = val / (KNOCK_PATTERN_SIZE - 1);

  Serial.print(F("Avg. knock pattern wait (ms): "));
  Serial.println(val);

  meanKnockPatternDiff = val;
}

bool isValidKnockPattern()
{
  if (knockHistory.size() < KNOCK_PATTERN_SIZE)
  {
    return false;
  }

  float tol = meanKnockPatternDiff * KNOCK_TOLERANCE;

  int currIdx;
  unsigned long baseTime;
  unsigned long currTime;
  float timeMin;
  float timeMax;
  bool isValid;

  int maxOffset = knockHistory.size() - KNOCK_PATTERN_SIZE;

  for (int i = 0; i <= maxOffset; i++)
  {
    isValid = true;
    baseTime = knockHistory[i];

    for (int j = 0; j < KNOCK_PATTERN_SIZE; j++)
    {
      currIdx = i + j;
      currTime = knockHistory[currIdx] - baseTime;
      timeMin = knockPattern[j] > tol ? knockPattern[j] - tol : 0;
      timeMax = knockPattern[j] + tol;

      if (currTime < timeMin || currTime > timeMax)
      {
        isValid = false;
        break;
      }
    }

    if (isValid)
    {
      return true;
    }
  }

  return false;
}

void showLedsKnock()
{
  for (int i = LED_DIVISION_SIZE; i < LED_NUM; i++)
  {
    ledStrip.setPixelColor(i, LED_KNOCK_COLOR);
  }

  ledStrip.show();
}

void onKnock(int idx, int v, int up)
{
  if (!progState.isAudioPatternOk)
  {
    return;
  }

  unsigned long now = millis();

  if (now < progState.lastKnock)
  {
    Serial.println(F("Millis overflow: History reset"));
    progState.lastKnock = 0;
    knockHistory.clear();
    return;
  }

  unsigned long diff = now - progState.lastKnock;

  if (progState.lastKnock == 0 || diff >= KNOCK_DELAY_MS)
  {
    Serial.print(now);
    Serial.println(F("::K"));

    progState.lastKnock = now;
    knockHistory.push(now);
    showLedsKnock();
    progState.ledKnockClearCountdown = LED_KNOCK_SHOW_ITERS;
  }
}

void initKnockSensor()
{
  setKnockPattern();
  setMeanKnockPatternDiff();

  knockAnalog
      .begin(KNOCK_PIN, KNOCK_SAMPLERATE)
      .range(KNOCK_RANGE_MIN, KNOCK_RANGE_MAX);

  knockController
      .begin()
      .IF(knockAnalog, '>', KNOCK_THRESHOLD)
      .onChange(true, onKnock);
}

/**
   Microphone functions.
*/

bool isAudioPatternOk()
{
  if (triggerHistory.size() < SOLUTION_SIZE)
  {
    return false;
  }

  for (int i = 0; i < SOLUTION_SIZE; i++)
  {
    if (triggerHistory[i] != SOLUTION_KEY[i])
    {
      return false;
    }
  }

  return true;
}

void onProxSensor(int idx, int v, int up)
{
  Serial.print(F("M:"));
  Serial.print(idx);
  Serial.print(F(":"));
  Serial.println(v);

  if (isTrackPlaying() || progState.isAudioPatternOk)
  {
    return;
  }

  Serial.print(F("Prox. sensor triggered: #"));
  Serial.println(idx);
  triggerHistory.push(idx);
  playTrack(AUDIO_PINS[idx]);
}

void initProxSensors()
{
  for (int i = 0; i < PROX_SENSORS_NUM; i++)
  {
    proxSensorsBtn[i]
        .begin(PROX_SENSORS_PINS[i])
        .onPress(onProxSensor, i);
  }
}

/**
   LED functions.
*/

void clearLeds()
{
  ledStrip.clear();
  ledStrip.show();
}

void initLedStrip()
{
  ledStrip.begin();
  ledStrip.setBrightness(LED_BRIGHTNESS);
  ledStrip.show();
  clearLeds();
}

void clearLedsPattern()
{
  for (int i = 0; i < LED_DIVISION_SIZE; i++)
  {
    ledStrip.setPixelColor(i, 0);
  }

  ledStrip.show();
}

void clearLedsKnock()
{
  for (int i = LED_DIVISION_SIZE; i < LED_NUM; i++)
  {
    ledStrip.setPixelColor(i, 0);
  }

  ledStrip.show();
}

void startLedPattern()
{
  if (progState.isLedPatternRunning)
  {
    Serial.println(F("WARN :: LED sequence ongoing"));
    return;
  }

  Serial.println(F("Starting LED sequence"));

  progState.isLedPatternRunning = true;
  progState.lastLedPatternMillis = millis();
}

void startLedPattern(int idx, int v, int up)
{
  startLedPattern();
}

void resetLedPattern()
{
  clearLedsPattern();
  progState.ledPatternStartMillis = 0;
  progState.isLedPatternRunning = false;
}

void showLedsPattern()
{
  for (int i = 0; i < LED_DIVISION_SIZE; i++)
  {
    ledStrip.setPixelColor(i, LED_PATTERN_COLOR);
  }

  ledStrip.show();
}

void updateLedPattern()
{
  if (!progState.isAudioPatternOk ||
      !progState.isLedPatternRunning)
  {
    clearLedsPattern();
    return;
  }

  unsigned long now = millis();

  if (progState.ledPatternStartMillis == 0)
  {
    Serial.println(F("Updating LED sequence start stamp"));
    progState.ledPatternStartMillis = now;
  }

  if (now < progState.ledPatternStartMillis)
  {
    Serial.println(F("Millis overflow on LED sequence loop"));
    resetLedPattern();
    return;
  }

  unsigned long diff = now - progState.ledPatternStartMillis;

  float limitMillisSoft = knockPattern[KNOCK_PATTERN_SIZE - 1];
  float limitMillisHard = limitMillisSoft + LED_PATTERN_CLEAR_MS;

  if (diff >= limitMillisSoft)
  {
    if (diff < limitMillisHard)
    {
      showLedsPattern();
    }
    else
    {
      resetLedPattern();
    }

    return;
  }

  int currIdx = -1;

  for (int i = 0; i < (KNOCK_PATTERN_SIZE - 1); i++)
  {
    if (diff >= knockPattern[i] &&
        diff < knockPattern[i + 1])
    {
      currIdx = i;
      break;
    }
  }

  if (currIdx == -1)
  {
    Serial.println(F("WARN :: Unable to find LED sequence index"));
    resetLedPattern();
    return;
  }

  float limitMillisShow = knockPattern[currIdx] + LED_PATTERN_CLEAR_MS;

  if (diff < limitMillisShow)
  {
    showLedsPattern();
  }
  else
  {
    clearLedsPattern();
  }
}

void updateLedPattern(int idx, int v, int up)
{
  updateLedPattern();
}

bool isLedPatternStartAllowed()
{
  if (progState.isLedPatternRunning || progState.isRelayOpen)
  {
    return false;
  }

  if (progState.lastLedPatternMillis == 0)
  {
    return true;
  }

  unsigned long now = millis();

  if (now < progState.lastLedPatternMillis)
  {
    Serial.println(F("Overflow checking LED schedule"));
    progState.lastLedPatternMillis = 0;
    return false;
  }

  unsigned long diff = now - progState.lastLedPatternMillis;

  unsigned long minDiff = LED_PATTERN_DELAY_MS +
                          LED_PATTERN_CLEAR_MS +
                          knockPattern[KNOCK_PATTERN_SIZE - 1];

  return diff >= minDiff;
}

/**
   Relay functions.
*/

void lockRelay()
{
  Serial.println(F("Locking relay"));
  digitalWrite(RELAY_PIN, LOW);
  progState.isRelayOpen = false;
}

void openRelay()
{
  Serial.println(F("Opening relay"));
  digitalWrite(RELAY_PIN, HIGH);
  progState.isRelayOpen = true;
}

void initRelay()
{
  pinMode(RELAY_PIN, OUTPUT);
  lockRelay();
}

/**
   Program state functions.
*/

void updateState()
{
  if (!progState.isAudioPatternOk && !isAudioPatternOk())
  {
    return;
  }

  progState.isAudioPatternOk = true;

  if (!isTrackPlaying() && !progState.hasPlayedFinalAudio)
  {
    Serial.println(F("Playing final audio"));
    progState.hasPlayedFinalAudio = true;
    playTrack(PIN_AUDIO_FINAL);
  }

  if (isLedPatternStartAllowed())
  {
    startLedPattern();
  }

  updateLedPattern();

  if (!progState.isRelayOpen && isValidKnockPattern())
  {
    openRelay();
  }

  if (progState.ledKnockClearCountdown > 0)
  {
    progState.ledKnockClearCountdown--;
  }
  else
  {
    clearLedsKnock();
  }
}

void onStateTimer(int idx, int v, int up)
{
  updateState();
}

void initStateTimer()
{
  timerState
      .begin(STATE_TIMER_MS)
      .repeat(-1)
      .onTimer(onStateTimer)
      .start();
}

/**
   Entrypoint.
*/

void setup()
{
  Serial.begin(9600);

  initRelay();
  initProxSensors();
  initLedStrip();
  initAudioPins();
  resetAudio();
  initKnockSensor();
  initStateTimer();

  Serial.println(F(">> Starting Palormonio program"));
  Serial.flush();
}

void loop()
{
  automaton.run();
}
