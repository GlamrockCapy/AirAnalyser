#include "MQUnifiedsensor.h"
#include <DHT.h>

// --- Пины ---
#define PIN_MQ135 A0
#define DHTPIN    5
#define RGB_R     9
#define RGB_G     10
#define RGB_B     11
#define BUZZER    6

// --- MQ-135 параметры ---
#define RL_VALUE        9.83
#define ADC_RESOLUTION  10
#define VREF            5.0

// --- DHT ---
#define DCTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
MQUnifiedsensor MQ135("Arduino", VREF, ADC_RESOLUTION, PIN_MQ135, "MQ-135");

// --- Конфигурация MQ (калибровка под CO2) ---
// Источник: Sandbox Electronics (значения для CO2)
const float MQ_A = 574.25;
const float MQ_B = -2.23;

// --- Параметры тревоги ---
const int WINDOW_SIZE = 5;
float history[WINDOW_SIZE] = {400.0};
int historyIndex = 0;

// Пороги изменения (настрой опытным путём):
const float DERIVATIVE_THRESHOLD = 25.0;
const float DERIVATIVE_CRITICAL = 60.0;

// --- Влажностная коррекция ---
const float ALPHA = 0.006;
const float RH_REF = 50.0;

// --- Функция: установка цвета RGB  ---
void setRGBColor(int r, int g, int b) {
  analogWrite(RGB_R, r);
  analogWrite(RGB_G, g);
  analogWrite(RGB_B, b);
}

// --- Функция: короткий звуковой сигнал ---
void beep(int duration_ms) {
  tone(BUZZER, 2000);      // 2 кГц
  delay(duration_ms);
  noTone(BUZZER);
}

void setup() {
  //Serial.begin(9600);

  // Инициализация пинов
  pinMode(RGB_R, OUTPUT);
  pinMode(RGB_G, OUTPUT);
  pinMode(RGB_B, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  noTone(BUZZER);

  // Инициализация датчиков
  dht.begin();
  MQ135.setRL(RL_VALUE);
  MQ135.init();
  MQ135.setRegressionMethod(1);
  MQ135.setA(MQ_A);
  MQ135.setB(MQ_B);

  // Быстрая калибровка R0
  //Serial.println("Calibrating MQ-135 (quick, assuming ~400 ppm CO2)...");
  delay(3000);

  int raw = analogRead(PIN_MQ135);
  float VRL = (raw / 1023.0) * VREF;
  float Rs = (VREF / VRL - 1.0) * RL_VALUE;
  float R0 = Rs / 1.0;
  MQ135.setR0(R0);
  //Serial.print("R0 = "); Serial.println(R0, 3);
  //Serial.println("Ready");
}

void loop() {
  // --- Чтение влажности и температуры ---
  float hum = dht.readHumidity();
  float temp = dht.readTemperature();

  if (isnan(hum) || isnan(temp)) {
    //Serial.println("DHT read failed — using fallback humidity");
    hum = RH_REF;
  }

  // --- Чтение газа ---
  MQ135.update();
  float raw_ppm = MQ135.readSensor();
  if (raw_ppm < 0) raw_ppm = 0;

  // --- Коррекция на влажность  ---
  float humidity_delta = hum - RH_REF;
  float factor = 1.0 + ALPHA * humidity_delta;
  factor = constrain(factor, 0.7, 1.6);
  float corrected_ppm = raw_ppm * factor;

  // --- Обновление истории ---
  history[historyIndex] = corrected_ppm;
  historyIndex = (historyIndex + 1) % WINDOW_SIZE;

  // --- Скользящее среднее ---
  float sum = 0;
  for (int i = 0; i < WINDOW_SIZE; i++) {
    sum += history[i];
  }
  float avg = sum / WINDOW_SIZE;

  // --- Производная  ---
  float current = corrected_ppm;
  float diff = current - avg;

  // --- Логика состояния ---
  bool warning = (diff > DERIVATIVE_THRESHOLD);
  bool critical = (diff > DERIVATIVE_CRITICAL);

  /* --- Вывод в Serial ---
  Serial.print("T: "); Serial.print(temp, 1);
  Serial.print("°C  RH: "); Serial.print(hum, 1);
  Serial.print("%  PPM: "); Serial.print(current, 1);
  Serial.print("  Avg: "); Serial.print(avg, 1);
  Serial.print("  Δ: "); Serial.print(diff, 1);
  Serial.print("  → ");
  if (critical) Serial.println("CRITICAL");
  else if (warning) Serial.println("WARNING");
  else Serial.println("OK");
  */

  // --- Управление RGB ---
  if (critical) {
    setRGBColor(255, 0, 0);
    beep(200);
    delay(300);
    setRGBColor(0, 0, 0);
    delay(300);
  } else if (warning) {
    setRGBColor(255, 255, 0);
    if (millis() % 2000 < 1000) {
      beep(100);
      delay(900);
    } else {
      delay(1000);
    }
  } else {
    setRGBColor(0, 255, 0);
    delay(1000);
  }
}
