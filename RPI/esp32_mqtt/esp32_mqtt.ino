#include "mqtt_handler.h"

// إنشاء كائن (Object) كواجهة للتحكم في مكتبتنا
MqttHandler mqttObj;

// هذه الدالة ستعمل تلقائياً حين يصل أمر أو رسالة من سيرفر HiveMQ
void myMqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived on topic [");
    Serial.print(topic);
    Serial.print("] : ");
    
    // نقوم بقراءة وطباعة الرسالة الواردة حرفاً تلو الآخر
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
    
    // 💡 يمكنك إضافة شروطنا المخصصة هنا:
    // مثلاً لو استلمنا كلمة "ON" نقوم بتشغيل الريلاي / اللمبة
    // if (strncmp((char*)payload, "ON", length) == 0) {
    //      digitalWrite(LED_BUILTIN, HIGH);
    // }
}

void setup() {
    // تهيئة سرعة الشاشة التسلسلية Serial Monitor
    Serial.begin(115200);
    delay(10); // وقفة قصيرة لثبات البوردة
    
    // تشغيل الاتصال بالواي فاي والـ MQTT
    mqttObj.setup();
    
    // إعلام الكود بالدالة التي ستستلم الرسائل
    mqttObj.setCallback(myMqttCallback);
    
    // مثال لتعريف بن (pin) للمبة خارجية
    // pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    // هذه الدالة السحرية تبقي الاتصال نشطاً، وإذا انقطع النت أو السيرفر
    // تحاول إعادة الاتصال بذكاء وبدون أن تجمد (Block) باقي البرنامج
    mqttObj.loop();

    // 💡 مساحتك لكتابة الكود الخاص بالمشروع ...
    // مثال للإرسال كل فتره زمنية (مثلاً إرسال قراءات حساس بدون استخدام delay):
    /*
    static unsigned long lastMsg = 0;
    unsigned long now = millis();
    if (now - lastMsg > 10000) {  // إرسال كل 10 ثواني (10000 ملي ثانية)
        lastMsg = now;
        mqttObj.publish("esp32/sensor_data", "Temperature is 25C");
    }
    */
}
