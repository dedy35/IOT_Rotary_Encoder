#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESP8266WebServer.h>
//#include <RTClib.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include "../lib/MQTT/PubSubClient.h"
//#include "../lib/HLW8012/HLW8012.h"
#include <WiFiUdp.h>    //tambahan untuk arduionOTA
#include <ArduinoOTA.h> //tambahan untuk arduinoOTA

// include library button
//#include <ezButton.h>

// Include Sensor
#include "Sensor/DigitalSensor/DigitalSensor.h"
//#include "Sensor/EnergySensor/EnergySensor.h"
//------------------------------------------------------------------------------

// Include Aktuator
#include "Aktuator/Led/Led.h"
//#include "Aktuator/Relay/Switch/Switch.h"
//------------------------------------------------------------------------------

// Include Pengaturan Perangkat
#include "PengaturanPerangkat/EEPROMData/EEPROMData.h"
#include "PengaturanPerangkat/HTMLForm/HTMLForm.h"
//------------------------------------------------------------------------------
#include "FirmwareInformation/FirmwareInformation.h"

#define UPDATE_TIME 5000

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
//LED
const int ledR = 15;
const int ledG = 2;
//rotary encoder
#define outputA 12
#define outputB 13
int encoder0PinALast = LOW;
int n = LOW;
int aState;
int aLastState;
float counterRE = 0;
int counter = 0;
float count = 0;
int counterx;
int prestate = 0;
int prestate2 = 0;

int time_H;
int time_L;
float t_period;
float frequency;

// Set timer untuk lama waktu tekan
const int start_opsi_Satu = 300;
const int stop_opsi_Satu = 1999;
const int start_opsi_Dua = 2000;
const int stop_opsi_Dua = 3500;
const int start_opsi_Tiga = 300;
const int stop_opsi_Tiga = 800;
const int start_opsi_Empat = 300;
const int stop_opsi_Empat = 1000;

int lastState = LOW;
int lastState2 = LOW;
int lastState3 = LOW;
int currentState = LOW;
int currentState2 = LOW;
int currentState3 = LOW;
unsigned long waktuTekan = 0;
unsigned long waktuLepas = 0;
unsigned long waktuLepas2 = 0;
unsigned long waktuTekan2 = 0;
unsigned long waktuTekan3 = 0;
unsigned long waktuLepas3 = 0;


int buttonState = 0;
int countpb = 0;
//int count = 0;

// konfurasi wemos
String ap_ssid = "honicel";
String chip_id, prefix, ip_device, nama_alat, t_chipid, t_ip_addr, t_nama_alat, t_count, t_status;
int interval;

const char *ap_password = "";
ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient MQTT(espClient);

HTMLForm form;
EEPROMData pengaturan;

Led led(LED_BUILTIN);
DigitalSensor jumper(16);
DigitalSensor jumper2(14);
DigitalSensor jumper3(0);
//Switch sw;
//ezButton button(16);

String session;

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

// Web Server
String generateSession()
{
  /*
  Spesifikasi :
  - Fungsi ini digunakan untuk membuat nama session secara random dengan panjang 100 digit.
  - Keluaran fungsi berupa objek kelas String.
  */

  String char_set = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";
  String str = "HONICEL_AUTOMATION_SESSION=";
  for (int i = 0; i < 10; i++)
  {
    str += char_set[random(char_set.length())];
  }
  return str;
}

bool is_authentified()
{
  /*
  Spesifikasi :
  - Fungsi ini digunakan untuk mengecek apakah pengguna telah login atau telah logout.
  - Keluaran fungsi berupa tipe data boolean.
  - true = apabila user telah login.
  - false = apabila user telah logout.
  */

  Serial.println("Enter is_authentified");
  if (server.hasHeader("Cookie"))
  {
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
    if (cookie.indexOf(session) != -1)
    {
      Serial.println("Authentification Successful");
      return true;
    }
  }
  Serial.println("Authentification Failed");
  return false;
}

void handleLogin()
{
  /*
  Spesifikasi :
  - Apabila pengguna belum login server akan memanggil prosedur ini, sehingga pengguna
    diminta untuk login terlebih dahulu sebelum masuk ke halaman pengaturan.
  */

  String message = "";
  if (server.hasHeader("Cookie"))
  {
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
  }
  if (server.hasArg("disconnect"))
  {
    session = generateSession();
    Serial.println("Disconnection");
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(301);
    return;
  }
  if (server.hasArg(form.getArg(ARG_USERNAME)) && server.hasArg(form.getArg(ARG_PASSWORD)))
  {
    if ((server.arg(form.getArg(ARG_USERNAME)) == pengaturan.readUsername()) && (server.arg(form.getArg(ARG_PASSWORD)) == pengaturan.readPassword()))
    {
      session = generateSession();
      server.sendHeader("Location", "/");
      server.sendHeader("Cache-Control", "no-cache");
      server.sendHeader("Set-Cookie", session);
      server.send(301);
      Serial.println("Log in Successful");
      return;
    }
    message = "Username atau password salah.";
    Serial.println("Log in Failed");
  }
  server.send(200, "text/html", form.login("/login", message));
}

void handleKoneksi()
{
  /*
  Spesifikasi :
  - Apabila pengguna telah login server membuka halaman / server akan memanggil prosedur ini,
    sehingga pengguna dapat masuk ke halaman pengaturan koneksi.
  - Apabila pengguna belum login server akan memanggil prosedur handleLogin(),
    sehingga pengguna diminta untuk login terlebih dahulu sebelum masuk ke halaman ini.
  */

  String message = "";
  if (!is_authentified())
  {
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(301);
    return;
  }

  if (server.hasArg(form.getArg(ARG_WIFI_SSID)) && server.hasArg(form.getArg(ARG_WIFI_PASSWORD)) && server.hasArg(form.getArg(ARG_MQTT_BROKER)) && server.hasArg(form.getArg(ARG_DEVICE_NAME)) && server.hasArg(form.getArg(ARG_PREFIX)) && server.hasArg(form.getArg(ARG_INTERVAL)))
  {
    String error_message = "";
    if (server.arg(form.getArg(ARG_WIFI_SSID)) == "")
    {
      error_message += "Wi-Fi SSID tidak boleh kosong.\\n";
    }

    if (server.arg(form.getArg(ARG_WIFI_PASSWORD)) == "")
    {
      error_message += "Wi-Fi Password tidak boleh kosong.\\n";
    }

    if (server.arg(form.getArg(ARG_MQTT_BROKER)) == "")
    {
      error_message += "MQTT Broker tidak boleh kosong.\\n";
    }

    if (server.arg(form.getArg(ARG_DEVICE_NAME)) == "")
    {
      error_message += "Nomor perangkat tidak boleh kosong.\\n";
    }

    if (server.arg(form.getArg(ARG_PREFIX)) == "")
    {
      error_message += "Prefix tidak boleh kosong.\\n";
    }

    if (server.arg(form.getArg(ARG_INTERVAL)) == "")
    {
      error_message += "Interval tidak boleh kosong.\\n";
    }

    if (error_message == "")
    {
      if (pengaturan.readWifiSSID() != server.arg(form.getArg(ARG_WIFI_SSID)))
      {
        pengaturan.writeWifiSSID(server.arg(form.getArg(ARG_WIFI_SSID)));
      }

      if (pengaturan.readWifiPassword() != server.arg(form.getArg(ARG_WIFI_PASSWORD)))
      {
        pengaturan.writeWifiPassword(server.arg(form.getArg(ARG_WIFI_PASSWORD)));
      }

      if (pengaturan.readMQTTBroker() != server.arg(form.getArg(ARG_MQTT_BROKER)))
      {
        pengaturan.writeMQTTBroker(server.arg(form.getArg(ARG_MQTT_BROKER)));
      }

      if (pengaturan.readDeviceName() != server.arg(form.getArg(ARG_DEVICE_NAME)))
      {
        pengaturan.writeDeviceName(server.arg(form.getArg(ARG_DEVICE_NAME)));
      }

      if (pengaturan.readPrefix() != server.arg(form.getArg(ARG_PREFIX)))
      {
        pengaturan.writePrefix(server.arg(form.getArg(ARG_PREFIX)));
      }

      if (pengaturan.readInterval() != server.arg(form.getArg(ARG_INTERVAL)))
      {
        pengaturan.writeInterval(server.arg(form.getArg(ARG_INTERVAL)));
      }

      // if (pengaturan.readNamaHeader() != server.arg(form.getArg(ARG_NAMA_HEADER)))
      // {
      //   pengaturan.writeNamaHeader(server.arg(form.getArg(ARG_NAMA_HEADER)));
      // }
      // if (pengaturan.readCodeHeader1() != server.arg(form.getArg(ARG_CODE_HEADER1)))
      // {
      //   pengaturan.writeCodeHeader1(server.arg(form.getArg(ARG_CODE_HEADER1)));
      // }
      // if (pengaturan.readCodeHeader2() != server.arg(form.getArg(ARG_CODE_HEADER2)))
      // {
      //   pengaturan.writeCodeHeader2(server.arg(form.getArg(ARG_CODE_HEADER2)));
      // }
      // if (pengaturan.readCodeHeader3() != server.arg(form.getArg(ARG_CODE_HEADER3)))
      // {
      //   pengaturan.writeCodeHeader3(server.arg(form.getArg(ARG_CODE_HEADER3)));
      // }
      // if (pengaturan.readCodeHeader4() != server.arg(form.getArg(ARG_CODE_HEADER4)))
      // {
      //   pengaturan.writeCodeHeader4(server.arg(form.getArg(ARG_CODE_HEADER4)));
      // }
      // if (pengaturan.readCodeHeader5() != server.arg(form.getArg(ARG_CODE_HEADER5)))
      // {
      //   pengaturan.writeCodeHeader5(server.arg(form.getArg(ARG_CODE_HEADER5)));
      // }
      // if (pengaturan.readCodeHeader6() != server.arg(form.getArg(ARG_CODE_HEADER6)))
      // {
      //   pengaturan.writeCodeHeader6(server.arg(form.getArg(ARG_CODE_HEADER6)));
      // }
      // if (pengaturan.readCodeHeader7() != server.arg(form.getArg(ARG_CODE_HEADER7)))
      // {
      //   pengaturan.writeCodeHeader7(server.arg(form.getArg(ARG_CODE_HEADER7)));
      // }
      // if (pengaturan.readCodeHeader8() != server.arg(form.getArg(ARG_CODE_HEADER8)))
      // {
      //   pengaturan.writeCodeHeader8(server.arg(form.getArg(ARG_CODE_HEADER8)));
      // }
      if (pengaturan.readChipIDD() != server.arg(form.getArg(ARG_CHIP_IDD)))
      {
        pengaturan.writeChipIDD(server.arg(form.getArg(ARG_CHIP_IDD)));
      }

      message = "Pengaturan koneksi telah tersimpan.";
    }
    else
    {
      message = error_message;
    }
  }
  server.send(200, "text/html", form.pengaturanKoneksi("/", message, "/", "/pengguna", "/firmware", "/login?disconnect=1", pengaturan.readWifiSSID(), pengaturan.readWifiPassword(), pengaturan.readMQTTBroker(), pengaturan.readDeviceName(), pengaturan.readPrefix(), pengaturan.readInterval(), pengaturan.readNamaHeader(), pengaturan.readCodeHeader1(), pengaturan.readCodeHeader2(), pengaturan.readCodeHeader3(), pengaturan.readCodeHeader4(), pengaturan.readCodeHeader5(), pengaturan.readCodeHeader6(), pengaturan.readCodeHeader7(), pengaturan.readCodeHeader8(), pengaturan.readChipIDD()));
}

void handlePengguna()
{
  /*
  Spesifikasi :
  - Apabila pengguna telah login server membuka halaman /pengguna server akan memanggil prosedur ini,
    sehingga pengguna dapat masuk ke halaman pengaturan pengguna.
  - Apabila pengguna belum login server akan memanggil prosedur handleLogin(),
    sehingga pengguna diminta untuk login terlebih dahulu sebelum masuk ke halaman ini.
  */

  String message = "";
  if (!is_authentified())
  {
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(301);
    return;
  }

  if (server.hasArg(form.getArg(ARG_USERNAME)) && server.hasArg(form.getArg(ARG_PASSWORD)) && server.hasArg(form.getArg(ARG_KONFIRMASI_PASSWORD)))
  {
    String error_message = "";
    if (server.arg(form.getArg(ARG_USERNAME)) == "")
    {
      error_message += "Username tidak boleh kosong.\\n";
    }

    if (server.arg(form.getArg(ARG_PASSWORD)) == "")
    {
      error_message += "Password tidak boleh kosong.\\n";
    }

    if (server.arg(form.getArg(ARG_KONFIRMASI_PASSWORD)) == "")
    {
      error_message += "Konfirmasi password tidak boleh kosong.\\n";
    }

    if (server.arg(form.getArg(ARG_PASSWORD)) != server.arg(form.getArg(ARG_KONFIRMASI_PASSWORD)))
    {
      error_message += "Konfirmasi password tidak sesuai.\\n";
    }

    if (error_message == "")
    {
      if (pengaturan.readUsername() != server.arg(form.getArg(ARG_USERNAME)))
      {
        pengaturan.writeUsername(server.arg(form.getArg(ARG_USERNAME)));
      }

      if (pengaturan.readPassword() != server.arg(form.getArg(ARG_PASSWORD)))
      {
        pengaturan.writePassword(server.arg(form.getArg(ARG_PASSWORD)));
      }

      message = "Pengaturan pengguna telah tersimpan.";
    }
    else
    {
      message = error_message;
    }
  }
  server.send(200, "text/html", form.pengaturanPengguna("/pengguna", message, "/", "/pengguna", "/firmware", "/login?disconnect=1", pengaturan.readUsername(), pengaturan.readPassword(), pengaturan.readPassword()));
}

void handleFirmware()
{
  /*
  Spesifikasi :
  - Apabila pengguna telah login server membuka halaman /pengguna server akan memanggil prosedur ini,
    sehingga pengguna dapat masuk ke halaman informasi firmware.
  - Apabila pengguna belum login server akan memanggil prosedur handleLogin(),
    sehingga pengguna diminta untuk login terlebih dahulu sebelum masuk ke halaman ini.
  */

  String message = "";
  if (!is_authentified())
  {
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(301);
    return;
  }

  FIRMWARE_IN informasi_firmware;
  informasi_firmware.firmwareName = firmware_name;
  informasi_firmware.firmwareDescription = firmware_description;
  informasi_firmware.firmwareVersion = firmware_version;
  informasi_firmware.legalCopyright = legal_copyright;
  informasi_firmware.companyName = company_name;
  informasi_firmware.firmwareType = firmware_type;

  t_ip_addr = prefix;
  t_ip_addr += "/";
  t_ip_addr += firmware_name;
  t_ip_addr += "/";
  t_ip_addr += chip_id;
  t_ip_addr += "/ip_addr";

  t_nama_alat = prefix;
  t_nama_alat += "/";
  t_nama_alat += firmware_name;
  t_nama_alat += "/";
  t_nama_alat += chip_id;
  t_nama_alat += "/nama_alat";

  t_chipid = prefix;
  t_chipid += "/";
  t_chipid += firmware_name;
  t_chipid += "/";
  t_chipid += chip_id;
  t_chipid += "/chipid";

  t_count = prefix;
  t_count += "/";
  t_count += firmware_name;
  t_count += "/";
  t_count += chip_id;
  t_count += "/encoderlam";

  t_status = prefix;
  t_status += "/";
  t_status += firmware_name;
  t_status += "/";
  t_status += chip_id;
  t_status += "/status";

  String list_topic = "<br><br>List Topic MQTT : <br>";
  list_topic += t_ip_addr;
  list_topic += "<br>";
  list_topic += t_nama_alat;
  list_topic += "<br>";
  list_topic += t_chipid;
  list_topic += "<br>";
  list_topic += t_count;
  list_topic += "<br>";
  list_topic += t_status;
  server.send(200, "text/html", form.informasiFirmware(informasi_firmware, list_topic, "/", "/pengguna", "/firmware", "/login?disconnect=1"));
}

void handleNotFound()
{
  /*
  Spesifikasi :
  - Apabila pengguna membuka halaman selain /login, /, dan /pengaturan server akan menjalankan prosedur ini,
    sehingga akan muncul status "Halaman tida ditemukan." di web browser.
  */

  String message = "Halaman tidak ditemukan.\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}
//------------------------------------------------------------------------------

void setup_wifi()
{
  /*
  Spesifikasi :
  - Prosedur ini digunakan untuk keneksi perangkat ke Wi-Fi.
  */

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(pengaturan.readWifiSSID());
  WiFi.begin(pengaturan.readWifiSSID().c_str(), pengaturan.readWifiPassword().c_str());
  

  while (WiFi.status() != WL_CONNECTED)
  {
    if (jumper.read() == HIGH)
    {
      ESP.restart();
    }
    lcd.setCursor(0, 0);
    lcd.print("Connecting To");
    lcd.setCursor(0, 1);
    lcd.print(pengaturan.readWifiSSID());
    lcd.clear();
    Serial.print(".");
    digitalWrite(ledR, HIGH);
    digitalWrite(ledG, HIGH);
    delay(500);
    digitalWrite(ledR, LOW);
    digitalWrite(ledG, LOW);
    
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  ip_device = WiFi.localIP().toString();

  lcd.setCursor(0, 0);
  lcd.print("Connected To");
  lcd.setCursor(0, 1);
  lcd.print(pengaturan.readWifiSSID());
  delay(500);
  lcd.clear();
}

// void callback(char *topic, byte *payload, unsigned int length)
// {
//   /*
//   Spesifikasi :
//   - Prosedur ini akan terpanggil apabila ada perubahan topic yang telah di subscribe perangkat.
//   */

//   Serial.print("Message arrived [");
//   Serial.print(topic);
//   Serial.print("] ");

//   String _topic = topic;

//   for (unsigned int i = 0; i < length; i++)
//   {
//     Serial.print((char)payload[i]);
//   }
//   Serial.println();

//   if (_topic.indexOf("switch") != -1)
//   {
//     if ((char)payload[0] == '1')
//     {
//       sw.setClosedCircuit();
//       digitalWrite(relay, LOW);
//       digitalWrite(LED_BUILTIN, LOW);
//     }
//     else
//     {
//       sw.setOpenCircuit();
//       digitalWrite(relay, HIGH);
//       digitalWrite(LED_BUILTIN, HIGH);
//     }
//   }
// }

void reconnect()
{
  /*
  Spesifikasi :
  - Apabila koneksi MQTT Broker terputus perangkat akan memanggil prosedur ini.
  */

  while (!MQTT.connected())
  {
    if (jumper.read() == HIGH)
    {
      ESP.restart();
    }

    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if (MQTT.connect(clientId.c_str(), t_status.c_str(), 2, 1, "offline"))
    {
      Serial.println("connected");
      MQTT.publish(t_status.c_str(), "online", 1);
      //MQTT.subscribe(t_switch.c_str());
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(MQTT.state());
      Serial.println(" try again in 1 seconds");
    }

    // led.setOn();
    // delay(500);
    // led.setOff();
    // delay(500);
  }
}
//------------------------------------------------------------------------------

void setup()
{
  Serial.begin(115200);
  pinMode (outputA, INPUT_PULLUP);
  pinMode (outputB, INPUT_PULLUP);
  pinMode (0,INPUT_PULLUP);
  pinMode (ledR, OUTPUT);
  pinMode (ledG, OUTPUT);
  aLastState = digitalRead(outputA);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(5, 0);
  lcd.print("WELCOME");
  lcd.setCursor(1, 1);
  lcd.print("HONICEL - TEAM");
  delay(500);
  lcd.clear();

  // if (!rtc.begin())
  // {
  //   Serial.println("Couldn't find RTC");
  //   Serial.flush();
  //   while (1)
  //     delay(10);
  // }
  // if (rtc.lostPower())
  // {
  //   Serial.println("RTC lost power, let's set the time!");
  //   rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // }

  randomSeed(micros()); // Inisialisasi random
  //sw.setPin(D5);
  pengaturan.begin();

  /*
  Inisialisasi Username & Password
  Dilakukan ketika username & password belum di-set
  kondisi tersebut ketika menggunakan device baru yang memiliki data eeprom yg masih kosong.
  */
  // pengaturan.writeUsername("admin");
  // pengaturan.writePassword("admin");
  //---------------------------
  chip_id = String(ESP.getChipId());
  // chip_id = "10754670"; // yang lama rusak terpaksa dibuat fix dulu
  Serial.println(chip_id);
  ap_ssid += chip_id;
  prefix = pengaturan.readPrefix();
  interval = pengaturan.readInterval().toInt();
  nama_alat = pengaturan.readDeviceName();

  t_ip_addr = prefix;
  t_ip_addr += "/";
  t_ip_addr += firmware_name;
  t_ip_addr += "/";
  t_ip_addr += chip_id;
  t_ip_addr += "/ip_addr";

  t_nama_alat = prefix;
  t_nama_alat += "/";
  t_nama_alat += firmware_name;
  t_nama_alat += "/";
  t_nama_alat += chip_id;
  t_nama_alat += "/nama_alat";

  t_chipid = prefix;
  t_chipid += "/";
  t_chipid += firmware_name;
  t_chipid += "/";
  t_chipid += chip_id;
  t_chipid += "/chipid";

  t_count = prefix;
  t_count += "/";
  t_count += firmware_name;
  t_count += "/";
  t_count += chip_id;
  t_count += "/rotarylam";

  t_status = prefix;
  t_status += "/";
  t_status += firmware_name;
  t_status += "/";
  t_status += chip_id;
  t_status += "/status";

  Serial.println(t_ip_addr);
  Serial.println(t_nama_alat);
  Serial.println(t_chipid);
  Serial.println(t_count);
  Serial.println(t_status);

  if (jumper.read() == HIGH)
  {
    WiFi.softAP(ap_ssid.c_str(), ap_password);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("Please Setting");
    lcd.setCursor(0, 1);
    lcd.print("To : ");
    lcd.print(myIP);
    session = generateSession();
    server.on("/", handleKoneksi);
    server.on("/pengguna", handlePengguna);
    server.on("/firmware", handleFirmware);
    server.on("/login", handleLogin);
    server.onNotFound(handleNotFound);

    const char *headerkeys[] = {"User-Agent", "Cookie"};
    size_t headerkeyssize = sizeof(headerkeys) / sizeof(char *);
    server.collectHeaders(headerkeys, headerkeyssize);
    server.begin();
  }
  else
  {
    setup_wifi();
    String broker = pengaturan.readMQTTBroker();
    Serial.println(broker.c_str());
    MQTT.setServer(strdup(broker.c_str()), 1883);
    //MQTT.setCallback(callback);

    /*
    // memunculkan laman web di normal operation
    session = generateSession();
    server.on("/", handleKoneksi);
    server.on("/pengguna", handlePengguna);
    server.on("/firmware", handleFirmware);
    server.on("/login", handleLogin);
    server.onNotFound(handleNotFound);
    const char *headerkeys[] = {"User-Agent", "Cookie"};
    size_t headerkeyssize = sizeof(headerkeys) / sizeof(char *);
    server.collectHeaders(headerkeys, headerkeyssize);
    server.begin();
    // selesai memunculkan laman web di normal operation
    */
  }
  led.blink();

  /*
  setting untuk OTA
  */
  ArduinoOTA.onStart([]()
                     {
                       String type;
                       if (ArduinoOTA.getCommand() == U_FLASH)
                       {
                         type = "sketch";
                       }
                       else
                       { // U_FS
                         type = "filesystem";
                       }
                       // NOTE: if updating FS this would be the place to unmount FS using FS.end()
                       Serial.println("Start updating " + type); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
                       Serial.printf("Error[%u]: ", error);
                       if (error == OTA_AUTH_ERROR)
                       {
                         Serial.println("Auth Failed");
                       }
                       else if (error == OTA_BEGIN_ERROR)
                       {
                         Serial.println("Begin Failed");
                       }
                       else if (error == OTA_CONNECT_ERROR)
                       {
                         Serial.println("Connect Failed");
                       }
                       else if (error == OTA_RECEIVE_ERROR)
                       {
                         Serial.println("Receive Failed");
                       }
                       else if (error == OTA_END_ERROR)
                       {
                         Serial.println("End Failed");
                       } });
  ArduinoOTA.begin();
}

//int counter = 0;

void loop()
{
  // Arduino OTA Update
  ArduinoOTA.handle();
  if (countpb == 2)
  {
    
    n = digitalRead(outputA);
    if((encoder0PinALast == LOW) && (n = HIGH)){
      if(digitalRead(outputB)== LOW){
        //counterRE = counterRE + 0;
        //Serial.print(" CW, ");
                counterRE++;

      }
      else{
        counterRE++;
        Serial.print(" CCW, ");
      }
      counterx = counterRE;
      Serial.print("Position: ");
      Serial.println(counterx);
      
      lcd.setCursor(0,0);
      lcd.print("STEP=");
      lcd.print(counterx);
      lcd.print("        ");
      if(counterx == 0)
      {
        count = 0 ;
        lcd.setCursor(0,1);
        lcd.print("P=");
        lcd.print(count/100, 2);
        lcd.setCursor(6,1);
        lcd.print("        ");
        lcd.setCursor(14,1);
        lcd.print(" M");
      }
      else if(counterx % 90 == 0)
      {
        count += 100;
        lcd.setCursor(0,1);
        lcd.print("P=");
        lcd.print(count/100, 2);
        lcd.setCursor(10,1);
        lcd.print(" M");
         lcd.setCursor(14,0);
    lcd.print("pb2:");
    lcd.setCursor(15,1);
    lcd.print(countpb);
      }
     time_H = pulseIn(outputA,HIGH);
     //time_L = pulseIn(outputB,LOW);     
     //t_period = time_H+time_L;
     //t_period = t_period/100;
     //frequency = 100/t_period;
     //Serial.print(" F = ");
     //Serial.print(frequency); 
     //Serial.println(" Hz "); 
     delay(50);
    } 
    aLastState = aState;
    encoder0PinALast = n;
  }
  currentState2 = jumper2.read();
  if(currentState2 == HIGH && prestate == 0)
  {
    countpb++;
    lcd.setCursor(0,1);
    lcd.print("Button2:");
    lcd.setCursor(8,1);
    lcd.print(countpb);
    delay(500);
    prestate = 1;
  }
  else if(currentState2 == LOW)
  {
    prestate = 0;
  }
  currentState3 = jumper3.read();
  if(currentState3 == HIGH && prestate2 == 0)
  {
    if(countpb == 1 || countpb == 0)
    {
      lcd.setCursor(0,0);
      lcd.print("start klik pb2");
    }
    else if(countpb == 3)
    {
        String chip_idd = String(ESP.getChipId());
        MQTT.publish(t_chipid.c_str(), String(chip_idd).c_str());
        MQTT.publish(t_ip_addr.c_str(), ip_device.c_str());
        MQTT.publish(t_nama_alat.c_str(), nama_alat.c_str());
        MQTT.publish(t_count.c_str(), String(count).c_str());
        
        char buffer[256];
        const int capacity =JSON_ARRAY_SIZE(10) + 2 * JSON_OBJECT_SIZE(10);
        StaticJsonDocument<capacity> doc;
        JsonObject obj = doc.createNestedObject();
        obj["chip_id"] = pengaturan.readChipIDD();
        obj["mesin"] = pengaturan.readDeviceName();
        obj["nilai"] = count/100,2;
        // JsonArray nilai = obj.createNestedArray("nilai");
        // nilai.add(count);
        size_t n = serializeJson(doc, buffer);
        //http.end();
        ++value;
        snprintf(msg, MSG_BUFFER_SIZE, "%ld", value);
        Serial.print("Publish message: ");
        Serial.println(n);
        //MQTT.publish(t_count.c_str(), buffer, n);
        MQTT.publish("mgdm/test", buffer, n);
        Serial.print("sent :");
        Serial.println(count/100,2);
         digitalWrite(ledR, HIGH);
         delay(200);
         digitalWrite(ledR, LOW);
         lcd.clear();
         lcd.setCursor(0,0);
        lcd.print("start klik pb2");
        lcd.setCursor(0,1);
        lcd.print("sent:");
        lcd.setCursor(5,1);
        lcd.print(count);
        lcd.setCursor(14,1);
        lcd.print("M");
        count = 0;
        counterx = 0;
        counterRE = 0;
        countpb = 1;
    }
    else if(countpb >= 4)
    {
      countpb = 1;
    }
    prestate2 = 1;
  }
   else if(currentState3 == LOW)
  {
    prestate2 = 0;
  }

  static unsigned long last = millis();
  if ((millis() - last) >= UPDATE_TIME)
      {
        // String chip_idd = String(ESP.getChipId());
        // MQTT.publish(t_chipid.c_str(), String(chip_idd).c_str());
        // MQTT.publish(t_ip_addr.c_str(), ip_device.c_str());
        // MQTT.publish(t_nama_alat.c_str(), nama_alat.c_str());
        // MQTT.publish(t_count.c_str(), String(count).c_str());

        // Serial.print("sent :");
        // Serial.println(count);
        
        // //kirim ke MQTT
        // char buffer[256];
        // const int capacity =JSON_ARRAY_SIZE(10) + 2 * JSON_OBJECT_SIZE(10);
        // StaticJsonDocument<capacity> doc;
        // JsonObject obj = doc.createNestedObject();
        // obj["chip_id"] = pengaturan.readChipIDD();
        // obj["mesin"] = pengaturan.readDeviceName();
        // obj["nilai"] = count;

        // // JsonArray nilai = obj.createNestedArray("nilai");
        // // nilai.add(count);

        // size_t n = serializeJson(doc, buffer);
        // ++value;
        // snprintf(msg, MSG_BUFFER_SIZE, "%ld", value);
        // Serial.print("Publish message: ");
        // Serial.println(n);
        // MQTT.publish(t_count.c_str(), buffer, n);

        // // print data ke lcd

        counter = 0;
        last=millis();
        // digitalWrite(ledK,HIGH);
        // delay(200);
        // digitalWrite(ledK, LOW);
      }

  if (jumper.read() == LOW)
  {
    server.handleClient();
    // Jika MQTT tidak Terkoneksi
    if (!MQTT.connected())
    {
      // ESP.restart();
      reconnect();
    }
    
    // if (counter < interval)
    // {
    //   counter++;
    // }
    // else
    // {
      
      
    // }
    MQTT.loop();
    delay(1);
  }
  else
  {
    server.handleClient();
  }

  //Untuk melakukan konfigurasi dan restart nodemcu
  currentState = jumper.read();
  if (lastState == LOW && currentState == HIGH)
  {
    waktuTekan = millis();
  }
  else if (lastState == HIGH && currentState == LOW)
  {
    waktuLepas = millis();
    long lamaTekan = waktuLepas - waktuTekan;
    Serial.print("Press :");
    Serial.println(lamaTekan);
    //lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Lama Tekan:");
    lcd.setCursor(11, 1);
    lcd.print(lamaTekan);
    delay(300);
    lcd.setCursor(11, 1);
    lcd.print("     ");
    // if (jumper.read() == HIGH)
    // if (lamaTekan >= start_opsi_Satu && lamaTekan <= stop_opsi_Satu)
    // {
    //   String chip_idd = String(ESP.getChipId());
    //     MQTT.publish(t_chipid.c_str(), String(chip_idd).c_str());
    //     MQTT.publish(t_ip_addr.c_str(), ip_device.c_str());
    //     MQTT.publish(t_nama_alat.c_str(), nama_alat.c_str());
    //     MQTT.publish(t_count.c_str(), String(count).c_str());

    //     Serial.print("sent :");
    //     Serial.println(count);
    // }
    if (lamaTekan >= start_opsi_Dua && lamaTekan <= stop_opsi_Dua)
    {
      if (!MQTT.connected())
      {
        reconnect();
      }
      WiFi.softAP(ap_ssid.c_str(), ap_password);
      IPAddress myIP = WiFi.softAPIP();
      Serial.print("AP IP address: ");
      Serial.println(myIP);
      lcd.clear();
      lcd.setCursor(1, 0);
      lcd.print("Setting in 20S");
      lcd.setCursor(0, 1);
      lcd.print("To : ");
      lcd.print(myIP);
      session = generateSession();
      server.on("/", handleKoneksi);
      server.on("/pengguna", handlePengguna);
      server.on("/firmware", handleFirmware);
      server.on("/login", handleLogin);
      server.onNotFound(handleNotFound);

      const char *headerkeys[] = {"User-Agent", "Cookie"};
      size_t headerkeyssize = sizeof(headerkeys) / sizeof(char *);
      server.collectHeaders(headerkeys, headerkeyssize);
      server.begin();
      // server.handleClient();
      // if(!digitalRead(pinIR1) && state)
      // {
      //   count++;
      //   state = false;
      //   Serial.print("Count: ");
      //   Serial.println(count);
      // }
      // if(digitalRead(pinIR1))
      // {
      //   state = true;
      // }
      delay(300);
      lcd.clear();
    }
    if (lamaTekan >= start_opsi_Tiga && lamaTekan <= stop_opsi_Tiga)
    {
     ESP.restart(); 
    }
  }
  lastState = currentState;

  // //untuk melakukan pengiriman manual
  // currentState2 = jumper2.read();
  // if (lastState2 == LOW && currentState2 == HIGH)
  // {
  //   waktuTekan2 = millis();
  // }
  // else if (lastState2 == HIGH && currentState2 == LOW)
  // {
  //   waktuLepas2 = millis();
  //   long lamaTekan2 = waktuLepas2 - waktuTekan2;
  //   Serial.print("Press :");
  //   Serial.println(lamaTekan2);
  //   //lcd.clear();
  //   lcd.setCursor(0, 1);
  //   lcd.print("Lama Tekan:");
  //   lcd.setCursor(11, 1);
  //   lcd.print(lamaTekan2);
  //   delay(300);
  //   lcd.setCursor(11, 1);
  //   lcd.print("     ");
  //   if (lamaTekan2 >= start_opsi_Satu && lamaTekan2 <= stop_opsi_Satu)
  //   {
  //     String chip_idd = String(ESP.getChipId());
  //       MQTT.publish(t_chipid.c_str(), String(chip_idd).c_str());
  //       MQTT.publish(t_ip_addr.c_str(), ip_device.c_str());
  //       MQTT.publish(t_nama_alat.c_str(), nama_alat.c_str());
  //       MQTT.publish(t_count.c_str(), String(count).c_str());
        
  //       char buffer[256];
  //       const int capacity =JSON_ARRAY_SIZE(10) + 2 * JSON_OBJECT_SIZE(10);
  //       StaticJsonDocument<capacity> doc;
  //       JsonObject obj = doc.createNestedObject();
  //       obj["chip_id"] = pengaturan.readChipIDD();
  //       obj["mesin"] = pengaturan.readDeviceName();
  //       obj["nilai"] = count/100,2;
  //       // JsonArray nilai = obj.createNestedArray("nilai");
  //       // nilai.add(count);
  //       size_t n = serializeJson(doc, buffer);
  //       //http.end();
  //       ++value;
  //       snprintf(msg, MSG_BUFFER_SIZE, "%ld", value);
  //       Serial.print("Publish message: ");
  //       Serial.println(n);
  //       //MQTT.publish(t_count.c_str(), buffer, n);
  //       MQTT.publish("mgdm/test", buffer, n);
  //       Serial.print("sent :");
  //       Serial.println(count/100,2);
  //        digitalWrite(ledR, HIGH);
  //        delay(200);
  //        digitalWrite(ledR, LOW);
  //       count = 0;
  //       counterx = 0;
  //       counterRE = 0;
  //       lcd.clear();
  //       lcd.setCursor(0,0);
  //       lcd.print("Count: ");
  //       lcd.setCursor(7,0);
  //       lcd.print(count);
  //   }  
  // }
  // lastState2 = currentState2;

  // //melakukan start
  // //untuk melakukan pengiriman manual
  // currentState3 = jumper3.read();
  // if (lastState3 == LOW && currentState3 == HIGH)
  // {
  //   waktuTekan3 = millis();
  // }
  // else if (lastState3 == HIGH && currentState3 == LOW)
  // {
  //   waktuLepas3 = millis();
  //   long lamaTekan3 = waktuLepas3 - waktuTekan3;
  //   Serial.print("Press :");
  //   Serial.println(lamaTekan3);
  //   //lcd.clear();
  //   lcd.setCursor(0, 1);
  //   lcd.print("Lama Tekan:");
  //   lcd.setCursor(11, 1);
  //   lcd.print(lamaTekan3);
  //   delay(300);
  //   lcd.setCursor(0, 1);
  //   lcd.print("                    ");
  //   lcd.setCursor(11, 1);
  //   lcd.print("                    ");
  //   lcd.setCursor(0,0);
  //   lcd.print("                    ");
  //   if (lamaTekan3 >= start_opsi_Empat)
  //   {
  //     countpb++;
  //     Serial.println(countpb);
  //     // if(countpb % 2 == 1){
  //     //   lcd.clear();
  //     // }
  //     if(countpb == 2){
  //       lcd.setCursor(0,0);
  //       lcd.print("Untuk Start Tekan");
  //       lcd.setCursor(0,1);
  //       lcd.print("Push Button 3");   
  //     }
      
  //   }
    
  // }
  // lastState3 = currentState3;
}