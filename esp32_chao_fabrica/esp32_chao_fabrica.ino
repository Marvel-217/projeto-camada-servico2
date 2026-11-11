// esp32_chao_fabrica.ino
// ESP32 Chão de Fábrica - Adaptado para ESP32-S3 e ESP-NOW v3.x

#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>

// ===== Pinagem Adaptada para ESP32-S3 =====
#define PIN_TRIG      5   
#define PIN_ECHO      18  
#define PIN_DHT       4   
#define PIN_LDR       1   
#define PIN_PIR       6   
#define PIN_LED_VERDE 7   
#define PIN_LED_VERM  15  

#define DHTTYPE DHT22

DHT dht(PIN_DHT, DHTTYPE);

// ===== Parâmetros do tanque =====
const float TANQUE_ALTURA_CM = 30.0;   
const float TANQUE_VAZIO_CM   = 25.0;  
const float TANQUE_CHEIO_CM   = 5.0;   

// ===== Endereço MAC do ESP32 de monitoramento =====
uint8_t macMonitor[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ===== Struct do pacote =====
typedef struct __attribute__((packed)) {
  float nivel_tinta;
  float temperatura;
  float umidade;
  int   luminosidade;
  int   presenca;
  uint32_t timestamp_ms;
} PacoteDados;

PacoteDados pacote;
esp_now_peer_info_t peerInfo;

// ===== Callback de envio (v3.x) =====
void onEnvio(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("Pacote enviado com sucesso");
    digitalWrite(PIN_LED_VERDE, HIGH);
    delay(50);
    digitalWrite(PIN_LED_VERDE, LOW);
  } else {
    Serial.println("Falha no envio ESPNOW");
    digitalWrite(PIN_LED_VERM, HIGH);
    delay(50);
    digitalWrite(PIN_LED_VERM, LOW);
  }
}

// ===== Leitura do ultrassônico =====
float lerDistanciaCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long dur = pulseIn(PIN_ECHO, HIGH, 30000);
  if (dur == 0) return -1;
  return dur * 0.0343 / 2.0;
}

float calcularNivelPercent(float dist) {
  if (dist < 0) return -1;
  float pct = (TANQUE_VAZIO_CM - dist) / (TANQUE_VAZIO_CM - TANQUE_CHEIO_CM) * 100.0;
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_VERM, OUTPUT);

  dht.begin();

  WiFi.mode(WIFI_STA);
  Serial.print("MAC deste ESP32: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

  esp_now_register_send_cb(onEnvio); 

  memcpy(peerInfo.peer_addr, macMonitor, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Falha ao adicionar peer");
  }
}

void loop() {
  float dist = lerDistanciaCm();
  float nivel = calcularNivelPercent(dist);

  float temp = dht.readTemperature();
  float umid = dht.readHumidity();
  if (isnan(temp)) temp = 0;
  if (isnan(umid)) umid = 0;

  int lux = analogRead(PIN_LDR);
  int pres = digitalRead(PIN_PIR);

  pacote.nivel_tinta  = nivel;
  pacote.temperatura  = temp;
  pacote.umidade      = umid;
  pacote.luminosidade = lux;
  pacote.presenca     = pres;
  pacote.timestamp_ms = millis();

  Serial.print("Nivel do tanque: "); Serial.print(nivel); Serial.println("%");
  Serial.print("Temperatura: "); Serial.print(temp); Serial.print(" ºC | Umidade: "); Serial.print(umid); Serial.println("%");
  Serial.print("Luminosidade: "); Serial.println(lux);
  Serial.println(pres ? "Presença detectada" : "Sem presença");

  if (nivel < 20.0) {
    digitalWrite(PIN_LED_VERM, HIGH);
    digitalWrite(PIN_LED_VERDE, LOW);
    Serial.println("Alerta! Nível de tinta baixo.");
    Serial.println("Alerta – verificar tanque de tinta");
  } else {
    digitalWrite(PIN_LED_VERM, LOW);
    digitalWrite(PIN_LED_VERDE, HIGH);
    Serial.println("Operação normal");
  }

  esp_err_t r = esp_now_send(macMonitor, (uint8_t *)&pacote, sizeof(pacote));
  if (r != ESP_OK) {
    Serial.println("Falha no envio ESPNOW (erro)");
    digitalWrite(PIN_LED_VERM, HIGH);
    delay(100);
    digitalWrite(PIN_LED_VERM, LOW);
  }

  Serial.println("---------------------------------");
  delay(2000);
}
