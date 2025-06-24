
#define BLYNK_TEMPLATE_ID "TMPL4lqe4UL05"
#define BLYNK_TEMPLATE_NAME "Katerina"
#define BLYNK_AUTH_TOKEN "-GSfjOdLUmTy84EGZljPjYFhWYmLfhKf"
#define BLYNK_PRINT Serial

#define NOTIF_PIN V10  // Виртуальный пин для уведомлений

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// --- Настройка пинов ---
#define DIR_PIN_MAIN 16
#define STEP_PIN_1 19
#define STEP_PIN_2 18
#define STEP_PIN_3 17
#define LED_RED 14
#define LED_GREEN 13
#define LED_BLUE 12
#define ENDSTOP_PIN 27

// --- Конфигурация ---
#define RIGHT_DIRECTION LOW
#define LEFT_DIRECTION HIGH
#define STEP_DELAY 1000
#define CALIBRATION_TIMEOUT 30000
#define WIFI_TIMEOUT 20000

// Настройки сети
const char* ssid = "TP-Link_5C06";
const char* password = "12853298";

// Координаты точек
const long stepsToPoints[] = {0, 0, 825, 1650, 2500, 3375, 4125};

// Глобальные переменные
bool isCalibrated = false;
long currentPosition = 0;
bool isMoving = false;

void printWiFiStatus() {
  Serial.print("Статус WiFi: ");
  switch(WiFi.status()) {
    case WL_CONNECTED: Serial.println("Подключено"); break;
    case WL_NO_SSID_AVAIL: Serial.println("Сеть не найдена"); break;
    case WL_CONNECT_FAILED: Serial.println("Ошибка подключения"); break;
    default: Serial.println("Неизвестная ошибка"); break;
  }
}

void setLED(int r, int g, int b) {
  digitalWrite(LED_RED, r);
  digitalWrite(LED_GREEN, g);
  digitalWrite(LED_BLUE, b);
}

void sendNotification(const String& message) {
  Blynk.virtualWrite(NOTIF_PIN, message);  
  Serial.println("Уведомление: " + message);  
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Инициализация пинов
  pinMode(DIR_PIN_MAIN, OUTPUT);
  pinMode(STEP_PIN_1, OUTPUT);
  pinMode(STEP_PIN_2, OUTPUT);
  pinMode(STEP_PIN_3, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(ENDSTOP_PIN, INPUT_PULLUP);

  setLED(HIGH, LOW, LOW); // Красный - инициализация

  // Подключение к WiFi
  Serial.println("\nПодключение к WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startTime = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT) {
    delay(500);
    Serial.print(".");
    setLED(!digitalRead(LED_RED), LOW, LOW); // Мигание красным
  }

  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi подключен!");
    printWiFiStatus();
    
    // Подключение к Blynk
    Blynk.config(BLYNK_AUTH_TOKEN);
    if(Blynk.connect()) {
      Serial.println("Blynk подключен");
      setLED(LOW, HIGH, LOW);
    } else {
      Serial.println("Ошибка подключения к Blynk");
      setLED(HIGH, HIGH, LOW);
    }
  } else {
    Serial.println("\nОшибка подключения к WiFi!");
    printWiFiStatus();
    setLED(HIGH, LOW, LOW); 
  }
}

void runSecondaryMotors() {
  if(isMoving) return;
  isMoving = true;
  
  // Движение влево
  Serial.println("Запуск вторичных двигателей: ВЛЕВО");
  digitalWrite(DIR_PIN_MAIN, HIGH);
  for(int i = 0; i < 3300; i++) {
    if(digitalRead(ENDSTOP_PIN) == LOW) break;
    
    digitalWrite(STEP_PIN_2, HIGH);
    digitalWrite(STEP_PIN_3, HIGH);
    delayMicroseconds(STEP_DELAY);
    digitalWrite(STEP_PIN_2, LOW);
    digitalWrite(STEP_PIN_3, LOW);
    delayMicroseconds(STEP_DELAY);
    
    if(i % 100 == 0) Blynk.run();
  }

  // Пауза
  delay(2500);

  // Движение вправо
  Serial.println("Запуск вторичных двигателей: ВПРАВО");
  digitalWrite(DIR_PIN_MAIN, LOW);
  for(int i = 0; i < 3300; i++) {
    digitalWrite(STEP_PIN_2, HIGH);
    digitalWrite(STEP_PIN_3, HIGH);
    delayMicroseconds(STEP_DELAY);
    digitalWrite(STEP_PIN_2, LOW);
    digitalWrite(STEP_PIN_3, LOW);
    delayMicroseconds(STEP_DELAY);
    
    if(i % 100 == 0) Blynk.run();
  }
  
  isMoving = false;
  Serial.println("Вторичные двигатели завершили работу");
}

bool calibrate() {
  if(isMoving) return false;
  isMoving = true;
  
  Serial.println("Начало калибровки...");
  digitalWrite(DIR_PIN_MAIN, RIGHT_DIRECTION);
  setLED(LOW, LOW, HIGH); 

  unsigned long startTime = millis();
  bool success = false;
  
  while(digitalRead(ENDSTOP_PIN)) {
    if(millis() - startTime > CALIBRATION_TIMEOUT) {
      Serial.println("Таймаут калибровки!");
      break;
    }
    
    digitalWrite(STEP_PIN_1, HIGH);
    delayMicroseconds(STEP_DELAY);
    digitalWrite(STEP_PIN_1, LOW);
    delayMicroseconds(STEP_DELAY);
    
    Blynk.run();
  }

  if(!digitalRead(ENDSTOP_PIN)) {
    currentPosition = 0;
    success = true;
    Serial.println("Калибровка успешно завершена");
    Blynk.virtualWrite(V6, 0);
    sendNotification("Калибровка завершена");
  } else {
    Serial.println("Ошибка калибровки!");
    sendNotification("Ошибка калибровки!");
  }

  isMoving = false;
  setLED(LOW, HIGH, LOW);
  return success;
}

void moveToPoint(int point) {
  if(point < 1 || point > 6) {
    Serial.println("Неверный номер точки (1-6)");
    return;
  }
  
  if(!isCalibrated) {
    Serial.println("Сначала выполните калибровку!");
    sendNotification("Требуется калибровка!");
    return;
  }

  if(isMoving) {
    Serial.println("Движение уже выполняется!");
    return;
  }
  
  isMoving = true;
  long target = stepsToPoints[point];
  long steps = target - currentPosition;
  int dir = (steps > 0) ? LEFT_DIRECTION : RIGHT_DIRECTION;
  
  Serial.print("Движение к точке ");
  Serial.print(point);
  Serial.print(" (шагов: ");
  Serial.print(abs(steps));
  Serial.println(")");

  digitalWrite(DIR_PIN_MAIN, dir);
  setLED(LOW, LOW, HIGH); 

  for(long i = 0; i < abs(steps); i++) {
    digitalWrite(STEP_PIN_1, HIGH);
    delayMicroseconds(STEP_DELAY);
    digitalWrite(STEP_PIN_1, LOW);
    delayMicroseconds(STEP_DELAY);
    
    if(i % 50 == 0) Blynk.run();
    
    if(point == 1 && !digitalRead(ENDSTOP_PIN)) break;
  }

  currentPosition = target;
  isMoving = false;
  setLED(LOW, HIGH, LOW); 
  
  Serial.println("Движение завершено");
  sendNotification(String("Достигнута точка ") + point);

  if(point >= 2 && point <= 6) runSecondaryMotors();
}

// Обработчики Blynk
BLYNK_WRITE(V0) { if(param.asInt()) moveToPoint(1); }
BLYNK_WRITE(V1) { if(param.asInt()) moveToPoint(2); }
BLYNK_WRITE(V2) { if(param.asInt()) moveToPoint(3); }
BLYNK_WRITE(V3) { if(param.asInt()) moveToPoint(4); }
BLYNK_WRITE(V4) { if(param.asInt()) moveToPoint(5); }
BLYNK_WRITE(V5) { if(param.asInt()) moveToPoint(6); }
BLYNK_WRITE(V6) { if(param.asInt()) isCalibrated = calibrate(); }

void loop() {
  Blynk.run();
  
  if(Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if(cmd == "c") {
      isCalibrated = calibrate();
    } 
    else if(cmd.toInt() >= 1 && cmd.toInt() <= 6) {
      moveToPoint(cmd.toInt());
    }
    else {
      Serial.println("Доступные команды:");
      Serial.println("c - калибровка");
      Serial.println("1-6 - движение к точке");
    }
  }
  
  if(!Blynk.connected() && WiFi.status() == WL_CONNECTED) {
    Serial.println("Переподключение к Blynk...");
    if(Blynk.connect()) {
      Serial.println("Blynk переподключен");
      setLED(LOW, HIGH, LOW);
    } else {
      setLED(HIGH, HIGH, LOW);
    }
  }
  
  delay(100);
}