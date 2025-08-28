#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Servo.h>

#define pinoAnalog A0   // Único pino analógico do ESP8266
#define pinoRele 5      // GPIO5 é seguro para uso como saída
#define pinoServo 4  // D2 = GPIO4
#define pinoServoZ 14  // D5 = GPIO14
#define pinoIRSensor 13 // D6 = GPIO12 
#define pinoEnable 12  // D7 = GPIO13

int gridAtual = 1;  // Começa no grid 1

Servo servoMotor;
Servo servoZ;

// credenciais wifi
const char* ssid = "PIC2-2.4G";
const char* password = "engcomp@ufes";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

int ValAnalogIn;
bool clientConnected = false; // Flag para verificar conexão

bool movendo = false;

//funções websocket para conexao com app
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    String msg = "";
    for (size_t i = 0; i < len; i++) msg += (char)data[i];
    msg.trim();

    if (msg == "UMIDADE") {
      Serial.println("Mensagem 'UMIDADE' recebida via WebSocket!"); 
      medir_umidade();
    } 
    else if (msg == "REGAR"){
      Serial.println("Mensagem 'REGAR' recebida via WebSocket!"); 
      regar(0);
    }
    else if (msg == "ROTINA") {
      Serial.println("INICIANDO ROTINA...");
      medir_umidade(); // Medindo grid 1
      moverEntreGrids(gridAtual, 2);  // De onde está para o grid 2
      medir_umidade(); // Medindo grid 2
      moverEntreGrids(gridAtual, 3);  // Agora para o grid 3
      medir_umidade(); // Medindo grid 3
      moverEntreGrids(gridAtual, 1);  // Agora para o grid 1
      delay(5000); // Pausa opcional entre ciclos
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

//setup
void setup() {
  Serial.begin(115200);
  
  pinMode(pinoRele, OUTPUT);
  pinMode(pinoIRSensor, INPUT);
  digitalWrite(pinoRele, LOW);
  pinMode(pinoEnable, OUTPUT);
  digitalWrite(pinoEnable, LOW);

  servoMotor.attach(pinoServo);  // Liga o servo ao pino D2
  servoZ.attach(pinoServoZ, 500, 2400); 
  servoZ.write(0);
  servoMotor.write(89);          // Posição neutra (parado, se for um servo contínuo)

  /*
   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando ao WiFi...");
  }

  Serial.print("Conectado! IP: ");
  Serial.println(WiFi.localIP());
  */
  
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
}


// Função para ler o sensor IR 10 vezes e retornar a maioria (LOW ou HIGH)
int leituraFiltradaIR() {
  int countLow = 0;
  int countHigh = 0;

  Serial.println("Status do sensor");
  Serial.println(pinoEnable);
  Serial.println("Leituras do Sensor");
  for (int i = 0; i < 10; i++) {
    int leitura = digitalRead(pinoIRSensor);
    Serial.println(leitura);
    
    if (leitura == LOW) countLow++;
    else countHigh++;
    delay(50); 
  }

  if (countLow > countHigh) return LOW;
  else return HIGH;
}

void moverEntreGrids(int origem, int destino) {
  if (origem == destino) {
    Serial.println("Origem e destino são iguais. Nenhum movimento necessário.");
    return;
  }

  Serial.print("Movendo de GRID ");
  Serial.print(origem);
  Serial.print(" para GRID ");
  Serial.println(destino);

  int direcao;
  if (destino > origem) {
    direcao = 94;  // Direita (ajustar se necessário)
  } else {
    direcao = 83;  // Esquerda (ajustar se necessário)
  }

  int movimentos = origem - destino;
  if(movimentos < 0){
    movimentos = -movimentos;
  }

  // Liga movimento
  servoMotor.write(direcao);
  Serial.println("Motor ligado, aguardando detecção IR...");
  digitalWrite(pinoEnable, HIGH);

  for(int i = 0; i < movimentos; i++){
    unsigned long tempoInicio = millis();
    while(digitalRead(pinoIRSensor) == LOW && millis() - tempoInicio < 30000){
      //Serial.println("obstaculo");
      delay(250);
    }
    delay(2000);
    while(digitalRead(pinoIRSensor) == HIGH && millis() - tempoInicio < 30000){
      //Serial.println("livre");
      delay(250);
    }

    if(millis() - tempoInicio >= 30000){
      //Serial.println("TIMEOUT");
    }
  }

  // Para o motor
  servoMotor.write(89); // neutro (parado)
  delay(500);
  gridAtual = destino;
}

void mover_sensor(int angulo) {
  servoZ.write(angulo);
  delay(8000); // dá tempo para o servo girar
}

void regar(double umidade){
  if (umidade <= 65) {
    Serial.println("Irrigando a planta ...");
    digitalWrite(pinoRele, HIGH);
    delay(500);
    digitalWrite(pinoRele, LOW);
  } else {
    Serial.println("Planta Irrigada ...");
    digitalWrite(pinoRele, LOW);
  } 
}

void medir_umidade(){
  Serial.println("Descendo sensor ...");
  servoZ.write(180);
  delay(4000);
  double porcentagem = 0;
  for(int i = 0; i < 5; i++){
    ValAnalogIn = analogRead(pinoAnalog);
    porcentagem += map(ValAnalogIn, 1023, 0, 0, 100);
    delay(200);
  }
  double media_porcentagem = porcentagem/5;
  delay(100);
  Serial.println("Subindo sensor ...");
  servoZ.write(0);
  delay(2000);
  Serial.println("UMIDADE MENSURADA: ");
  Serial.println(media_porcentagem);
  regar(media_porcentagem);
}

unsigned long tempoAnterior = 0;           // Última vez que o sinal foi disparado
unsigned long intervalo = 10000;    // 15 minutos em milissegundos (60 * 1000)

void rotina(){
  delay(5000);
  Serial.println("MEDINDO UMIDADE");
  Serial.println("GRID ATUAL: ");
  Serial.println(gridAtual);
  medir_umidade();
  moverEntreGrids(1, 2);
  Serial.println("GRID ATUAL: ");
  Serial.println(gridAtual);
  delay(1000);
  medir_umidade();
  moverEntreGrids(2, 3);
  Serial.println("GRID ATUAL: ");
  Serial.println(gridAtual);
  delay(1000);
  medir_umidade();
  moverEntreGrids(3, 1);
  Serial.println("GRID ATUAL: ");
  Serial.println(gridAtual);
  delay(3000);
}


int val = 0;
void loop() {
  //ws.cleanupClients(); // WebSocket manutenção
  delay(5000);
  if(val == 0){
    rotina();
    val++;
  }
  delay(10000);
}
