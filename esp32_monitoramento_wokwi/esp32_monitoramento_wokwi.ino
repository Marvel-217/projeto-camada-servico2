// esp32_monitoramento_wokwi.ino

#include <WiFi.h>
#include <esp_now.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <HTTPClient.h>

// ============ CONFIGURAÇÃO ============
#define MODO_SIMULADO true

// ============ PINAGEM ============
#define PIN_LED_VERDE 25  
#define PIN_LED_VERM  26  
#define PIN_MATRIX    23  // Pino DIN da matriz WS2812

// ============ CREDENCIAIS DO INFLUXDB CLOUD ============
// Cole aqui a URL que montamos e o Token que você gerou
const char* influxUrl = "https://us-east-1-1.aws.cloud2.influxdata.com/api/v2/write?org=e3196d240f6c0f6f&bucket=Pintura&precision=s";
const char* influxToken = "6zW2h0QD2Ka8dWpJY2DJSmfs2d1auABn80xsYlsimceCw6wiNLwwahZrU93dFv9haZGLwT1z5wJ0OY_hTa-A_w==";

// ============ CONEXÃO WIFI (Wokwi) ============
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ============ CONFIGURAÇÃO DA MATRIZ NEOPIXEL ============
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(8, 8, PIN_MATRIX,
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
  NEO_MATRIX_ROWS    + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

// ============ ESTRUTURAS ============
typedef struct __attribute__((packed)) {
  float nivel_tinta;
  float temperatura;
  float umidade;
  int   luminosidade;
  int   presenca;
  uint32_t timestamp_ms;
} PacoteDados;

PacoteDados pacote;
unsigned long lastRxMillis = 0;
const unsigned long TIMEOUT_MS = 5000;

// ============ TELAS ============
enum Tela { TELA_NVL, TELA_TMP, TELA_UMD, TELA_LUX, TELA_PRS, TELA_ALERTA };
Tela telaAtual = TELA_NVL;
unsigned long lastTrocaTela = 0;
const unsigned long TEMPO_TELA_MS = 2000;

// Variáveis para rolagem de texto
int16_t x = 8;
char textoAtual[20];

// ============ SIMULAÇÃO DE DADOS ============
void simularLeitura() {
  pacote.nivel_tinta  = 40.0 + 35.0 * sin(millis() / 8000.0);
  pacote.temperatura  = 22.0 + 4.0  * sin(millis() / 12000.0);
  pacote.umidade      = 50.0 + 15.0 * cos(millis() / 10000.0);
  pacote.luminosidade = 600   + 200  * (int)(sin(millis() / 6000.0) * 100) / 100;
  pacote.presenca     = (millis() / 7000) % 2;
  pacote.timestamp_ms = millis();
  
  // Simula alerta de nível baixo de vez em quando
  if (millis() % 25000 < 5000) pacote.nivel_tinta = 12.0; 
  
  lastRxMillis = millis();
}

// ============ EXIBIÇÃO NA MATRIZ ============
void prepararTexto(Tela t) {
  uint16_t cor = matrix.Color(0, 255, 0); // Verde padrão

  switch (t) {
    case TELA_NVL: 
      snprintf(textoAtual, sizeof(textoAtual), "NVL %.0f%%", pacote.nivel_tinta); 
      if (pacote.nivel_tinta < 20.0) cor = matrix.Color(255, 0, 0); 
      break;
    case TELA_TMP: snprintf(textoAtual, sizeof(textoAtual), "TMP %.1fC", pacote.temperatura); cor = matrix.Color(0, 255, 255); break;
    case TELA_UMD: snprintf(textoAtual, sizeof(textoAtual), "UMD %.0f%%", pacote.umidade); cor = matrix.Color(0, 0, 255); break;
    case TELA_LUX: snprintf(textoAtual, sizeof(textoAtual), "LUX %d", pacote.luminosidade); cor = matrix.Color(255, 255, 0); break;
    case TELA_PRS: snprintf(textoAtual, sizeof(textoAtual), "PRS %s", pacote.presenca ? "ON" : "OFF"); cor = matrix.Color(255, 0, 255); break;
    case TELA_ALERTA: snprintf(textoAtual, sizeof(textoAtual), "SEM DADOS"); cor = matrix.Color(255, 0, 0); break;
  }

  matrix.setTextColor(cor);
  x = 8; 
  Serial.print("[MATRIZ] "); Serial.println(textoAtual);
}

// ============ ENVIO PARA O INFLUXDB ============
void enviarParaInflux() {
  if (WiFi.status() != WL_CONNECTED) return;

  // Monta o texto no formato Line Protocol do InfluxDB
  String dados = "leitura_pintura,origem=esp32_wokwi ";
  dados += "nivel_tinta=" + String(pacote.nivel_tinta) + ",";
  dados += "temperatura=" + String(pacote.temperatura) + ",";
  dados += "umidade=" + String(pacote.umidade) + ",";
  dados += "luminosidade=" + String(pacote.luminosidade) + "i,";
  dados += "presenca=" + String(pacote.presenca) + "i";

  HTTPClient http;
  http.begin(influxUrl);
  http.addHeader("Content-Type", "text/plain; charset=utf-8");
  http.addHeader("Authorization", "Token " + String(influxToken));

  int httpResponseCode = http.POST(dados);

  if (httpResponseCode > 0) {
    Serial.print("[INFLUX] Resposta HTTP: ");
    Serial.println(httpResponseCode);
    if (httpResponseCode == 204) {
      Serial.println("[INFLUX] Dados salvos com sucesso na nuvem!");
    }
  } else {
    Serial.print("[INFLUX] Erro no envio. Código: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_VERM, OUTPUT);
  
  matrix.begin();
  matrix.setTextWrap(false); 
  matrix.setBrightness(40);  

  Serial.println("=== ESP32 Monitoramento - Wokwi ===");
  
  // Conecta ao WiFi
  Serial.print("Conectando ao WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado!");

  lastRxMillis = millis();
  prepararTexto(telaAtual);
}

// ============ LOOP ============
void loop() {
  // Animação de rolagem do texto na matriz
  matrix.fillScreen(0);
  matrix.setCursor(x, 0);
  matrix.print(textoAtual);
  matrix.show();

  x--; 
  if (x < -60) x = 8; 

  // Simula a chegada de novos dados a cada 2 segundos
  static unsigned long lastSim = 0;
  if (millis() - lastSim >= 2000) {
    lastSim = millis();
    simularLeitura();
  }

  // Verifica se os dados estão atualizados
  bool timeout = (millis() - lastRxMillis) > TIMEOUT_MS;

  if (timeout) {
    digitalWrite(PIN_LED_VERM, HIGH);
    digitalWrite(PIN_LED_VERDE, LOW);
    if (telaAtual != TELA_ALERTA) {
      telaAtual = TELA_ALERTA;
      prepararTexto(telaAtual);
    }
  } else {
    digitalWrite(PIN_LED_VERDE, HIGH);
    digitalWrite(PIN_LED_VERM, LOW);

    // Ciclo de telas
    if (millis() - lastTrocaTela >= TEMPO_TELA_MS) {
      lastTrocaTela = millis();
      telaAtual = (Tela)((telaAtual + 1) % 5);
      prepararTexto(telaAtual);
    }
  }

  // Envio para o InfluxDB a cada 5 segundos (para não sobrecarregar)
  static unsigned long lastEnvioInflux = 0;
  if (millis() - lastEnvioInflux >= 5000 && !timeout) {
    lastEnvioInflux = millis();
    enviarParaInflux();
  }

  delay(50); 
}
