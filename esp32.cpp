#define PIN_TRIG 18
#define PIN_ECHO 19
#define PIN_LED 21
#define PIN_BUZZER 22
#define PIN_SWITCH 23

const int DIST_THRESHOLD = 150; // cm

void setup() {
  Serial.begin(115200);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_SWITCH, INPUT);

  digitalWrite(PIN_TRIG, LOW);
  digitalWrite(PIN_LED, LOW);
  noTone(PIN_BUZZER);

  Serial.println("System ready!");
}

// Đọc khoảng cách từ HC-SR04
long readDistanceCM() {
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long duration = pulseIn(PIN_ECHO, HIGH, 30000);
  if (duration == 0) return -1;
  return duration / 58;
}

void loop() {
  long distance = readDistanceCM();
  int switchState = digitalRead(PIN_SWITCH); // Đọc trạng thái công tắc (HIGH hoặc LOW).

  bool objectClose = (distance > 0 && distance <= DIST_THRESHOLD);
  // true nếu có vật nằm trong khoảng cách báo động (≤150 cm)
  // false nếu xa hơn hoặc không đo được.
  // ========================================================================================
  bool alarmOn = false;

  // switchState == LOW → cho phép báo động
  if (switchState == LOW && objectClose) {
    alarmOn = true;
  }

  // Điều khiển LED và Buzzer
  if (alarmOn) {
    digitalWrite(PIN_LED, HIGH);
    tone(PIN_BUZZER, 1000);
  } else {
    digitalWrite(PIN_LED, LOW);
    noTone(PIN_BUZZER);
  }

  // Xuất ra Serial
  Serial.print("Distance: ");
  if (distance < 0) Serial.print("Out of range");
  else Serial.print(distance);
  Serial.print(" cm | Switch: ");
  Serial.print(switchState == HIGH ? "LEFT ( OFF)" : "RIGHT (Active)");
  Serial.print(" | Alarm: ");
  Serial.println(alarmOn ? "ACTIVE" : "OFF");

  delay(300);
}
