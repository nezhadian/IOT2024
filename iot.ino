#include <LiquidCrystal.h> // کتابخانه برای کنترل LCD
#include <WiFi.h> // کتابخانه برای اتصال به Wi-Fi
#include <PubSubClient.h> // کتابخانه برای ارتباط با سرور MQTT

// -------------------- تنظیمات فن --------------------
const int fanPin = 18; // پایه‌ای که فن به آن متصل است
long curFanSpeed = 0; // سرعت فعلی فن

// -------------------- تنظیمات سنسور رطوبت خاک --------------------
const int soilPin = 35; // پایه‌ای که سنسور رطوبت خاک به آن متصل است

// -------------------- تنظیمات LCD --------------------
LiquidCrystal lcd(13, 12, 14, 27, 26, 25); // تنظیم پایه‌های متصل به LCD

// -------------------- تنظیمات MQTT --------------------
const char* ssid = "BB"; // نام شبکه Wi-Fi
const char* password = "yasiyasi"; // رمز شبکه Wi-Fi

const char* mqttServer = "192.168.43.1"; // آدرس IP سرور MQTT
const int mqttPort = 1884; // پورت سرور MQTT
const char* mqttClientName = "ArashYasinOmid"; // نام کلاینت MQTT
const char* mqttFanSpeedTopic = "AYO/fan"; // موضوع (Topic) برای سرعت فن
const char* mqttSoilMoistureTopic = "AYO/soil"; // موضوع (Topic) برای رطوبت خاک

WiFiClient espClient; // شیء برای ارتباط با Wi-Fi
PubSubClient client(espClient); // شیء برای ارتباط با سرور MQTT

// -------------------- تابع setup --------------------
void setup() {
  // تنظیمات PWM برای کنترل فن
  const int pwmFreq = 20000;   // فرکانس PWM (۲۰ کیلوهرتز)
  const int pwmChannel = 0;    // کانال PWM
  const int pwmResolution = 8; // رزولوشن PWM (۸ بیت = مقادیر ۰ تا ۲۵۵)
  ledcAttachChannel(fanPin, pwmFreq, pwmResolution, 8); // اتصال پایه فن به کانال PWM
  setFanSpeed(255); // تنظیم سرعت اولیه فن (خاموش)

  // شروع ارتباط سریال برای نمایش اطلاعات
  Serial.begin(115200);

  // تنظیم پایه سنسور رطوبت خاک به عنوان ورودی
  pinMode(soilPin, INPUT);

  // تنظیم ابعاد LCD (۱۶ ستون و ۲ سطر)
  lcd.begin(16, 2);

  // اتصال به Wi-Fi و تنظیم سرور MQTT
  WiFi.begin(ssid, password);
  client.setServer(mqttServer, mqttPort);
}

// -------------------- توابع کنترل فن --------------------
void setFanSpeed(int speed) {
  ledcWrite(fanPin, speed);  // تنظیم سرعت فن با استفاده از PWM
  curFanSpeed = speed; // ذخیره سرعت فعلی فن
}

void setFanSpeedBasedOnSoilMoisture(int soilValue) {
  // تنظیم سرعت فن بر اساس مقدار رطوبت خاک
  if (soilValue < 30)
    setFanSpeed(255); // فن خاموش
  else if (soilValue < 70)
    setFanSpeed(128); // فن با سرعت متوسط
  else if (soilValue <= 100)
    setFanSpeed(0); // فن با حداکثر سرعت
  else
    setFanSpeed(255); // فن خاموش
}

// -------------------- توابع LCD --------------------
void refreshLCD(int soilValue, int speed) {
  lcd.clear(); // پاک کردن صفحه LCD
  lcd.setCursor(0, 0);  // قرار دادن کرسر در ستون ۰ و سطر ۰
  lcd.print("Soil  ");  // نمایش متن "Soil  "
  lcd.print(soilValue); // نمایش مقدار رطوبت خاک
  lcd.setCursor(0, 1);  // قرار دادن کرسر در ستون ۰ و سطر ۱
  lcd.print("RPM  ");  // نمایش متن "RPM  "
  lcd.print(speed); // نمایش مقدار سرعت فن
}

// -------------------- توابع سنسور رطوبت خاک --------------------
int readSoilMoistureValue() {
  int analog = analogRead(soilPin); // خواندن مقدار آنالوگ از سنسور
  int soilValue = 100 - map(analog, 0, 4095, 0, 100); // تبدیل مقدار آنالوگ به درصد رطوبت
  Serial.print("Debug:soil analog "); // نمایش مقدار آنالوگ در سریال مانیتور
  Serial.println(analog);
  Serial.print("Debug:soil value "); // نمایش مقدار رطوبت خاک در سریال مانیتور
  Serial.println(soilValue);
  return soilValue; // بازگرداندن مقدار رطوبت خاک
}

// -------------------- توابع MQTT --------------------
char* intToCharPtr(int value) {
  // تبدیل عدد به رشته
  int size = snprintf(nullptr, 0, "%d", value) + 1; // محاسبه اندازه رشته
  char* buffer = (char*)malloc(size); // اختصاص حافظه برای رشته

  if (buffer == nullptr) {
    return nullptr; // در صورت خطا در اختصاص حافظه
  }

  snprintf(buffer, size, "%d", value); // تبدیل عدد به رشته
  return buffer; // بازگرداندن رشته
}

void publishMQTTData(int soil, int fanSpeed) {
  // بررسی اتصال به Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("MQTT: Wifi is not connected"); // چاپ پیام خطا اگر Wi-Fi وصل نباشد
    return; // خروج از تابع
  }

  // بررسی اتصال به سرور MQTT
  if (!client.connected()) {
    client.connect(mqttClientName); // تلاش برای اتصال به سرور MQTT
    Serial.println("MQTT: Attempting to connect to broker..."); // چاپ پیام در حال اتصال
    return; // خروج از تابع
  }

  // تبدیل مقادیر به رشته و ارسال به سرور MQTT
  char* fanSpeedPtr = intToCharPtr(fanSpeed); // تبدیل سرعت فن به رشته
  char* soilSpeedPtr = intToCharPtr(soil); // تبدیل رطوبت خاک به رشته

  client.publish(mqttFanSpeedTopic, fanSpeedPtr); // ارسال سرعت فن
  client.publish(mqttSoilMoistureTopic, soilSpeedPtr); // ارسال رطوبت خاک

  free(fanSpeedPtr); // آزاد کردن حافظه
  free(soilSpeedPtr); // آزاد کردن حافظه

  Serial.println("MQTT: data published."); // چاپ پیام موفقیت‌آمیز بودن ارسال داده‌ها
}

// -------------------- تابع محاسبه سرعت واقعی فن --------------------
int getActualFanSpeed(int value) {
  return 12000 - map(value, 0, 255, 0, 12000); // تبدیل مقدار PWM به سرعت واقعی (RPM)
}

// -------------------- تابع loop --------------------
void loop() {
  // خواندن مقدار رطوبت خاک
  float soilValue = readSoilMoistureValue();
  Serial.print("Sensor: Soil ");
  Serial.println(soilValue);

  // تنظیم سرعت فن بر اساس رطوبت خاک
  setFanSpeedBasedOnSoilMoisture(soilValue);

  // محاسبه سرعت واقعی فن
  int actualFanSpeed = getActualFanSpeed(curFanSpeed);
  Serial.print("Sensor: Fan speed ");
  Serial.println(actualFanSpeed);

  // بروزرسانی اطلاعات روی LCD
  refreshLCD(soilValue, actualFanSpeed);

  // ارسال داده‌ها به سرور MQTT
  publishMQTTData(soilValue, actualFanSpeed);

  // تاخیر ۵۰۰ میلی‌ثانیه قبل از تکرار حلقه
  delay(500);
}