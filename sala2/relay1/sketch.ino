///////////////////////////////////////
///         PLACA: ZONA 1           ///
///   Controla Rele 1, PIR 1, LDR 1  ///
///////////////////////////////////////
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* mqtt_server = "9c2c5a2ff2444b98b9f7fff2e9aa1180.s1.eu.hivemq.cloud"; 
const int mqtt_port = 8883; 
const char* mqtt_user = "pir_user"; 
const char* mqtt_pass = "Agaci123";

// --- TÓPICOS MQTT PARA A ZONA 1 ---
// Escuta a sala de forma ampla para capturar dados das outras zonas
const char* TOPICO_SUB_SALA2      = "/sensores/+/sala2/+/+";

const char* TOPICO_Z1_MODO        = "/servicos/atuador/sala2/zona1/modo";
const char* TOPICO_Z1_COMANDO     = "/servicos/atuador/sala2/zona1/comando";
const char* TOPICO_Z1_ESTADO      = "/servicos/atuador/sala2/zona1/estado";

// Canais de saída locais
const char* TOPICO_PUB_PIR1       = "/sensores/presenca/sala2/zona1/pir1";
const char* TOPICO_PUB_LDR1       = "/sensores/luminosidade/sala2/zona1/ldr1";

// Hardware local
const int PINO_RELE = 12;
const int PINO_PIR  = 13; 
const int PINO_LDR  = 34; 

// Estados Locais (Zona 1)
int z1_movimento = 0;
int z1_luminosidade = 100;
String z1_modo_operacao = "AUTOMATICO";
int z1_comando_manual = 0;
int z1_estado_rele = -1;

// Estados Remotos (Zona 2)
int z2_movimento = 0;
int z2_luminosidade = 100;

unsigned long ultimo_envio_sensor = 0;

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

  if (z1_modo_operacao == "MANUAL") {
    novo_estado_rele = z1_comando_manual;
  } 
  else if (z1_modo_operacao == "ECONOMICO") {
    // Modo Isolado: Só reage aos critérios do sensor local
    if (z1_movimento == 1 && z1_luminosidade <= 15) {
      novo_estado_rele = 1;
    }
  } 
  else if (z1_modo_operacao == "AUTOMATICO") {
    // Modo Compartilhado: Qualquer zona que acionar os gatilhos, acende esta lâmpada
    bool condicao_z1 = (z1_movimento == 1 && z1_luminosidade <= 30);
    bool condicao_z2 = (z2_movimento == 1 && z2_luminosidade <= 30);
    
    if (condicao_z1 || condicao_z2) {
      novo_estado_rele = 1;
    }
  }

  if (novo_estado_rele != z1_estado_rele) {
    z1_estado_rele = novo_estado_rele;
    digitalWrite(PINO_RELE, z1_estado_rele);
    client.publish(TOPICO_Z1_ESTADO, String(z1_estado_rele).c_str(), true);
    Serial.printf("[Relé 1] Estado Modificado: %s | Modo: %s\n", (z1_estado_rele == 1) ? "LIGADO" : "DESLIGADO", z1_modo_operacao.c_str());
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String topicoStr = String(topic);
  
  // CORREÇÃO CRÍTICA: Se a mensagem de sensor veio da própria zona 1, ignora 
  // para evitar estouro de buffer e loop de feedback circular
  if (topicoStr.indexOf("/zona1/") > 0) {
    if (topicoStr.indexOf("/presenca/") > 0 || topicoStr.indexOf("/luminosidade/") > 0) {
      return; 
    }
  }

  String mensagem = "";
  for (int i = 0; i < length; i++) { mensagem += (char)payload[i]; }

  // Filtra dados remotos da Zona 2
  if (topicoStr.indexOf("/zona2/") > 0) {
    if (topicoStr.indexOf("/presenca/") > 0)         z2_movimento = mensagem.toInt();
    else if (topicoStr.indexOf("/luminosidade/") > 0) z2_luminosidade = mensagem.toInt();
    Serial.printf("[Rede] Atualização da Zona 2 recebida -> PIR: %d, LDR: %d%%\n", z2_movimento, z2_luminosidade);
  }
  // Filtra comandos vindos da Dashboard
  else if (topicoStr == TOPICO_Z1_MODO) {
    z1_modo_operacao = mensagem;
    Serial.printf("[Painel Web] Alteração de Modo: %s\n", z1_modo_operacao.c_str());
  } 
  else if (topicoStr == TOPICO_Z1_COMANDO) {
    z1_comando_manual = mensagem.toInt();
    Serial.printf("[Painel Web] Comando Manual Recebido: %d\n", z1_comando_manual);
  }

  processarEstrategiaAtuacao();
}

void lerEEnviarSensoresLocais() {
  z1_movimento = digitalRead(PINO_PIR);
  
  int leitura_analogica = analogRead(PINO_LDR);
  z1_luminosidade = map(leitura_analogica, 0, 4095, 0, 100); 

  // Publica dados locais para o Broker atualizar a Web e o ESP da Zona 2
  client.publish(TOPICO_PUB_PIR1, String(z1_movimento).c_str(), true);
  client.publish(TOPICO_PUB_LDR1, String(z1_luminosidade).c_str(), true);
  
  Serial.printf("[Local] Telemetria enviada -> PIR1: %d | LDR1: %d%%\n", z1_movimento, z1_luminosidade);

  processarEstrategiaAtuacao();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("[MQTT] Conectando Placa Zona 1...");
    // CORREÇÃO CRÍTICA: ID único baseado no ruído analógico gerado no setup()
    String clientId = "ESP32_Z1_Sala2_" + String(random(0xffff), HEX);
    
    // Conecta passando a flag 'true' para Clean Session
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass, 0, 0, 0, 0, true)) {
      Serial.println(" Conectado com sucesso!");
      
      client.subscribe(TOPICO_SUB_SALA2);
      client.subscribe(TOPICO_Z1_MODO);
      client.subscribe(TOPICO_Z1_COMANDO);
      
      // Sincroniza o estado atual logo após reestabelecer rede
      client.publish(TOPICO_Z1_ESTADO, String(z1_estado_rele == -1 ? 0 : z1_estado_rele).c_str(), true);
    } else {
      Serial.printf(" Falhou, rc=%d. Nova tentativa em 5s.\n", client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // CORREÇÃO CRÍTICA: Semente geradora de números pseudoaleatórios usando pino flutuante
  randomSeed(analogRead(35)); 
  
  pinMode(PINO_RELE, OUTPUT);
  pinMode(PINO_PIR, INPUT);
  
  setup_wifi();
  espClient.setInsecure(); 
  client.setBufferSize(512); 
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback); 
}

void loop() {
  if (!client.connected()) { 
    reconnect(); 
  }
  client.loop();

  // Envio periódico sem usar delay() para manter a pilha do MQTT rodando lisa
  if (millis() - ultimo_envio_sensor > 2000) {
    lerEEnviarSensoresLocais();
    ultimo_envio_sensor = millis();
  }
}