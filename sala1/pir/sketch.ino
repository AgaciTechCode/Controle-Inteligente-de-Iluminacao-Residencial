///////////////////
///    SALA 1   ///
///    PIR 1    ///
///////////////////
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// Configurações de rede para o simulador Wokwi
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ====================================================================
// CREDENCIAIS EXCLUSIVAS DO SEU USUÁRIO PIR (NÓ DE PRESENÇA)
// ====================================================================
const char* mqtt_server = "9c2c5a2ff2444b98b9f7fff2e9aa1180.s1.eu.hivemq.cloud"; 
const int mqtt_port = 8883; 
const char* mqtt_user = "pir_user"; // Seu usuário do PIR
const char* mqtt_pass = "Agaci123";   // Sua senha do PIR
// ====================================================================

// Tópico definido por você
const char* TOPICO_PRESENCA = "/sensores/presenca/sala1/zona1/pir1";

// Pino físico conforme o diagram.json
const int PINO_PIR = 13;

WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long ultimoEnvio = 0;
const long intervalo = 2000; // Intervalo de checagem/envio (2 segundos)

void setup_wifi() {
  delay(10);
  Serial.print("\n[Wi-Fi] Conectando-se a rede: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n[Wi-Fi] Conectado com sucesso!");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("[MQTT] Tentando conectar ao Cluster com credenciais do PIR...");
    
    // Identificador único para este nó na rede do broker
    String clientId = "ESP32_PIR_Sala1_" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println(" Conectado com segurança!");
    } else {
      Serial.print(" Falhou, erro de conexão=");
      Serial.print(client.state());
      Serial.println(" Nova tentativa em 5 segundos.");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Configura o pino de sinal do PIR como entrada digital
  pinMode(PINO_PIR, INPUT);
  
  setup_wifi();

  // Permite conexões TLS estáveis dentro do ambiente de simulação do Wokwi
  espClient.setInsecure(); 

  client.setBufferSize(512);

  client.setServer(mqtt_server, mqtt_port);
  
  Serial.println("Aguardando 2s para estabilização do hardware PIR...");
  delay(2000); 
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); 

  unsigned long atual = millis();
  
  if (atual - ultimoEnvio >= intervalo) {
    ultimoEnvio = atual;

    // Realiza a leitura digital discreta do sensor
    int movimento = digitalRead(PINO_PIR);
    String payload = String(movimento);

    // Preparação dos dados com a flag RETAIN ativa (true)
    const uint8_t* payloadBytes = (const uint8_t*)payload.c_str();
    bool sucesso = client.publish(TOPICO_PRESENCA, payloadBytes, payload.length(), true);

    Serial.print("[PIR Node] ");
    if (sucesso) {
      Serial.printf("Transmissão concluída (Retained) -> Status de Movimento: %s\n", payload.c_str());
    } else {
      Serial.println("Falha crítica no envio do pacote MQTT.");
    }
  }
}