#include <Arduino.h>

#include <DHT.h> // Підключення бібліотеки для роботи з сенсором DHT22

// Wokwi підтримує симуляцію апаратного генератора випадкових чисел ESP32,
// тому використаємо esp_random() як початкове значення (seed)
// для генератора random()
#include "esp_random.h" 

#define DHT_PIN 4 // Пін, до якого підключено сенсор DHT22
#define DHT_TYPE DHT22 // Тип сенсора
#define BUTTON_PIN 14 // Пін, до якого підключено кнопку для перемикання джерела даних

DHT dht(DHT_PIN, DHT_TYPE);

struct SensorData {
  float temperature; // 4 байти; Температура в градусах Цельсія
  float humidity; // 4 байти; Вологість у відсотках
  unsigned long timestamp; // 4 байти; Час у мс від моменту запуску програми
};

SensorData sensorData; // 12 байт; Структура для зберігання даних сенсора

// Глобальні змінні

// Основний режим роботи — генерація випадкових даних
// DHT22 використовується після перемикання режиму кнопкою (було трохи вільного часу)
bool useDHT = false; // Початково використовуємо випадкові дані, а не DHT22

unsigned long lastSensorTime = 0; // Час останнього зчитування даних сенсора
unsigned long lastHeapTime = 0; // Час останньої перевірки вільної пам'яті

const unsigned long SENSOR_INTERVAL = 20000; // Інтервал зчитування даних сенсора (20 секунд)
const unsigned long HEAP_INTERVAL = 60000; // Інтервал перевірки вільної пам'яті (60 секунд)

bool lastButtonState = HIGH; // Початковий стан кнопки (HIGH, оскільки використовується INPUT_PULLUP)
bool buttonState = HIGH; // Поточний стан кнопки

unsigned long lastDebounceTime = 0; // Час останньої зміни стану кнопки
const unsigned long DEBOUNCE_DELAY = 50; // Затримка для дебаунсу кнопки (50 мс)

// ФУНКЦІЇ

// Генерація випадкових даних
// +1 до верхнього діапазону, оскільки random() не включає верхню межу https://doc.arduino.ua/ru/prog/Random
void generateRandomData() {
  sensorData.temperature = random(150, 301) / 10.0; // Генерація випадкової температури від 15.0 до 30.0 градусів Цельсія
  sensorData.humidity = random(300, 651) / 10.0; // Генерація випадкової вологості від 30.0% до 65.0%
  sensorData.timestamp = millis(); // Збереження часу зчитування даних
}

// Перевірка коректності даних сенсора DHT22
// Bit 0 = temperature OK
// Bit 1 = humidity OK
uint8_t validateSensorData(float temperature, float humidity) {

  uint8_t sensorStatus = 0b00000000; // Ініціалізація статусу сенсора (усі біти встановлені в 0)

  if (!isnan(temperature)) {
    sensorStatus |= 0b00000001; // Встановлюємо Bit 0
  }

  if (!isnan(humidity)) {
    sensorStatus |= 0b00000010; // Встановлюємо Bit 1
  }

  return sensorStatus;
}

// Читання даних з DHT22
bool readDHTData() {
  
  float humidity = dht.readHumidity(); // Зчитування вологості
   float temperature = dht.readTemperature(); // Зчитування температури

  // Перевірка коректності отриманих даних
  uint8_t sensorStatus = validateSensorData(temperature, humidity);

  // Для коректних даних Bit 0 і Bit 1 повинні бути встановлені в 1
  if ((sensorStatus & 0b00000011) != 0b00000011) {

    Serial.print("DHT22 read error | Status: 0b");
    Serial.println(sensorStatus, BIN);

    return false;
  }

  sensorData.temperature = temperature; // Збереження температури в структурі sensorData
  sensorData.humidity = humidity; // Збереження вологості в структурі sensorData
  sensorData.timestamp = millis(); // Збереження часу зчитування даних

  return true;
}

// Вивід даних сенсора
void printSensorData() {
  Serial.print("Timestamp: "); // Вивід часу зчитування даних сенсора
  Serial.print(sensorData.timestamp / 1000); // Перетворення мілісекунд у секунди для зручності
  Serial.print(" s");  // Вивід одиниці виміру часу

  Serial.print(" | Temperature: "); 
  Serial.print(sensorData.temperature, 1); // Вивід температури з одним десятковим знаком
  Serial.print(" °C"); // Вивід одиниці виміру температури

  Serial.print(" | Humidity: "); 
  Serial.print(sensorData.humidity, 1); // Вивід вологості з одним десятковим знаком
  Serial.print(" %"); // Вивід одиниці виміру вологості

  Serial.print(" | Source: "); // Вивід джерела даних (DHT22 або RANDOM)

  if (useDHT) {
    Serial.println("DHT22"); 
  } else {
    Serial.println("RANDOM"); 
  }
}


// Обробка кнопки
void handleButton() {
  bool reading = digitalRead(BUTTON_PIN); // Зчитування поточного стану кнопки

  // Перевірка на зміну стану кнопки
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  // Перевірка, чи пройшов час дебаунсу
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {

    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == LOW) {
        useDHT = !useDHT;

        // Вивід повідомлення про зміну джерела даних
        // Якщо одне натискання визначається кілька разів,
        // можна збільшити DEBOUNCE_DELAY
        if (useDHT) {
          Serial.println("Data source switched to DHT22"); 
        } else {
          Serial.println("Data source switched to RANDOM");
        }
      }
    }
  }

  lastButtonState = reading; // Оновлення останнього стану кнопки
}


void setup() {
  Serial.begin(115200);

  delay(1000); // Затримка для стабілізації серійного з'єднання

  dht.begin(); // Ініціалізація сенсора DHT22

  pinMode(BUTTON_PIN, INPUT_PULLUP); // Використовуємо внутрішній підтягуючий резистор для кнопки

  randomSeed(esp_random()); // Ініціалізація генератора випадкових чисел за допомогою апаратного генератора ESP32

  Serial.println();
  Serial.println("System started");
  Serial.println("Data source: RANDOM");
}


void loop() {

  handleButton();

  // Дані сенсора кожні 20 секунд
  if (millis() - lastSensorTime >= SENSOR_INTERVAL) {
    lastSensorTime = millis();

    bool dataValid = true; // Змінна для перевірки валідності даних сенсора

    // Зчитування даних з сенсора або генерація випадкових даних залежно від стану кнопки
    if (useDHT) {
      dataValid = readDHTData();
    } else {
      generateRandomData();
    }

    // Вивід даних сенсора
    if (dataValid) {
      printSensorData();
    }
  }

  // Контроль Heap кожні 60 секунд
  if (millis() - lastHeapTime >= HEAP_INTERVAL) {
    lastHeapTime = millis();

    Serial.print("Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
  }
}