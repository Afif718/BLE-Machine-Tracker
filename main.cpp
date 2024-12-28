#include <NimBLEDevice.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <sntp.h>

#define RELAY_PIN 27

// Replace these with your WiFi credentials
#define WIFI_SSID "Your_WiFi_SSID"
#define WIFI_PASSWORD "Your_WiFi_Password"

// Google Apps Script URL for logging data
#define GOOGLE_SCRIPT_URL "Your_Google_Script_URL"

// BLE Scan time (in seconds)
const int scanTime = 5;

NimBLEScan* pBLEScan;
bool relayState = false;
const char* currentUserUUID = nullptr;
unsigned long startMillis = 0;
unsigned long endMillis = 0;
String globalStartTime = "";

// UUIDs and corresponding usernames (update as needed)
const char uuids[][37] PROGMEM = {
    "2F234454-CF6D-4A0F-ADF2-F4911BA9FFA6",
    "12345678-1234-5678-1234-567812345678",
    "87654321-4321-8765-4321-876543218765"
};

const char* usernames[] PROGMEM = {
    "Worker_A",
    "Worker_B",
    "Worker_C"
};

// Get formatted current time
String getFormattedTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return "";
    }
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
    return String(buffer);
}

// Get username based on UUID
const char* getUsername(const char* uuid) {
    for (int i = 0; i < sizeof(uuids) / sizeof(uuids[0]); i++) {
        if (strcmp_P(uuid, uuids[i]) == 0) {
            return usernames[i];
        }
    }
    return "";
}

// Format duration in human-readable form
String formatDuration(unsigned long seconds) {
    if (seconds < 60) {
        return String(seconds) + " seconds";
    }
    unsigned long minutes = seconds / 60;
    unsigned long remainingSeconds = seconds % 60;
    if (remainingSeconds == 0) {
        return String(minutes) + " minutes";
    }
    return String(minutes) + " minutes " + String(remainingSeconds) + " seconds";
}

// Send data to Google Sheet
void sendToGoogleSheet(const char* name, const String& start, const String& end, const String& duration) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(GOOGLE_SCRIPT_URL);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");

        String httpRequestData = "username=" + String(name) + 
                               "&startTime=" + start + 
                               "&endTime=" + end + 
                               "&duration=" + duration;
        
        int httpResponseCode = http.POST(httpRequestData);
        
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Response Code: " + String(httpResponseCode));
            Serial.println("Response Body: " + response);
        } else {
            Serial.print("Error on sending POST: ");
            Serial.println(httpResponseCode);
        }
        http.end();
    }
}

// BLE callback class for processing scanned devices
class MyAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        if (advertisedDevice->haveManufacturerData() && advertisedDevice->getManufacturerData().length() >= 22) {
            std::string manufacturerData = advertisedDevice->getManufacturerData();
            std::string uuid = manufacturerData.substr(4, 16);

            char uuidString[37];
            snprintf(uuidString, sizeof(uuidString), 
                    "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                    (uint8_t)uuid[0], (uint8_t)uuid[1], (uint8_t)uuid[2], (uint8_t)uuid[3],
                    (uint8_t)uuid[4], (uint8_t)uuid[5], (uint8_t)uuid[6], (uint8_t)uuid[7],
                    (uint8_t)uuid[8], (uint8_t)uuid[9], (uint8_t)uuid[10], (uint8_t)uuid[11],
                    (uint8_t)uuid[12], (uint8_t)uuid[13], (uint8_t)uuid[14], (uint8_t)uuid[15]);

            const char* detectedUser = getUsername(uuidString);

            if (detectedUser && strlen(detectedUser) > 0) {
                if (currentUserUUID == detectedUser && relayState) return;

                if (relayState) {
                    endMillis = millis();
                    String endTimeStr = getFormattedTime();
                    unsigned long durationSeconds = (endMillis - startMillis) / 1000;
                    String formattedDuration = formatDuration(durationSeconds);
                    
                    sendToGoogleSheet(currentUserUUID, globalStartTime, endTimeStr, formattedDuration);
                    relayState = false;
                    digitalWrite(RELAY_PIN, LOW);
                }

                currentUserUUID = detectedUser;
                startMillis = millis();
                globalStartTime = getFormattedTime();
                relayState = true;
                digitalWrite(RELAY_PIN, HIGH);
            }
        }
    }
};

void setup() {
    Serial.begin(115200);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi!");

    configTime(6 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr);
    while (now < 24 * 3600) {
        delay(500);
        Serial.println("Waiting for time sync...");
        now = time(nullptr);
    }

    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
}

void loop() {
    NimBLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    if (foundDevices.getCount() == 0 && relayState) {
        digitalWrite(RELAY_PIN, LOW);
        relayState = false;
        endMillis = millis();
        String endTimeStr = getFormattedTime();
        unsigned long durationSeconds = (endMillis - startMillis) / 1000;
        String formattedDuration = formatDuration(durationSeconds);
        
        sendToGoogleSheet(currentUserUUID, globalStartTime, endTimeStr, formattedDuration);
        currentUserUUID = nullptr;
    }
    delay(1000);
}
