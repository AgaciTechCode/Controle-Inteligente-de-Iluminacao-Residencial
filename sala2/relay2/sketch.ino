///////////////////////////////////////
///         PLACA: ZONA 2           ///
///   Controla Rele 2, PIR 2, LDR 2  ///
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

const char* TOPICO_SUB_SALA2 = "/sensores/+/sala2/+/+";

const char* TOPICO_Z2_MODO    = "/servicos/atuador/sala2/zona2/modo";
const char* TOPICO_Z2_COMANDO = "/servicos/atuador/sala2/zona2/comando";
const char* TOPICO_Z2_ESTADO  = "/servicos/atuador/sala2/zona2/estado";

const char* TOPICO_PUB_PIR2 = "/sensores/presenca/sala2/zona2/pir2";
const char* TOPICO_PUB_LDR2 = "/sensores/luminosidade/sala2/zona2/ldr2";

const int PINO_RELE = 14;
const int PINO_PIR  = 27;
const int PINO_LDR  = 32;

int z2_movimento    = 0;
int z2_luminosidade = 100;
String z2_modo_operacao = "AUTOMATICO";
int z2_comando_manual = 0;
int z2_estado_rele  = -1;

int z1_movimento    = 0;
int z1_luminosidade = 100;

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

  if (z2_modo_operacao == "MANUAL") {
    novo_estado_rele = z2_comando_manual;
  }
  else if (z2_modo_operacao == "ECONOMICO") {
    if (z2_movimento == 1 && z2_luminosidade <= 30) {
      novo_estado_rele = 1;
    }
  }
  else if (z2_modo_operacao == "AUTOMATICO") {
    bool condicao_z1 = (z1_movimento == 1 && z1_luminosidade <= 30);
    bool condicao_z2 = (z2_movimento == 1 && z2_luminosidade <= 30);
    if (condicao_z1 || condicao_z2) {
      novo_estado_rele = 1;
    }
  }

  if (novo_estado_rele != z2_estado_rele) {
    z2_estado_rele = novo_estado_rele;
    digitalWrite(PINO_RELE, z2_estado_rele);
    client.publish(TOPICO_Z2_ESTADO, String(z2_estado_rele).c_str(), true);
    Serial.printf("[Relé 2] Estado Modificado: %s | Modo: %s\n",
                  (z2_estado_rele == 1) ? "LIGADO" : "DESLIGADO",
                  z2_modo_operacao.c_str());
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String topicoStr = String(topic);

  if (topicoStr.indexOf("/zona2/") > 0) {
    if (topicoStr.indexOf("/presenca/") > 0 || topicoStr.indexOf("/luminosidade/") > 0) {
      return;
    }
  }

  String mensagem = "";
  for (int i = 0; i < length; i++) { mensagem += (char)payload[i]; }

  if (topicoStr.indexOf("/zona1/") > 0) {
    if (topicoStr.indexOf("/presenca/") > 0)          z1_movimento    = mensagem.toInt();
    else if (topicoStr.indexOf("/luminosidade/") > 0) z1_luminosidade = mensagem.toInt();
    Serial.printf("[Rede] Atualização da Zona 1 recebida -> PIR: %d, LDR: %d%%\n", z1_movimento, z1_luminosidade);
  }
  else if (topicoStr == TOPICO_Z2_MODO) {
    z2_modo_operacao = mensagem;
    Serial.printf("[Painel Web] Alteração de Modo Z2: %s\n", z2_modo_operacao.c_str());
  }
  else if (topicoStr == TOPICO_Z2_COMANDO) {
    z2_comando_manual = mensagem.toInt();
    Serial.printf("[Painel Web] Comando Manual Recebido Z2: %d\n", z2_comando_manual);
  }

  processarEstrategiaAtuacao();
}

void lerEEnviarSensoresLocais() {
  z2_movimento = digitalRead(PINO_PIR);

  int leitura_analogica = analogRead(PINO_LDR);
  z2_luminosidade = map(leitura_analogica, 0, 4095, 0, 100);

  client.publish(TOPICO_PUB_PIR2, String(z2_movimento).c_str(), false); // retain removido no PIR
  client.publish(TOPICO_PUB_LDR2, String(z2_luminosidade).c_str(), true);

  Serial.printf("[Local] Telemetria enviada -> PIR2: %d | LDR2: %d%%\n", z2_movimento, z2_luminosidade);

  processarEstrategiaAtuacao();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("[MQTT] Conectando Placa Zona 2...");
    String clientId = "ESP32_Z2_Sala2_" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass, 0, 0, 0, 0, true)) {
      Serial.println(" Conectado com sucesso!");

      client.subscribe(TOPICO_SUB_SALA2);
      client.subscribe(TOPICO_Z2_MODO);
      client.subscribe(TOPICO_Z2_COMANDO);

      client.publish(TOPICO_Z2_ESTADO, String(z2_estado_rele == -1 ? 0 : z2_estado_rele).c_str(), true);
    } else {
      Serial.printf(" Falhou, rc=%d. Nova tentativa em 5s.\n", client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(33)); // pino flutuante diferente do Z1 (que usa 35)

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

  if (millis() - ultimo_envio_sensor > 2000) {
    lerEEnviarSensoresLocais();
    ultimo_envio_sensor = millis();
  }
}
