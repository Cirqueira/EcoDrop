#include <esp_now.h>
#include <WiFi.h>
#include <DHT.h>

#define DHTPIN 26
#define DHTTYPE DHT11
#define SOIL_PIN 34

uint8_t broadcastAddress[] = {0x38, 0x18, 0x2B, 0xE7, 0x3A, 0xC0};

// --- Calibração do sensor de umidade do solo (ADC 0..4095) ---
// Ajuste conforme seus valores observados:
static const int limiteSeco = 4095;
static const int limiteMolhado = 2700;

typedef struct struct_message {
    float temp;
    float umid;
    int solo;
} struct_message;

struct_message myData;
DHT dht(DHTPIN, DHTTYPE);

void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? " -> Status Envio: Ok" : " -> Status Envio: Erro");
  Serial.println(); 
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  dht.begin();
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_send_cb(OnDataSent);
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  esp_now_add_peer(&peerInfo);
}

void loop() {
  // Média para estabilizar leitura do solo (valor bruto ADC)
  long soma = 0;
  for(int i=0; i<10; i++) { soma += analogRead(SOIL_PIN); delay(10); }
  const int valorLido = (int)(soma / 10);

  // Converte para percentual:
  // limiteSeco -> 0%
  // limiteMolhado -> 100%
  int umidadePercentual = map(valorLido, limiteSeco, limiteMolhado, 0, 100);
  umidadePercentual = constrain(umidadePercentual, 0, 100);

  // Mantém compatibilidade com o receptor atual:
  // - `solo` continua sendo o valor bruto (usado na lógica do relé)
  // - `umid` passa a carregar o percentual calculado
  myData.solo = valorLido;
  myData.umid = (float)umidadePercentual;

  float t = dht.readTemperature();
  myData.temp = isnan(t) ? 0.0 : t;

  // Serial.printf("Enviando -> Solo(bruto): %d | Umidade: %d%% | Temp: %.1f\n",
  //               myData.solo, umidadePercentual, myData.temp);
  Serial.println("Enviando..:");
  Serial.printf(" -> Solo(bruto): %d | Umidade: %d%% | Temp: %.1f\n",
                myData.solo, umidadePercentual, myData.temp);

  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  delay(10000);
}