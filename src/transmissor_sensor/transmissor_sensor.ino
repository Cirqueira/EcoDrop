#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include "arduino_secrets.h"

#define DHTPIN 26
#define DHTTYPE DHT11
#define SOIL_PIN 34

// --- Calibração do sensor de umidade do solo ---
static const int limiteSeco = 4095;
static const int limiteMolhado = 2700;

DHT dht(DHTPIN, DHTTYPE);
unsigned long sendDataPrevMillis = 0;

bool modoManual = false;
bool comandoManual = false;

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

void setup() {
  Serial.begin(115200);
  delay(2000);

  dht.begin();
  pinMode(SOIL_PIN, INPUT);

  // 1. Conexão Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  tlsClient.setInsecure();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWi-Fi Conectado com sucesso!");
}

// Busca o modo de operacao e o comando manual para registrar o status correto no historico
void buscarConfigAtuador() {
  if (WiFi.status() != WL_CONNECTED) return;

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

// Grava cada leitura em um no de historico (acumula registros com timestamp do servidor)
void gravarHistorico(int umidadePercentual, int valorLido, float tempFinal, bool bombaLigada) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" -> Historico nao gravado: Wi-Fi desconectado.");
    return;
  }

  String url = String(DATABASE_URL) + "/dispositivos/ESP32_Solo_01/historico.json";

  if (!iniciarHttp(url)) {
    Serial.println(" -> Falha ao iniciar conexao com o historico.");
    return;
  }

  http.addHeader("Content-Type", "application/json");

  String payload = "{";
  payload += "\"temperatura\":" + String(tempFinal, 1) + ",";
  payload += "\"umidade_solo\":" + String(umidadePercentual) + ",";
  payload += "\"solo_bruto\":" + String(valorLido) + ",";
  payload += "\"bomba\":" + String(bombaLigada ? "true" : "false") + ",";
  payload += "\"modo\":";
  payload += modoManual ? "\"MANUAL\"," : "\"AUTOMATICO\",";
  payload += "\"timestamp\":{\".sv\":\"timestamp\"}";
  payload += "}";

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == HTTP_CODE_CREATED) {
    Serial.println(" -> Historico gravado no Firebase.");
  } else {
    Serial.printf(" -> Erro ao gravar historico: HTTP %d\n", httpResponseCode);
  }

  http.end();
}

void loop() {
  garantirWiFi();

  // Envia a cada 10 segundos
  if (millis() - sendDataPrevMillis > 10000 || sendDataPrevMillis == 0) {
    sendDataPrevMillis = millis();

    // Média para estabilizar leitura do solo (valor bruto ADC)
    long soma = 0;
    for (int i = 0; i < 10; i++) {
      soma += analogRead(SOIL_PIN);
      delay(10);
    }
    int valorLido = (int)(soma / 10);

    // Converte para percentual
    int umidadePercentual = map(valorLido, limiteSeco, limiteMolhado, 0, 100);
    umidadePercentual = constrain(umidadePercentual, 0, 100);

    // Leitura do DHT11
    float t = dht.readTemperature();
    bool leituraValida = !isnan(t);
    float tempFinal = leituraValida ? t : 0.0;

    Serial.println("Lendo sensores e preparando envio..:");
    Serial.printf(" -> Solo(bruto): %d | Umidade: %d%% | Temp: %.1f\n", valorLido, umidadePercentual, tempFinal);

    // Verifica se o Wi-Fi está conectado antes de enviar
    if (WiFi.status() == WL_CONNECTED) {
      buscarConfigAtuador();

      // No modo MANUAL o status segue o interruptor; no AUTOMATICO, segue o sensor
      bool bombaAutomatica = (umidadePercentual <= 55.0f);
      bool bombaLigada = modoManual ? comandoManual : bombaAutomatica;

      String url = String(DATABASE_URL) + "/dispositivos/ESP32_Solo_01/estado_atual.json";

      if (iniciarHttp(url)) {
        http.addHeader("Content-Type", "application/json");

        String payload = "{";
        payload += "\"temperatura\":" + String(tempFinal, 1) + ",";
        payload += "\"umidade_solo\":" + String(umidadePercentual) + ",";
        payload += "\"solo_bruto\":" + String(valorLido) + ",";
        payload += "\"bomba\":" + String(bombaLigada ? "true" : "false") + ",";
        payload += "\"timestamp\":" + String(millis());
        payload += "}";

        int httpResponseCode = http.PUT(payload);

        if (httpResponseCode > 0) {
          if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == HTTP_CODE_CREATED) {
            Serial.println(" -> Status Envio: Ok (Firebase atualizado com sucesso!)");
          } else {
            Serial.printf(" -> Status Envio: Erro HTTP %d\n", httpResponseCode);
          }
        } else {
          Serial.printf(" -> Erro na requisicao: %s\n", http.errorToString(httpResponseCode).c_str());
        }
        http.end();

        if (leituraValida) {
          gravarHistorico(umidadePercentual, valorLido, tempFinal, bombaLigada);
        } else {
          Serial.println(" -> Historico nao gravado: leitura invalida do sensor (DHT).");
        }
      }
    } else {
      Serial.println(" -> Erro: Wi-Fi desconectado!");
    }
    Serial.println();
  }

  delay(20);
}