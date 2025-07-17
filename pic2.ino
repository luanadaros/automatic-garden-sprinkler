#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define pinoAnalog A0   // Único pino analógico do ESP8266
#define pinoRele 5      // GPIO5 é seguro para uso como saída

// credenciais wifi
const char* ssid = "PIC2-2.4G";
const char* password = "engcomp@ufes";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

int ValAnalogIn;
bool clientConnected = false; // Flag para verificar conexão

//funções websocket
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    String msg = "";
    for (size_t i = 0; i < len; i++) msg += (char)data[i];
    msg.trim();

    if (msg == "UMIDADE") {
      Serial.println("Mensagem 'UMIDADE' recebida via WebSocket!"); 
      ValAnalogIn = analogRead(pinoAnalog);
      int Porcento = map(ValAnalogIn, 1023, 0, 0, 100);
      ws.textAll(String(Porcento));  // envia só o valor da umidade
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
             AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch(type) {
    case WS_EVT_CONNECT:
      Serial.println("Cliente WebSocket conectado!");
      clientConnected = true;
      break;
    case WS_EVT_DISCONNECT:
      Serial.println("Cliente WebSocket desconectado!");
      clientConnected = false;
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(pinoRele, OUTPUT);
  digitalWrite(pinoRele, LOW);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando ao WiFi...");
  }

  Serial.print("Conectado! IP: ");
  Serial.println(WiFi.localIP());

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
}

void loop() {
  ws.cleanupClients();
  ValAnalogIn = analogRead(pinoAnalog); // Leitura de 0 a 1023
  
  int Porcento = map(ValAnalogIn, 1023, 0, 0, 100); // 0 = solo molhado, 1023 = solo seco

  Serial.print(Porcento);
  Serial.println("%");

  if (Porcento <= 30) {
    Serial.println("Irrigando a planta ...");
    digitalWrite(pinoRele, HIGH);
  } else {
    Serial.println("Planta Irrigada ...");
    digitalWrite(pinoRele, LOW);
  }

/*
  if (clientConnected) {
    ws.textAll(String(Porcento));
    Serial.println("Enviado: " + Porcento);
  }
*/
  delay(1000);
}
