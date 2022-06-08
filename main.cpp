#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <bits/stdc++.h>
#include <LiquidCrystal.h>
#include <SD.h>
#include <Wire.h>
#include <RtcDS3231.h>


//  gps Ublox 7 pinning
static const int RXPin = 16, TXPin = 17;
static const uint32_t GPSBaud = 9600;

// inicialização única
bool inicial = true;

String instantLat = "";
String instantLng = "";

TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);

RtcDS3231<TwoWire> Rtc(Wire);
LiquidCrystal lcd(13, 12, 14, 27, 26, 25);

#define CS_PIN  D8

const char* ssid =                                                                       "alisson";
const char* password =                                                                "alisson303";

String device_disp =                                                                     "AICM_44";
String device_ID =                                                                            "44";
String device_user =                                                                        "2222";
String device_hash =  "$5$rounds=5000$steamedhams2222$h5tV6LCoMOVT43OWgnAMztwv4GbuMI.a5aS4b5M92tC";
String device_salt =                                             "$5$rounds=5000$steamedhams2222$";
String serverName =                                        "http://acerture.com/api/HttpPost2.php";
String echoDateServer =                                     "http://acerture.com/api/EchoDate.php";
String echoTimeServer =                                     "http://acerture.com/api/EchoTime.php";

String payload = "";
char date[20];

String retainedPayload = "";
String sinal = "desligado";

String dateNow = "";  // para poder inicializar com tempo caso não haja conexão com a internet logo de início e não popular qualquer bobagem no módulo RTC e causar bugs
String timeNow = "";

int httpResponseCode;

float timer = 0;

unsigned long startMillis;
unsigned long currentMillis;
const unsigned long tempoEntreLeituras = 40 * 1000; // valor em milisegundos (10^-3s) 5s         // PUBLICAÇÃO EM BANCO DE DADOS

unsigned long startMillis2;
unsigned long currentMillis2;
const unsigned long tempoEntreLeituras2 = 1000 * 1000; // valor em milisegundos (10^-3s) 100s   // UPDATE DO HORÁRIO DO RELÓGIO INTERNO, RTC a cada 15 min aproximadamente

unsigned long startMillis3;
unsigned long currentMillis3;
const unsigned long tempoEntreLeituras3 = 86400 * 1000; // valor em milisegundos (10^-3s) 100s    // RESET DA PLACA INTEIRA a cada 01 dia.

int i = 0;
const int buttonPin = 15;
int n = 0; // número de mensagens retidas.

String auxState = "";

// CARACTERÍSTICAS DE RETENÇÃO DE VARIÁVEIS.

char charArray[25000];  // array 01 contendo todos os payloads não enviados.
char charArray2[25000]; // array 02 contendo todos os payloads não enviados.
char payloadChar[100];  // array contendo o payload atual. Lembre-se: o payload é uma string.

void setup() // ------------------------ V O I D     S E T U P  ------------------------------
{
  Serial.begin(115200);
  ss.begin(GPSBaud);
  Rtc.Begin();
  lcd.begin(20, 4);

  WiFi.begin(ssid, password);
  Serial.println("Conectando...");
  lcd.print("Conectando...");

  // Aguarda a primeira conexão com Wi-fi para ir seguir adiante
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.println(".");
    lcd.print(".");
  }



} // ------------------------E N D     V O I D     S E T U P  ------------------------------

void(* resetFunc) (void) = 0;  // declare reset fuction at address 0

void loop() // ------------------------ V O I D     L O O P ------------------------------
{
  if (inicial)
  { //////////////////   INICIALIZAÇÃO ÚNICA   //////////////////
    startMillis = millis();  //initial start time
    startMillis2 = millis();  //initial start time
    startMillis3 = millis();  //initial start time

    BoasVindas();
    FirstTimeUpdate();
    payload = "aehooooooooooooooooooooooo";
    inicial = false;
  } //////////////////   INICIALIZAÇÃO ÚNICA   //////////////////

  currentMillis = millis();
  currentMillis2 = millis();
  currentMillis3 = millis();

  while (ss.available() > 0)
    if (gps.encode(ss.read()))
    {
      GetGPSInfo();
      Serial.println(F(" "));
      Serial.println(instantLat);
      Serial.println(instantLng);

      LCDIddle();         // informações persistentes na tela LCD

      // ReadDigitalPort();  // grava o estado atual na variavel "sinal"

      GetDate();          // Grava na variável date a hora atual através da leitura do RTC.

      payload.toCharArray(payloadChar, 100);
      strcat(payloadChar, date);

      // Rotinas baseadas em tempo //

      if ((currentMillis - startMillis >= tempoEntreLeituras) || ( sinal != auxState ))               // tempo entre publicar em banco.
      {
        if (WiFi.status() == WL_CONNECTED)
        {
          PublicarHTTP();
          auxState = sinal;
        }
        else
        {
          if (n < 200)
          {
            strcat(charArray, payloadChar);
            n++;
          }
          else // overflow na memória de retenção.
          {
            Serial.println("Memória cheia.");
          }
        }
        startMillis = currentMillis;
      }

      if (currentMillis2 - startMillis2 >= tempoEntreLeituras2)  // tempo entre atualização de horário.
      {
        Serial.println("Tentando atualizar o horário interno da placa...");
        if (WiFi.status() == WL_CONNECTED)
        {
          UpdateDate();
          UpdateTime();
          UpdateRTCDateTime();
        }
        startMillis2 = currentMillis2;
      }

      if (currentMillis3 - startMillis3 >= tempoEntreLeituras3)  // tempo para um hardReset na placa.
      {
        Serial.println("RESETANDO A PLACA...");
        resetFunc();

        startMillis3 = currentMillis3;
      }

      if (WiFi.status() != WL_CONNECTED)
      {
        lcd.clear();
        Serial.println("WiFi Disconnected");
        lcd.setCursor(0, 0);
        lcd.print("Reconectando...");
        lcd.setCursor(0, 1);
        lcd.print(String(ssid).c_str());
        delay(1000);
      }


    }


  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS detected: check wiring."));
    while (true);
  }

}// ------------------------ E N D    V O I D     L O O P ------------------------------



// ------------- M É T O D O S ----------------

void LCDIddle()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ACERTURE TECH");
  lcd.setCursor(0, 1);
  lcd.print("wifi: ");
  lcd.setCursor(7, 1);
  lcd.print(String(ssid).c_str());
  lcd.setCursor(0, 2);
  lcd.print("IP: ");
  lcd.print(WiFi.localIP());
  lcd.setCursor(17, 1);
  lcd.print(n);
  lcd.setCursor(0, 3);
  lcd.print(date);
}

void GetDate()
{
  RtcDateTime instante = Rtc.GetDateTime();
  sprintf(date, "%d-%d-%d %d:%d:%d^",
          instante.Year(),
          instante.Month(),
          instante.Day(),
          instante.Hour(),
          instante.Minute(),
          instante.Second()
         );
}

void UpdateDate()
{
  WiFiClient client;
  HTTPClient http;

  String serverPathEchoDate = echoDateServer; // Acerture API para echo:date as __DATE__ format (c++ default format)
  http.begin(client, serverPathEchoDate.c_str());
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String httpRequestData = "";
  int httpResponseCodeEchoDate  = http.POST(httpRequestData);
  String serverResponse = http.getString();
  
  if (httpResponseCodeEchoDate == 200) // mensagem recebida com sucesso.
  {
    Serial.println("Data do servidor armazenada em variável global dateNow ---> ");
    dateNow = serverResponse;
    Serial.println(dateNow);

  } else
  {
    Serial.println("Não foi possível obter a data do servidor. Problema de requisição HTTP. Provavelmente sem internet.");
  }

  Serial.println(serverResponse);

  http.end();
}

void UpdateTime()
{
  WiFiClient client;
  HTTPClient http;

  String serverPathEchoTime = echoTimeServer; // Acerture API para echo:date as __DATE__ format (c++ default format)
  http.begin(client, serverPathEchoTime.c_str());
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String httpRequestData = "";
  int httpResponseCodeEchoTime  = http.POST(httpRequestData);
  String serverResponse = http.getString();
  if (httpResponseCodeEchoTime == 200) // mensagem recebida com sucesso.
  {
    Serial.println("Hora do servidor armazenada em variável global timeNow ---> ");
    timeNow = serverResponse;
    Serial.println(timeNow);

  } else
  {
    Serial.println("Não foi possível obter a hora do servidor. Problema de requisição HTTP. Provavelmente sem internet.");
  }

  Serial.println(serverResponse);

  http.end();
}

void UpdateRTCDateTime()
{
  Rtc.Begin();
  // RtcDateTime tempoatual = RtcDateTime(dateNow, timeNow);
  RtcDateTime tempoatual = RtcDateTime(dateNow.c_str(), timeNow.c_str());
  Rtc.SetDateTime(tempoatual);
}

void PublicarHTTP()
{
  WiFiClient client;
  HTTPClient http;

  String serverPath = serverName; // url da API para requisção HttpPost
  http.begin(client, serverPath.c_str());
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  if (n == 0)
  {
    String httpRequestData = device_disp + "§" + device_user  + "§" +  device_hash  + "§" +  device_salt + "§" + payloadChar + "§" + device_ID + "§";   // "§" used to split string into separate words on backend.
    httpResponseCode  = http.POST(httpRequestData);
  }
  else
  {
    String httpRequestData = device_disp + "§" + device_user  + "§" +  device_hash  + "§" +  device_salt + "§" + charArray + "§" + device_ID + "§";   // "§" used to split string into separate words on backend.
    httpResponseCode  = http.POST(httpRequestData);
  }

  // requisição HTTP
  if (httpResponseCode == 200) // mensagem recebida com sucesso.
  {
    Serial.println("-- HTTP RESPONSE CODE --");
    Serial.println(httpResponseCode);
    String serverResponse = http.getString();
    Serial.println("-- RESPONSE FROM SERVER --");
    Serial.println(serverResponse);

    // Atualizar na tela o status da requisição http
    lcd.setCursor(13, 0);
    lcd.print("Pub");

    // Zera as arrays apenas se existir entradas.
    if (n != 0)
    {
      charArray[0] = '\0'; // procedimento para zerar o charArray[];
    }
    n = 0;
  }
  else // conectado no Wi-Fi, mas sem internet
  {
    Serial.println("-- HTTP RESPONSE CODE --");
    Serial.println(httpResponseCode);
    String serverResponse = http.getString();
    Serial.println("-- RESPONSE FROM SERVER --");
    Serial.println(serverResponse);

    // Atualizar na tela o status da requisição http
    lcd.setCursor(13, 0);
    lcd.print("Err");

    if (n < 200)
    {
      strcat(charArray, payloadChar);
      // Serial.println(charArray);
      n++;
    }
    else // overflow na memória de retenção.
    {
      Serial.println("Memória cheia.");
    }


  }

  http.end();

} // end void PublicarHttp();

void GetGPSInfo()
{
  Serial.print(F("Location: "));
  //if (gps.location.isValid())
  // {
  Serial.print(gps.location.lat(), 6);
  Serial.print(F(","));
  Serial.print(gps.location.lng(), 6);
  //}
  //else
  //{
  //  Serial.print(F("INVALID"));
  //}

  instantLat = ( gps.location.lat() );
  instantLng = ( gps.location.lng() );

  Serial.print(F("  Date/Time: "));
  if (gps.date.isValid())
  {
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F(" "));
  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
    Serial.print(F("."));
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gps.time.centisecond());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.println();
}

void BoasVindas()
{

  for (int i = 0; i < 20; i++)
  {
    Serial.println(F(""));
  }
  Serial.println(F("Dispositivo iniciando..."));
  Serial.println(F("Versão do código:   AICM_esp_v2 "));
  Serial.println(F("Features:  GPS, LCD 20X4, RTC."));

}

void FirstTimeUpdate()
{
  Serial.println("");
  Serial.println("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  lcd.setCursor(0, 0);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bem-vindo...");
  delay(200);
  lcd.setCursor(0, 1);
  lcd.print("ACERTURE");
  delay(200);
  lcd.setCursor(0, 2);
  lcd.print("wi-fi: ");
  delay(200);
  lcd.setCursor(0, 3);
  lcd.print(String(ssid).c_str());
  delay(200);

  delay(500);

  UpdateDate();
  UpdateTime();

  if ((dateNow != "") && (timeNow != ""))
  {
    UpdateRTCDateTime();
  }

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("updating datetime.. ");
  lcd.setCursor(0, 2);
  lcd.print(date);

  delay(2000);

}



// ------------- E N D     M É T O D O S ----------------
