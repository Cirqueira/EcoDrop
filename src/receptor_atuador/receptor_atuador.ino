#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "arduino_secrets.h" // Contém WIFI_SSID, WIFI_PASSWORD e DATABASE_URL

#define RELE_PIN 5

// OLED (SSD1306 I2C)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_I2C_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Estrutura de dados recebidos da nuvem
struct struct_message {
    float temp;
    float umid;
    int solo;
};

struct_message readings = {0.0, 0.0, 0};

bool modoManual = false;
bool comandoManual = false;

unsigned long ultimaLeituraNuvemMillis = 0;
const long intervaloConsulta = 5000; // Consulta o Firebase a cada 5 segundos

// Cliente TLS e HTTP reutilizados entre requisicoes (evita multiplos handshakes)
WiFiClientSecure tlsClient;
HTTPClient http;
unsigned long ultimaTentativaWifi = 0;

void garantirWiFi() {
    if (WiFi.status() == WL_CONNECTED) return;

    unsigned long agora = millis();
    if (ultimaTentativaWifi != 0 && agora - ultimaTentativaWifi < 10000) return;
    ultimaTentativaWifi = agora;

    Serial.println("Wi-Fi desconectado. Tentando reconectar...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool iniciarHttp(const String& url) {
    http.setReuse(true);
    http.setConnectTimeout(5000);
    http.setTimeout(8000);
    tlsClient.setInsecure();
    tlsClient.setHandshakeTimeout(8);
    return http.begin(tlsClient, url);
}

static void desenharOLED() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("EcoDrop");

    display.setTextSize(1);
    display.setCursor(0, 20);
    display.print("WiFi: Conectado");

    display.setCursor(0, 36);
    display.print("Umid: ");
    display.print(readings.umid, 0);
    display.print("% Temp: ");
    display.print(readings.temp, 1);
    display.println("C");

    display.display();
}

void processarBomba() {
    // No modo MANUAL o interruptor tem prioridade total sobre os sensores
    if (modoManual) {
        if (comandoManual) {
            digitalWrite(RELE_PIN, HIGH);
            Serial.println(" -> Bomba Comando: LIGAR (Manual)");
        } else {
            digitalWrite(RELE_PIN, LOW);
            Serial.println(" -> Bomba Comando: DESLIGAR (Manual)");
        }
        return;
    }

    Serial.printf(" -> Avaliando umidade para bomba: %.0f%%\n", readings.umid);

    // Se a umidade estiver <= 55% (Solo seco -> Liga a bomba)
    if (readings.umid <= 55.0f) {
        digitalWrite(RELE_PIN, HIGH); 
        Serial.println(" -> Bomba Comando: LIGAR (Automatico)");
    } 
    // Se a umidade estiver > 65% (Solo molhado -> Desliga a bomba)
    else if (readings.umid > 65.0f) {
        digitalWrite(RELE_PIN, LOW);  
        Serial.println(" -> Bomba Comando: DESLIGAR (Automatico)");
    }
}

// Busca somente as leituras dos sensores
void buscarLeituras() {
    String url = String(DATABASE_URL) + "/dispositivos/ESP32_Solo_01/estado_atual.json";

    if (iniciarHttp(url)) {
        int httpResponseCode = http.GET();

        if (httpResponseCode == HTTP_CODE_OK) {
            String payload = http.getString();

            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                readings.temp = doc["temperatura"] | 0.0;
                readings.umid = doc["umidade_solo"] | 0.0;
                readings.solo = doc["solo_bruto"] | 0;

                Serial.println("Dados sincronizados do Firebase..:");
                Serial.printf(" -> Solo(bruto): %d | Umidade: %.0f%% | Temp: %.1f\n",
                              readings.solo, readings.umid, readings.temp);
            } else {
                Serial.println("Erro ao interpretar JSON do Firebase.");
            }
        } else {
            Serial.printf("Erro HTTP ao buscar dados: %d\n", httpResponseCode);
        }
        http.end();
    } else {
        Serial.println("Falha ao iniciar conexão HTTPS.");
    }
}

// Busca o modo de operacao e o comando manual do interruptor
void buscarConfigAtuador() {
    String url = String(DATABASE_URL) + "/atuadores/bomba_dagua.json";

    if (iniciarHttp(url)) {
        int httpResponseCode = http.GET();

        if (httpResponseCode == HTTP_CODE_OK) {
            String payload = http.getString();

            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                const char* modo = doc["modo"] | "AUTOMATICO";
                modoManual = (String(modo) == "MANUAL");
                comandoManual = doc["comando_manual"] | false;
            }
        }
        http.end();
    }
}

// Busca os dados atualizados do EcoDrop (Firebase)
void buscarDadosFirebase() {
    garantirWiFi();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Erro: Wi-Fi desconectado. Reconectando...");
        return;
    }

    buscarLeituras();
    buscarConfigAtuador();
    processarBomba();
    desenharOLED();
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    pinMode(RELE_PIN, OUTPUT);
    digitalWrite(RELE_PIN, LOW); // Inicia desligado

    // OLED: inicializa I2C explicitamente informando pinos padrão do ESP32 (SDA=21, SCL=22)
    Wire.begin(21, 22); 
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        Serial.println("Erro: OLED nao encontrado (0x3C).");
    } else {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("Iniciando Receptor...");
        display.display();
    }

    // CONFIGURAÇÃO DE WI-FI
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    tlsClient.setInsecure();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    Serial.print("Conectando ao Wi-Fi principal");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nWi-Fi Conectado com sucesso!");

    desenharOLED();
}

void loop() {
    // Consulta o Firebase periodicamente a cada 5 segundos
    if (millis() - ultimaLeituraNuvemMillis >= intervaloConsulta || ultimaLeituraNuvemMillis == 0) {
        ultimaLeituraNuvemMillis = millis();
        buscarDadosFirebase();
    }

    delay(20);
}