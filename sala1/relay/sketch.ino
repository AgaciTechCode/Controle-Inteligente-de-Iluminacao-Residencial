///////////////////
///    SALA 1   ///
///   RELAY 1   ///
///////////////////
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* mqtt_server = "9c2c5a2ff2444b98b9f7fff2e9aa1180.s1.eu.hivemq.cloud"; 
const int mqtt_port = 8883; 
const char* mqtt_user = "pir_user"; 
const char* mqtt_pass = "Agaci123";

// Tópicos base para a Sala 1
const char* TOPICO_PRESENCA = "/sensores/presenca/sala1/zona1/+";
const char* TOPICO_LUMINOSIDADE = "/sensores/luminosidade/sala1/zona1/+";
const char* TOPICO_MODO = "/servicos/atuador/sala1/zona1/modo";
const char* TOPICO_COMANDO = "/servicos/atuador/sala1/zona1/comando";
const char* TOPICO_ESTADO_LAMP = "/servicos/atuador/sala1/zona1/estado";

const int PINO_RELE = 12;

// Armazenamento de Estados
int ultimo_movimento = 0;
int ultima_luminosidade = 100;
String modo_operacao = "AUTOMATICO"; // Modos: AUTOMATICO, ECONOMICO, MANUAL
int comando_manual = 0; 
int estado_atual_rele = -1; // -1 força a primeira atualização

WiFiClientSecure espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  Serial.println("\n[Wi-Fi] Conectado!");
}

void processarEstrategiaAtuacao() {
  int novo_estado_rele = 0;

  if (modo_operacao == "MANUAL") {
    novo_estado_rele = comando_manual;
  } 
  else if (modo_operacao == "AUTOMATICO") {
    // Inclui perfeitamente o valor limiar de 30%
    if (ultimo_movimento == 1 && ultima_luminosidade <= 30) {
      novo_estado_rele = 1;
    }
  } 
  else if (modo_operacao == "ECONOMICO") {
    if (ultimo_movimento == 1 && ultima_luminosidade <= 30) {
      novo_estado_rele = 1;
    }
  }

  if (novo_estado_rele != estado_atual_rele) {
    estado_atual_rele = novo_estado_rele;
    digitalWrite(PINO_RELE, estado_atual_rele);
    
    // Envia o estado físico real para sincronizar com a dashboard
    String statusStr = String(estado_atual_rele);
    client.publish(TOPICO_ESTADO_LAMP, statusStr.c_str(), true);
    Serial.printf("[Atuador] Lâmpada alterada para: %s no modo [%s]\n", (estado_atual_rele == 1) ? "LIGADA" : "DESLIGADA", modo_operacao.c_str());
  }
}

// POSICIONAMENTO CORRIGIDO: O callback agora é declarado antes do setup()
void callback(char* topic, byte* payload, unsigned int length) {
  String mensagem = "";
  for (int i = 0; i < length; i++) { 
    mensagem += (char)payload[i]; 
  }

  if (String(topic) == TOPICO_PRESENCA) {
    ultimo_movimento = mensagem.toInt();
  } 
  else if (String(topic) == TOPICO_LUMINOSIDADE) {
    ultima_luminosidade = mensagem.toInt();
  }
  else if (String(topic) == TOPICO_MODO) {
    modo_operacao = mensagem;
    Serial.printf("[Modo] Mudança de estratégia recebida: %s\n", modo_operacao.c_str());
  }
  else if (String(topic) == TOPICO_COMANDO) {
    comando_manual = mensagem.toInt();
    Serial.printf("[Manual] Comando direto recebido: %d\n", comando_manual);
  }

  processarEstrategiaAtuacao();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("[MQTT] Conectando Atuador...");
    String clientId = "ESP32_Atuador_Sala1_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println(" Pronto!");
      
      // Inscrição na malha de tópicos
      client.subscribe(TOPICO_PRESENCA);
      client.subscribe(TOPICO_LUMINOSIDADE);
      client.subscribe(TOPICO_MODO);
      client.subscribe(TOPICO_COMANDO);
      
      Serial.println("[MQTT] Inscrições validadas com sucesso.");
    } else {
      Serial.printf(" Falhou, rc=%d. Tentando em 5s.\n", client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PINO_RELE, OUTPUT);
  setup_wifi();
  
  espClient.setInsecure(); 
  client.setBufferSize(512); 
  client.setServer(mqtt_server, mqtt_port);
  
  // O compilador agora encontra a função sem problemas
  client.setCallback(callback); 
}

void loop() {
  if (!client.connected()) { 
    reconnect(); 
  }
  client.loop(); 
}
