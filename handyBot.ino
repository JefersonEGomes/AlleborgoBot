/*
Grupo Handy 2021
Membros : 
Lucas Gabriel
Jeferson Gomes
Horacio Hugo
Victor Mendes
Vinicius Vieira

Tecnologias utilizadas :
1 Esp32-cam
2 Bot Father ( Telegram )
3 User ID ( Telegram )
4 API do Telegram
*/

// Importando as bibliotecas necessarias para o projeto ser executado
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

const char* ssid = "JACIEL";
const char* password = "a1s2d3f4g5";

// Inicializando o AlleborgoBot
String BOTtoken = "2140309502:AAFoIXteOzmJbcXcBZMkIckTMGeq91Lv3f8";  // Bot Token Obtido atraves do Bot Father no telegram

// Usando @myidbot para descobrir o ID do bate-papo de um indivíduo ou grupo
// Inicializando meu ID da conversa
String CHAT_ID = "1760314752";

// Inicializa o marcador para o envio da fot
bool sendPhoto = false;

// Criando um cliente wifi para estabelecer a conexão segura
WiFiClientSecure clientTCP;

// Inicializado o bot com TCP do wifi e o token
UniversalTelegramBot bot(BOTtoken, clientTCP);

// Variavel para o flash do esp32 e setando como low ( desligado )
#define FLASH_LED_PIN 4
bool flashState = LOW;

//Checa as mensagens a cada segundo
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

#define CAMERA_MODEL_AI_THINKER

//Pinagem da camera , segundo a documentação de pinagem do esp32 cam

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22



// Inicializa o esp 32 e atribui os pino para funcionamento
void configInitCamera(){
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;

  //Inicia as configurações de especificações 
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;  // Quanto menor o numero, maior a qualidade obs.: numeros entre 0 - 63
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;  //// Quanto menor o numero, maior a qualidade obs.: numeros entre 0 - 63
    config.fb_count = 1;
  }
  
  // Inicializa a camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) { // condição para verificar integridade da camera
    Serial.printf("Falha ao iniciar a camera erro 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  // Configurações referente ao tamanho de frame para otimizar o funcionamento da camera
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_CIF);  // UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA
}

// Identifica novas mensagens e controla os acontecimentos apos isso
void handleNewMessages(int numNewMessages) {
  Serial.print("Nova mensagem: ");
  Serial.println(numNewMessages);

// identifica a identidade do receptor se houver novas mensagens
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Usuario invalido", "");
      continue;
    }
    
    // Imprime a mensagem que foi recebida
    String text = bot.messages[i].text;
    Serial.println(text);

    // salva o nome de quem enviou e utiliza logo depois para mandar a mensagem de bem vindo e as outras opções
    String from_name = bot.messages[i].from_name;
    if (text == "/start") {
      String welcome = "Bem vindo , " + from_name + "\n";
      welcome += "Use os seguintes comandos para interagir com o bot \n";
      welcome += "/photo : captura uma nova foto\n";
      welcome += "/flash : Liga o flash \n";
      bot.sendMessage(CHAT_ID, welcome, "");
    }
    if (text == "/flash") {
      
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
        if (flashState == HIGH ){
          String feedback = "Led Ligada";
      
          bot.sendMessage(CHAT_ID, feedback, "");
      }
      if (flashState == LOW ){
          String feedback = "Led Desligada";
      
          bot.sendMessage(CHAT_ID, feedback, "");
      }
      Serial.println("Change flash LED state");
    }
    if (text == "/photo") {
      String foto = "Aqui esta sua foto :)";
      sendPhoto = true;
      bot.sendMessage(CHAT_ID, foto, "");
      Serial.println("New photo request");
    }
  }
}

// Função para tirar a foto
String sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";

  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Falha na captura da camera");
    delay(1000);
    ESP.restart();
    return "Falha na captura da camera";
  }  
  
  Serial.println("Conectando " + String(myDomain));

// conecta com o cliente e faz requisições post para o envio da foto e suas caracteristicas
  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Conexão concluida");
    
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID + "\r\n--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";

    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;
  
    clientTCP.println("POST /bot"+BOTtoken+"/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    clientTCP.println();
    clientTCP.print(head);
  
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0;n<fbLen;n=n+1024) {
      if (n+1024<fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        clientTCP.write(fbBuf, remainder);
      }
    }  
    
    clientTCP.print(tail);
    
    esp_camera_fb_return(fb);

    // Tempo de espera entre as funções utilizando o milis ( tempo de máquina)
    int waitTime = 10000;   // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;

    // faz tambem algumas verificações para ver se a conexão com a api está funcionando
    while ((startTimer + waitTime) > millis()){
      Serial.print(".");
      delay(100);      
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if (state==true) getBody += String(c);        
        if (c == '\n') {
          if (getAll.length()==0) state=true; 
          getAll = "";
        } 
        else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length()>0) break;
    }
    clientTCP.stop();
    Serial.println(getBody);
  }
  else {
    getBody="Conexão com api.telegram.org falhou.";
    Serial.println("Conexão com api.telegram.org falhou.");
  }
  return getBody;
}

void setup(){
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  // Inicializa o monitor serial
  Serial.begin(115200);

  // Seta o LED como output
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, flashState);

  // Configura e inicia a camera
  configInitCamera();

  // Conecta ao Wi-Fi
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Conectado ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("ESP32-CAM Endereço IP : ");
  Serial.println(WiFi.localIP()); 
}

void loop() {
  // comando para execução o envio e registor da foto
  if (sendPhoto) {
    Serial.println("Preparando a foto");
    sendPhotoTelegram(); 
    sendPhoto = false; 
  }
  if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      Serial.println("Resposta obtida");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}
