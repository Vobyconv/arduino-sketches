#include <CircularBuffer.h>

#define STATE_STABLE 1
#define STATE_VIBRATING 2
#define STATE_UNKNOWN 3

struct sensorSample {
  unsigned long tstamp;
  int level;
};

struct stateSample {
  unsigned long tstamp;
  byte state;
};

typedef struct sensorSample SensorSample;
typedef struct stateSample StateSample;

// Pin connected to the vibration sensor
const byte SENSOR_PIN = 2;

// Size of the sensor samples buffer
const int SENSOR_BUFFER_SIZE = 100;

// Buffer that will contain the vibration sensor readings
CircularBuffer<SensorSample, SENSOR_BUFFER_SIZE> sensorBuffer;

// Size of the vibration states buffer
const int STATE_BUFFER_SIZE = 10;

// Buffer that will contain the history of vibration states
CircularBuffer<StateSample, STATE_BUFFER_SIZE> stateBuffer;

// Vibration ratio threshold
const float VIBRATION_RATIO = 0.7;

// Iteration delay (ms)
const int LOOP_WAIT_MS = 10;

// Last observed state
byte lastState = STATE_UNKNOWN;

/**
   Read the sensor and push the value to the buffer.
*/
void updateSensorBuffer() {
  int sensorVal = digitalRead(SENSOR_PIN);

  SensorSample sample;
  sample.tstamp = millis();
  sample.level = sensorVal;

  sensorBuffer.push(sample);

  digitalWrite(13, sensorVal);
}

/**
   Returns the current vibration sensor status.
*/
byte getSensorBufferState() {
  if (sensorBuffer.isFull() == false) {
    return STATE_UNKNOWN;
  }

  int counterHi = 0;
  int counterLo = 0;

  for (int i = 0; i < sensorBuffer.size(); i++) {
    if (sensorBuffer[i].level == HIGH) {
      counterHi += 1;
    } else {
      counterLo += 1;
    }
  }

  float bufferSize = sensorBuffer.size();

  if (counterHi / bufferSize >= VIBRATION_RATIO) {
    return STATE_STABLE;
  } else if (counterLo / bufferSize >= VIBRATION_RATIO) {
    return STATE_STABLE;
  } else {
    return STATE_VIBRATING;
  }
}

/**
   Reads the current sensor state and updates the state buffer.
*/
void updateStateBuffer() {
  byte currentState = getSensorBufferState();

  if (currentState == STATE_UNKNOWN) {
    return;
  }

  StateSample sample;
  sample.tstamp = millis();
  sample.state = currentState;

  Serial.print("updateStateBuffer: ");
  Serial.println(currentState == STATE_STABLE ? "STABLE" : "VIBRATING");
  Serial.flush();

  stateBuffer.push(sample);
}

void setup() {
  Serial.begin(9600);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(13, OUTPUT);
}

void loop() {
  updateSensorBuffer();
  updateStateBuffer();

  delay(LOOP_WAIT_MS);
}
