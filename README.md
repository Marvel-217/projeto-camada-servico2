# Sistema de Monitoramento de Pintura (Camada de Serviço) 🎨

Projeto desenvolvido para o Edital 02 - Práticas Integradas: Camada de Serviço (SENAI). O objetivo é simular um processo de pintura de blocos de madeira com monitoramento em tempo real via IoT.

## 🏗️ Arquitetura do Projeto
O sistema é dividido em duas partes principais:
1. **ESP32 Chão de Fábrica:** Lê os sensores (Ultrassônico, DHT22, LDR, PIR) e envia os dados via rádio (ESP-NOW).
2. **ESP32 de Monitoramento (Wokwi):** Recebe os dados, exibe em uma matriz de LED (NeoPixel) e envia as informações para a nuvem via HTTP.

## 🔌 Pinagem (ESP32)
**Chão de Fábrica (Físico):**
- Ultrassônico (Nível): TRIG (5) e ECHO (18)
- DHT22 (Temp/Umidade): 4
- LDR (Luminosidade): 1
- PIR (Presença): 6
- LED Verde / Vermelho: 7 e 15

**Monitoramento (Wokwi):**
- Matriz NeoPixel (DIN): 23
- LED Verde / Vermelho: 25 e 26

## 🚀 Como testar a Simulação (Wokwi)
1. Abra o código `esp32_monitoramento_wokwi.ino` no Wokwi.
2. Certifique-se de que a variável `MODO_SIMULADO` está definida como `true`.
3. No Library Manager, adicione as bibliotecas `Adafruit NeoPixel` e `Adafruit GFX Library`.
4. Clique em Play e observe a matriz rolando os dados e os LEDs de status.

## ☁️ Etapa 03: InfluxDB Cloud
Para o banco de dados, foi utilizada a API de Gravação direta do **InfluxDB Cloud** (sem necessidade de backend intermediário em Python).
O ESP32 envia os dados via requisição HTTP POST a cada 5 segundos, no formato *Line Protocol*.

<img width="560" height="207" alt="grafico_influx png" src="https://github.com/user-attachments/assets/f90e2fab-0e13-45d7-8f81-e7cc96f5c759" />


## 📊 Como consultar os dados salvos
No Data Explorer do InfluxDB, utilize a seguinte query em Flux para visualizar o nível de tinta:
```flux
from(bucket: "Pintura")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "leitura_pintura")
  |> filter(fn: (r) => r._field == "nivel_tinta")
  |> last()
