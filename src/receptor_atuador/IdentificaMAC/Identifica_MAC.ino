#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000); // Dá um tempo para o Serial estabilizar

  // Inicia o Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(); 

  Serial.println("\n--- IDENTIFICADOR DE MAC ADDRESS ---");
  
  // Tenta ler o MAC até que ele não seja zero
  while (WiFi.macAddress() == "00:00:00:00:00:00") {
    Serial.println("Aguardando inicialização do hardware Wi-Fi...");
    delay(500);
  }

  Serial.print("O endereco MAC deste ESP32 e: ");
  Serial.println(WiFi.macAddress());
  Serial.println("------------------------------------");
}

void loop() {}