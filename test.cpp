#include <WiFi.h>              // Kết nối WiFi cho ESP32
#include <WiFiClientSecure.h>  // Giao tiếp bảo mật SSL/TLS (HTTPS, MQTTs)
#include <PubSubClient.h>      // Thư viện MQTT client (gửi/nhận dữ liệu)
#include <ArduinoJson.h>       // Tạo chuỗi JSON để gửi qua MQTT

// ===== Chân thiết bị =====
#define PIN_TRIG 18
#define PIN_ECHO 19
#define PIN_LED 21
#define PIN_BUZZER 22
#define PIN_SWITCH 23

// ===== Cấu hình =====
const int DIST_THRESHOLD = 150; // cm
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

const char* MQTT_SERVER = "ee28d9d3741148dfbc940cd87943327e.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;
const char* MQTT_USER = "khanh";
const char* MQTT_PASS = "Khanh1595730";

WiFiClientSecure espClient; // tạo client SSL bảo mật cho MQTT.
PubSubClient client(espClient); // đối tượng MQTT chính.

// ===== Biến lưu trạng thái cũ =====
// nhớ giá trị cũ, phát hiện thay đổi => gửi MQTT khi có sự kiện mới (tránh spam).
int lastSwitchState = -1;
bool lastNearState = false;
float lastDistance = -1;

void setup_wifi() {
  // Kết nối ESP32 vào mạng WiFi.
  // Hiển thị tiến trình trên Serial Monitor.
  Serial.print("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
}

void reconnect() {
  //   Nếu mất kết nối MQTT → vòng lặp sẽ tự thử lại.
  // Khi kết nối lại thành công, gửi 1 thông báo : esp32/status.
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32DoorSensor", MQTT_USER, MQTT_PASS)) {
      Serial.println("connected!");
      client.publish("esp32/status", "ESP32 connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retry in 5s");
      delay(5000);
    }
  }
}

// ===== Đọc khoảng cách từ HC-SR04 =====
long readDistanceCM() {
  // Gửi xung 10µs ra chân TRIG. => Nhận tín hiệu phản hồi trên ECHO.
  // Dùng pulseIn() để đo thời gian sóng siêu âm đi -> về.
  // distance(cm) = duration / 58.

  // Nếu không nhận tín hiệu → trả về -1.
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long duration = pulseIn(PIN_ECHO, HIGH, 30000);
  if (duration == 0) return -1;
  return duration / 58;
}

void setup() {
  // Tắt LED, buzzer khi khởi động.
  // Gọi setup_wifi() để kết nối WiFi.
  // espClient.setInsecure() : kết nối SSL.
  // Gán thông tin MQTT server.
  Serial.begin(115200);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_SWITCH, INPUT);

  digitalWrite(PIN_TRIG, LOW);
  digitalWrite(PIN_LED, LOW);
  noTone(PIN_BUZZER);

  setup_wifi();
  espClient.setInsecure();  // SSL mà không cần CA
  client.setServer(MQTT_SERVER, MQTT_PORT);

  Serial.println("System ready!");
}

void loop() {
  // Kiểm tra xem MQTT có bị mất kết nối không, nếu mất → tự động gọi reconnect().
  if (!client.connected()) reconnect();
  client.loop();

  long distance = readDistanceCM(); // khoảng cách đọc được.
  int switchState = digitalRead(PIN_SWITCH); // đọc trạng thái công tắc (LOW = bật, HIGH = tắt).
  bool objectClose = (distance > 0 && distance <= DIST_THRESHOLD); // xác định có vật thể trong ngưỡng cảnh báo.
  bool alarmOn = (switchState == LOW && objectClose); // hệ thống chỉ kích hoạt cảnh báo khi công tắc bật và vật thể ở gần.

  // ===== Điều khiển LED + Buzzer =====
  if (alarmOn) {
    //Khi phát hiện vật thể gần → bật LED và còi báo động.
    // Khi xa hoặc switch OFF → tắt LED, tắt còi.
    digitalWrite(PIN_LED, HIGH);
    tone(PIN_BUZZER, 1000);
  } else {
    digitalWrite(PIN_LED, LOW);
    noTone(PIN_BUZZER);
  }

  // ===== JSON mẫu =====
  StaticJsonDocument<128> doc;
  char payload[128];

  // 1️⃣ Gửi khi trạng thái công tắc thay đổi
  if (switchState != lastSwitchState) {
    //Chỉ gửi khi công tắc thay đổi ON ↔ OFF.
    // Gửi dữ liệu lên topic esp32/switch dưới dạng: {"switch": "ON"}
    doc["switch"] = (switchState == LOW ? "ON" : "OFF");
    serializeJson(doc, payload);
    client.publish("esp32/switch", payload);
    Serial.print("MQTT >> ");
    Serial.println(payload);
    lastSwitchState = switchState;
  }

  // 2️⃣ Gửi khi công tắc bật và có thay đổi khoảng cách
  if (switchState == LOW) {   // ✅ chỉ gửi khi switch ON
    //   Chỉ khi switch = ON, mới gửi dữ liệu khoảng cách.

    bool nowNear = objectClose;
    if (nowNear != lastNearState || fabs(distance - lastDistance) > 5) {
    // Gửi khi có:
    // Vật thể tiến lại gần hoặc đi xa.
    // Khoảng cách thay đổi > 5 cm.
      doc.clear();
      doc["distance_cm"] = distance;
      doc["threshold_cm"] = DIST_THRESHOLD;
      doc["alert"] = nowNear;
      doc["device"] = "DoorSensor";

      serializeJson(doc, payload);
    // Topic gửi:
    // esp32/distance/near → khi vật ở gần.
    // esp32/distance/far → khi vật ở xa.

      if (nowNear) {
        client.publish("esp32/distance/near", payload);
      } else {
        client.publish("esp32/distance/far", payload);
      }

      Serial.print("MQTT >> ");
      Serial.println(payload);

      lastNearState = nowNear;
      lastDistance = distance;
    }
  }
  delay(400);
}
// Mở https://www.hivemq.com/demos/websocket-client/

// Điền:

// Host: ee28d9d3741148dfbc940cd87943327e.s1.eu.hivemq.cloud
// Port: 8884
// SSL: ✅
// Username: khanh
// Password: Khanh1595730


// Nhấn Connect

// Thêm subscription:

// esp32/#


// → bạn sẽ thấy 3 nhóm log:

// esp32/switch

// esp32/distance/near

// esp32/distance/far
// =====================================================================================================