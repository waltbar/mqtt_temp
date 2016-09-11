#include <Wire.h>
#include <ESPHTU21D.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <EEPROM.h>

IPAddress ip(XX, XX, XX, XX);
IPAddress dns1(XX, XX, XX, XX);
IPAddress gateway(XX, XX, XX, XX);
IPAddress subnet(XX, XX, XX, XX);

extern "C" {
uint16 readvdd33(void);   // for measure voltage
}

const char* ssid     = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";
const char* user = "MQTT_USERNAME";
const char* pass = "MQTT_PASSWORD";
const char* topic = "out1";
const char* lwt_topic = "out1/lwt";
const char* lwt_message = "Connection died...";
const char* server = "MQTT_SERVER_IP";

float h_measure, t_measure;  // Values read from sensor
int humidity, temp;
byte flag = 0;
int address_flag = 0;
byte count = 0;
int address_count = 5;
long rssi;

// union dateTime {
//         byte b[4];
//         unsigned long l;
// };

union Temp {
        byte b[2];
        int i;
};

union Humidity {
        byte b[2];
        int i;
};

WiFiClient wifiClient;
HTU21D myHumidity;

String macToStr(const uint8_t* mac)
{
        String result;
        for (int i = 0; i < 6; ++i) {
                result += String(mac[i], 16);
                if (i < 5)
                        result += ':';
        }
        return result;
}

void gettemperature() {
        myHumidity.begin();
        //myHumidity.setResolution(0b10000001);
        t_measure = myHumidity.readTemperature();
        h_measure = myHumidity.readHumidity();
        temp = (t_measure + 0.5);
        humidity = (h_measure + 0.5);
}

String getTime() {
        WiFiClient wClient;
        while (!!!wClient.connect("time.is", 80)) {
        }

        wClient.print("HEAD / HTTP/1.1\r\n\r\n");

        while (!!!wClient.available()) {
                yield();
        }

        while (wClient.available()) {
                if (wClient.read() == '\n') {
                        if (wClient.read() == 'D') {
                                if (wClient.read() == 'a') {
                                        if (wClient.read() == 't') {
                                                if (wClient.read() == 'e') {
                                                        if (wClient.read() == ':') {
                                                                wClient.read();
                                                                String theDate = wClient.readStringUntil('\r');
                                                                wClient.stop();
                                                                return theDate;
                                                        }
                                                }
                                        }
                                }
                        }
                }
        }
}

PubSubClient client(server, 1883, wifiClient);

void wifiConnect() {
        WiFi.mode(WIFI_STA);
        WiFi.config(ip, gateway, subnet, dns1);
        WiFi.begin (ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
                delay(500);
        }
        rssi = WiFi.RSSI();
}

void write2eeprom() {
        union Humidity myHumidity;
        union Temp myTemperature;
        //union dateTime myDateTime;
        myHumidity.i = humidity;
        myTemperature.i = temp;
        EEPROM.write(1, myTemperature.b[0]);
        EEPROM.write(2, myTemperature.b[1]);
        EEPROM.write(3, myHumidity.b[0]);
        EEPROM.write(4, myHumidity.b[1]);
        EEPROM.commit();
        delay(50);
        Serial.println("Wrote to EEPROM:");
        Serial.print ("Temp_EEPROM= ");
        Serial.println (myTemperature.i);
        Serial.print ("Hum_EEPROM= ");
        Serial.println (myHumidity.i);
}

byte getWiFiFlag(){
        return EEPROM.read(address_flag);
}
void setWiFiFlag(byte flag){
        EEPROM.write(address_flag, flag);
        EEPROM.commit();
        delay(50);
}

byte getCount(){
        return EEPROM.read(address_count);
}
void setCount (byte count){
        EEPROM.write(address_count, count);
        EEPROM.commit();
        delay(50);
}

void setup() {
        union Humidity lastHumidity;
        union Temp lastTemperature;
        //union dateTime myDateTime;

        EEPROM.begin(512);
        //Serial.begin(9600);
        flag = getWiFiFlag();
        delay(50);
        count = getCount();
        delay (50);

        if (flag == 1 || count > 9) {
                // Connect to WiFi network
                wifiConnect();

                String clientName;
                clientName += "esp8266-";
                uint8_t mac[6];
                WiFi.macAddress(mac);
                clientName += macToStr(mac);

                gettemperature();

                write2eeprom();

                String nettime = getTime();

                float voltage = readvdd33() / 1000.0;
                String payload = "{\"time\":\"";
                payload += nettime;
                payload += "\",\"temperature\":";
                payload += temp;
                payload += ",\"humidity\":";
                payload += humidity;
                payload += ",\"voltage\":";
                payload += voltage;
                payload += ",\"signal\":";
                payload += rssi;
                payload += "}";

                if (client.connect((char*) clientName.c_str(), user, pass, lwt_topic, 1, 0,lwt_message)) {

                        if (client.publish(topic, (char*) payload.c_str(), true)) {
                                //Serial.println("Publish ok");
                                setWiFiFlag(0); // reset flag for mqtt
                                setCount(0);
                        }
                        else {
                                //Serial.println("Publish failed");
                                setWiFiFlag(1);
                                EEPROM.end();
                                ESP.deepSleep(1000000, WAKE_RF_DEFAULT);
                                delay(100);
                        }
                }
        }
        else {
                lastTemperature.b[0] = EEPROM.read(1);
                lastTemperature.b[1] = EEPROM.read(2);
                lastHumidity.b[0] = EEPROM.read(3);
                lastHumidity.b[1] = EEPROM.read(4);
                delay(50);

                gettemperature();

                if ((lastTemperature.i != temp) || (lastHumidity.i != humidity)) {
                        setWiFiFlag(1);
                        EEPROM.end();
                        ESP.deepSleep(1000000, WAKE_RF_DEFAULT);
                        delay(100);
                }
                count++;
                setCount(count);
        }

        EEPROM.end();
        ESP.deepSleep(5 * 60 * 1000000, WAKE_RF_DISABLED); // Sleep for 5*60 seconds
        delay(100);
}

void loop()  {
}
