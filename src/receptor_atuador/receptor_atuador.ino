#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "View.h" 

#define RELE_PIN 5

// OLED (SSD1306 I2C)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_I2C_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

typedef struct struct_message {
    float temp;
    float umid;
    int solo;
} struct_message;

struct_message readings;
AsyncWebServer server(80);

// NOVA VARIÁVEL: Controla se o automatismo está ligado ou desligado
bool rotinaAtiva = true;
bool estadoBombaManual = false; // Guarda o estado manual desejado pelo usuário

static void desenharOLED() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("UmiTechIOT");
    //display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 20);
    display.print("WiFi: ");
    display.println("UmiTechIOT");

    display.setCursor(0, 36);
    display.print("IP: ");
    display.println(WiFi.softAPIP());

    display.display();
}

void processarBomba() {
    // Se o usuário bloqueou a rotina pelo Browser, força a bomba a ficar desligada e ignora o sensor
    if (!rotinaAtiva) {
        // digitalWrite(RELE_PIN, LOW); // Garante que fica DESLIGADA fisicamente
        digitalWrite(RELE_PIN, estadoBombaManual ? HIGH : LOW);
        return;
    }

    // Lógica Automática Normal
    // Invertendo os comandos para sincronizar com o comportamento físico do seu relé
    if (readings.umid < 55.0f) {
        digitalWrite(RELE_PIN, HIGH); // Liga o relé fisicamente
        estadoBombaManual = true;     // Sincroniza a variável manual
        Serial.println(" -> Bomba Comando: Ligar (Automatico)");
    } 
    else if (readings.umid > 65.0f) {
        digitalWrite(RELE_PIN, LOW);  // Desliga o relé fisicamente
        estadoBombaManual = false;    // Sincroniza a variável manual
        Serial.println(" -> Bomba Comando: Desligar (Automatico)");
    }
}

// Callback de Recebimento
void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *data, int len) {
    memcpy(&readings, data, sizeof(readings));
    
    Serial.println("Recebido..:");
    Serial.printf(" -> Solo(bruto): %d | Umidade: %.0f%% | Temp: %.1f\n",
                  readings.solo, readings.umid, readings.temp);
    // Agora, em vez de apenas um ponto, vamos ver os dados:
    // Formatação idêntica à do transmissor usando printf
    // %.0f remove as casas decimais da umidade (ex: 100 em vez de 100.00)
    // Serial.printf("Recebido -> Solo(bruto): %d | Umidade: %.0f%% | Temp: %.1f\n",
    //               readings.solo, readings.umid, readings.temp);

    // Serial.print("Recebido -> Solo Bruto: "); Serial.print(readings.solo);
    // Serial.print(" | Umid: "); Serial.print(readings.umid);
    // Serial.print(" | Temp: "); Serial.print(readings.temp);
    // Serial.println();

    if(!rotinaAtiva) {
        Serial.println(" -> Rotina Automatica: [BLOQUEADA PELO USUARIO]");
        Serial.printf(" -> Rotina Automatica: [BLOQUEADA] | Comando Manual: [%s]\n", estadoBombaManual ? "LIGADO" : "DESLIGADO");
    }

    processarBomba();
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(500);
	
	pinMode(RELE_PIN, OUTPUT);
    digitalWrite(RELE_PIN, HIGH); // Inicia desligado (lógica inversa de relés)

    // OLED: inicializa I2C e o display
    Wire.begin(); // padrão ESP32: SDA=21, SCL=22
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        Serial.println("Erro: OLED nao encontrado (0x3C).");
    } else {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("Iniciando...");
        display.display();
    }

    // CONFIGURAÇÃO DE WI-FI ROBUSTA
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("UmiTechIOT", "123@456#"); 
    
    // Forçar o canal 1 (evita que o rádio fique pulando de frequência)
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    Serial.println("\nRede: UmiTechIOT | IP: 192.168.4.1");

    // Desenha rede/IP no OLED (tela fica estática)
    if (display.width() > 0) {
        desenharOLED();
    }

    // ESP-NOW (Comunicacao entre Esp's)
    if (esp_now_init() != ESP_OK) {
        Serial.println("Erro ESP-NOW"); 
        return;
    }

    esp_now_register_recv_cb(OnDataRecv);

    // Rota Principal
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html);
        response->addHeader("Cache-Control", "no-cache");
        request->send(response);
    });

    // Rota da Automação: Ativa/Desativa o automatismo
    server.on("/toggleRotina", HTTP_GET, [](AsyncWebServerRequest *request){
        rotinaAtiva = !rotinaAtiva; // Inverte o estado atual
        if(!rotinaAtiva) {
            // Ao desligar a automação, por segurança, desliga a bomba
            digitalWrite(RELE_PIN, LOW); 
            estadoBombaManual = false;
        }
        request->send(200, "text/plain", rotinaAtiva ? "Ativa" : "Bloqueada");
        Serial.printf(">>> Modificacao Manual: Rotina Automatica agora esta [%s]\n", rotinaAtiva ? "ATIVA" : "BLOQUEADA");
    });

    // NOVA ROTA: Liga/Desliga a bomba manualmente pelo botão no browser
    server.on("/toggleBomba", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!rotinaAtiva) { // Só aceita o comando se a automação estiver inoperante
            estadoBombaManual = !estadoBombaManual;
            digitalWrite(RELE_PIN, estadoBombaManual ? HIGH : LOW);
            request->send(200, "text/plain", "OK");
            Serial.printf(">>> Comando Manual: Bomba alterada para [%s]\n", estadoBombaManual ? "LIGADA" : "DESLIGADA");
        } else {
            request->send(400, "text/plain", "Bloqueado pelo modo automatico");
        }
    });

    // Rota de Dados JSON para o Navegador
    server.on("/dados", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        StaticJsonDocument<192> doc; // Aumentado ligeiramente para incluir o novo booleano
        doc["atual"]["temp"] = readings.temp;
        doc["atual"]["umid"] = readings.umid; 
        
        // Ajustado para refletir a nova lógica (HIGH agora significa bomba ligada)
        doc["atual"]["bomba"] = (digitalRead(RELE_PIN) == HIGH); 

        doc["atual"]["rotina"] = rotinaAtiva; // Envia se a automação está ativa ou não
        
        serializeJson(doc, *response);
        request->send(response);
    });

    server.begin();
    Serial.println("Servidor Web ONLINE.");
}

void loop() {
    // Dá um tempo para as tarefas de fundo (WiFi/TCP) processarem
    delay(1);
}