///////////////////
///    SALA 2   ///
///    LDR 2    ///
///////////////////
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ====================================================================
// CREDENCIAIS EXCLUSIVAS DO SEU USUÁRIO LDR (NÓ DE LUMINOSIDADE)
// ====================================================================
const char* mqtt_server = "9c2c5a2ff2444b98b9f7fff2e9aa1180.s1.eu.hivemq.cloud"; 
const int mqtt_port = 8883; 
const char* mqtt_user = "ldr_user"; 
const char* mqtt_pass = "Agaci123"; 
// ====================================================================

const char* TOPICO_LUMINOSIDADE = "/sensores/luminosidade/sala2/zona2/ldr2";
const int PINO_LDR = 36;

WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long ultimoEnvio = 0;
const long intervalo = 2000; 

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
    Serial.print("[MQTT] Tentando conectar au Cluster com credenciais do LDR...");
    String clientId = "ESP32_LDR_Sala1_" + String(random(0xffff), HEX);
    
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
  analogSetAttenuation(ADC_11db); 
  
  setup_wifi();
  espClient.setInsecure(); 
  client.setBufferSize(512); 
  client.setServer(mqtt_server, mqtt_port);
  
  Serial.println("Nó LDR iniciado e pronto para leitura.");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); 

  unsigned long atual = millis();
  
  if (atual - ultimoEnvio >= intervalo) {
    ultimoEnvio = atual;

    // Leitura analógica bruta e inversão para porcentagem
    int leituraBruta = analogRead(PINO_LDR);
    int porcentagemLuz = map(leituraBruta, 4095, 0, 0, 100);
    porcentagemLuz = constrain(porcentagemLuz, 0, 100); 

    String payload = String(porcentagemLuz);
    
    // Preparação dos dados com a flag RETAIN ativa (true)
    const uint8_t* payloadBytes = (const uint8_t*)payload.c_str();
    bool sucesso = client.publish(TOPICO_LUMINOSIDADE, payloadBytes, payload.length(), true);

    Serial.print("[LDR Node] ");
    if (sucesso) {
      Serial.printf("Transmissão concluída (Retained) -> Luminosidade: %s%%\n", payload.c_str());
    } else {
      Serial.println("Falha crítica no envio do pacote MQTT.");
    }
  }
}