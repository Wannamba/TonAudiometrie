#include <ESP8266WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_VS1053.h>
#include <SoftwareSerial.h>

// pins grove speech recognizer
#define SOFTSERIAL_RX_PIN  5
#define SOFTSERIAL_TX_PIN  4

SoftwareSerial softSerial(SOFTSERIAL_RX_PIN,SOFTSERIAL_TX_PIN);

// pins adafruit mp3 featherwing
#define VS1053_RESET   -1     // VS1053 reset pin (not used!)

// Feather ESP8266
#define VS1053_CS      16     // VS1053 chip select pin (output)
#define VS1053_DCS     15     // VS1053 Data/command select pin (output)
#define CARDCS         2     // Card chip select pin
#define VS1053_DREQ    0     // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer musicPlayer = 
  Adafruit_VS1053_FilePlayer(
    VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);

int aba[] = {250, 500, 1000, 1500, 2000, 3000, 4000, 6000, 8000};

void setup() { // Einmalige Initialisierung
  Serial.begin(9600);
  while (!Serial) { delay(1); }
  
  // grove speech recognizer
  softSerial.begin(9600);
  softSerial.listen();

  // adafruit mp3 featherwing
  Serial.println("\n\nAdafruit VS1053 Feather Test");
  if (!musicPlayer.begin()) {
    Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
    while (1);
  }
  Serial.println(F("VS1053 found"));
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");
  printDirectory(SD.open("/"), 0);  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(40,40);  
#if defined(__AVR_ATmega32U4__) 
  // Timer interrupts are not suggested, better to use DREQ interrupt!
  // but we don't have them on the 32u4 feather...
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT); // timer int
#else
  // If DREQ is on an interrupt pin we can do background
  // audio playing
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
#endif

  // WLAN connection
  randomSeed(analogRead(A0) + analogRead(A0) + analogRead(A0));
  Serial.println();
  //------------ WLAN initialisieren
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  delay(100);
  Serial.print ("\nWLAN connect to:");
  Serial.print("WLAN-NAME");
  WiFi.begin("WLAN-NAME", "PASSWORT");
  while (WiFi.status() != WL_CONNECTED) { // Warte bis Verbindung steht
    delay(500);
    Serial.print(".");
  };
  Serial.println ("\nconnected, meine IP:" + WiFi.localIP().toString());
}

void loop() { // Kontinuierliche Wiederholung
  char cmd;
  if(softSerial.available())
  {
    cmd = softSerial.read();
    if (cmd){
      
      // Da der SD-Kartenleser nur 8 Zeichen des Dateinamens lesen kann
      // wurden alle Frequenzen und Zwischensequenzen nach "track0XX" umbenannt
      
      musicPlayer.setVolume(40,40);
      // Play a file in the background, REQUIRES interrupts!
      musicPlayer.playFullFile("/track050.mp3"); // intro
      playFrequences(musicPlayer, true); // left ear
      musicPlayer.setVolume(40,40);
      musicPlayer.playFullFile("/track051.mp3"); // change ear
      playFrequences(musicPlayer, false); // right ear
      musicPlayer.setVolume(40,40);
      musicPlayer.playFullFile("/track052.mp3"); // outro
    }
  }
}

//--------------------------------------- File listing helper
void printDirectory(File dir, int numTabs) {
   while(true) {
     
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       //Serial.println("**nomorefiles**");
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('\t');
     }
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numTabs+1);
     } else {
       // files have sizes, directories do not
       Serial.print("\t\t");
       Serial.println(entry.size(), DEC);
     }
     entry.close();
   }
}

//--------------------------------------- HTTP-Get
int httpGET(String host, String cmd, String &antwort, int Port) {
  WiFiClient client; // Client über WiFi
  String text = "GET http://" + host + cmd + " HTTP/1.1\r\n";
  text = text + "Host:" + host + "\r\n";
  text = text + "Connection:close\r\n\r\n";
  int ok = 1;
  if (ok) { // Netzwerkzugang vorhanden
    ok = client.connect(host.c_str(), Port); // verbinde mit Client
    if (ok) {
      client.print(text);                 // sende an Client
      for (int tout = 1000; tout > 0 && client.available() == 0; tout--)
        delay(10);                         // und warte auf Antwort
      if (client.available() > 0)         // Anwort gesehen
        while (client.available())         // und ausgewertet
          antwort = antwort + client.readStringUntil('\r');
      else ok = 0;
      client.stop();
      Serial.println(antwort);
    }
  }
  if (!ok) Serial.print(" no connection"); // Fehlermeldung
  return ok;
}

//--------------------------------------- Get Frequences per ear
boolean playFrequences(Adafruit_VS1053_FilePlayer myPlayer, const boolean leftEar){
  char read1; // read from grove speech recognizer
  int leftVolume = 100;
  int rightVolume = 100; 
  int incVolume = 100;
  int hz;
  int counter = 0;
  String host = "api.thingspeak.com";
  String antwort = " ";
  boolean resp;
  
  for (int i = 1; i < 10; i++){
    resp = false;
    String track = "";
    String substr = "";
    String cmd = "/update?api_key=" + String("CUUVLGMC41X98FZ5"); // WLAN thingspeak update
    if (leftEar == true) { leftVolume = incVolume; track = (String("/") + String("track00")); }
    else { rightVolume = incVolume; track = (String("/") + String("track01")); }
    track = String(track + String(i) + ".mp3");
    const char *myTrack = track.c_str();
    myPlayer.setVolume(leftVolume, rightVolume);
    myPlayer.playFullFile(myTrack); // Frequenz abspielen
    if(softSerial.available())
    {
      read1 = softSerial.read();
      if (read1){
        substr = "";
        resp = true;
        counter ++;
        if (counter == 1)
        incVolume = (incVolume - 100) * (-1);
        hz = aba[i-1];
      //  myPlayer.stopPlaying();
        if (leftEar == true && counter == 1){ // leftEar -> first of customer
          substr = String("&field3=" + String(1)
                         + "&field4=" + String(1));
        } else if (leftEar == false && counter == 1){ // rightEar -> new ear of customer
          substr = String("&field3=" + String(1));
        }
        cmd = cmd                               // APIkey & update
              + String("&field1=" + String(hz) + "&field2=" + String(incVolume)) // always these fields
              + substr + "\n\r";                // new customer / new ear
              
        { //Block------------------------------ sende Daten an Thingspeak (mit http GET)
          Serial.println("\nThingspeak update ");
          Serial.print(" Dezibel: ");
          Serial.print(incVolume);
          Serial.print("cmd: ");
          Serial.println(cmd);
          if (WiFi.status() == WL_CONNECTED)
          httpGET(host, cmd, antwort, 80); // und absenden
        } // Blockende
        Serial.print("update done");
        Serial.println();
        incVolume = 100;
        delay( 15000 );
      }
      
      }
      if (resp == false){
        if (incVolume > 40) { 
          incVolume -= 10;
          i -= 1;
        } else incVolume = 100; // nächste Frequenz
      }
    }
}
