// CavaWiFi Version 1.7 (Migrado a ESP32 + Bluetooth Scan)
// Author Juan Maioli
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <time.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

struct WifiNetwork {
  String ssid;
  int32_t rssi;
  wifi_auth_mode_t encryptionType;
};

// --- Variables Globales ---
const char *host = "ifconfig.me";
const char* hostname_prefix = "Esp32-";
String serial_number;
String id_Esp;
String localIP;
String publicIP = "Obteniendo...";
String cavaData = "Cargando...";
String wifiNetworksList = "Escaneando...";
String bluetoothDevicesList = "Escaneando..."; // Nueva variable para BT
String lastWifiScanTime = "Nunca";
String lastBluetoothScanTime = "Nunca"; // Nueva variable de tiempo BT
String downloadSpeed = "No medido";
String lastSpeedTestTime = "Nunca";

// --- Objetos Globales ---
WebServer server(3000);
BLEScan* pBLEScan; // Puntero para el escáner BLE

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600;
const int daylightOffset_sec = 0;

unsigned long previousTimeUpdate = 0;
unsigned long previousIpUpdate = 0;
const long timeInterval = 60000;      
const long ipInterval = 1740000;

// --- Funciones Auxiliares ---

String getFormattedTime() {
  time_t now = time(nullptr);
  if (now < 8 * 3600 * 2) return "No sincronizada";
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buffer[80];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

String getFormattedDate() {
  time_t now = time(nullptr);
  if (now < 8 * 3600 * 2) return "Fecha no sincronizada";
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buffer[80];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
  return String(buffer);
}

String leftRepCadena(String mac) {
  mac.replace(":", "");
  mac = mac.substring(mac.length() - 4);
  return mac;
}

String getUniqueId() {
  uint64_t mac = ESP.getEfuseMac();
  return String((uint32_t)(mac >> 32), HEX) + String((uint32_t)mac, HEX);
}

void sortNetworks(WifiNetwork *networks, int count) {
  for (int i = 0; i < count - 1; i++) {
    for (int j = 0; j < count - i - 1; j++) {
      if (networks[j].rssi < networks[j + 1].rssi) {
        WifiNetwork temp = networks[j];
        networks[j] = networks[j + 1];
        networks[j + 1] = temp;
      }
    }
  }
}

String scanWifiNetworks() {
  int n = WiFi.scanNetworks();
  if (n == 0) return "<p>No se encontraron redes.</p>";
  
  WifiNetwork *networks = new WifiNetwork[n];
  for (int i = 0; i < n; ++i) {
    networks[i].ssid = WiFi.SSID(i);
    networks[i].rssi = WiFi.RSSI(i);
    networks[i].encryptionType = WiFi.encryptionType(i);
  }
  sortNetworks(networks, n);

  String list = "";
  for (int i = 0; i < n; ++i) {
    list += "<p>";
    list += (networks[i].encryptionType == WIFI_AUTH_OPEN) ? "\xF0\x9F\x8C\x90 " : "\xF0\x9F\x94\x92 ";
    list += networks[i].ssid;
    list += " (";
    list += networks[i].rssi;
    list += " dBm)";
    list += "</p>";
  }
  delete[] networks;
  return list;
}

String scanBluetoothDevices() {
  // Escaneo de 5 segundos - start devuelve un puntero
  BLEScanResults *foundDevices = pBLEScan->start(5, false);
  int count = foundDevices->getCount();
  String list = "";
  
  if (count == 0) {
    list = "<p>No se encontraron dispositivos BLE.</p>";
  } else {
    for (int i = 0; i < count; i++) {
      BLEAdvertisedDevice device = foundDevices->getDevice(i);
      list += "<p>\xF0\x9F\x94\xB5 "; // Emoji 🔵
      String devName = device.getName().c_str();
      if (devName.length() == 0) devName = "Dispositivo Desconocido";
      list += devName;
      list += " (";
      list += device.getRSSI();
      list += " dBm)";
      list += "</p>";
    }
  }
  pBLEScan->clearResults(); // Limpiar memoria del escaneo
  return list;
}

void getPublicIP() {
  WiFiClientSecure client;
  client.setInsecure();
  if (!client.connect(host, 443)) return;

  client.print(String("GET /ip HTTP/1.1\r\n") + "Host: " + host + "\r\n" + "User-Agent: ESP32-IP-Checker\r\n" + "Connection: close\r\n\r\n");
  if (client.find("\r\n\r\n")) {
    String newPublicIP = client.readStringUntil('\n');
    newPublicIP.trim();
    if (newPublicIP.length() > 0) publicIP = newPublicIP;
  }
}

void testDownloadSpeed() {
  Serial.println("[" + getFormattedTime() + "] [INFO] Iniciando prueba de velocidad...");
  HTTPClient http;
  WiFiClient client;
  const char* testUrl = "http://ipv4.download.thinkbroadband.com/5MB.zip";
  const float fileSizeMB = 5.0;

  if (http.begin(client, testUrl)) {
    http.setUserAgent("Mozilla/5.0 (ESP32; Compatible)");
    unsigned long startTime = millis();
    int httpCode = http.GET();
    if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
      int len = http.getSize();
      uint8_t buff[128] = { 0 };
      while (http.connected() && (len > 0 || len == -1)) {
        size_t size = client.readBytes(buff, sizeof(buff));
        if (size == 0) break;
        if (len > 0) len -= size;
      }
      unsigned long endTime = millis();
      float duration = (endTime - startTime) / 1000.0;
      float speedMbps = (fileSizeMB * 8) / duration;
      downloadSpeed = String(speedMbps, 2) + " Mbps";
      lastSpeedTestTime = getFormattedTime();
      Serial.println("[" + lastSpeedTestTime + "] [OK] Velocidad medida: " + downloadSpeed);
    } else {
      downloadSpeed = "Error: " + String(httpCode);
      Serial.println("[" + getFormattedTime() + "] [ERROR] Fallo Speedtest: " + downloadSpeed);
    }
    http.end();
  }
}

void updateNetworkData() {
  Serial.println("[" + getFormattedTime() + "] [INFO] Iniciando ciclo de actualizacion...");
  
  // 1. Datos IP y Cava
  localIP = WiFi.localIP().toString();
  getPublicIP();
  
  HTTPClient http;
  WiFiClient client;
  if (http.begin(client, "http://pikapp.com.ar/cava/txt/")) {
    int httpCode = http.GET();
    if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
      cavaData = http.getString();
    } else {
      cavaData = "Error Cava: " + String(httpCode);
    }
    http.end();
  }
  
  // 2. Escaneo WiFi
  Serial.println("[" + getFormattedTime() + "] [INFO] Escaneando WiFi...");
  wifiNetworksList = scanWifiNetworks();
  lastWifiScanTime = getFormattedTime();

  // 3. Escaneo Bluetooth
  Serial.println("[" + getFormattedTime() + "] [INFO] Escaneando Bluetooth...");
  bluetoothDevicesList = scanBluetoothDevices();
  lastBluetoothScanTime = getFormattedTime();
  
  Serial.println("[" + getFormattedTime() + "] [OK] Ciclo de actualizacion completado.");
}

// --- Handler para el Servidor Web ---
void handleRoot() {
    String formattedCavaData = cavaData;
    unsigned long totalSeconds = millis() / 1000;
    unsigned long days = totalSeconds / 86400;
    unsigned long hours = (totalSeconds % 86400) / 3600;
    unsigned long minutes = ((totalSeconds % 86400) % 3600) / 60;
    unsigned long seconds = ((totalSeconds % 86400) % 3600) % 60;
    String uptime = "";
    if (days > 0) uptime += String(days) + "d ";
    if (hours > 0 || days > 0) uptime += String(hours) + "h ";
    if (minutes > 0 || hours > 0 || days > 0) uptime += String(minutes) + "m ";
    uptime += String(seconds) + "s";
    if (uptime == "") uptime = "0s";

    String page = "<!DOCTYPE html><html lang='es'><head>";
    page += "<meta charset='UTF-8'>";
    page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    page += "<meta http-equiv='refresh' content='1200'>"; 
    page += "<title>Estado del Dispositivo (ESP32)</title>";
    page += "<style>";
    page += ":root { --bg-color: #f0f2f5; --container-bg: #ffffff; --text-primary: #1c1e21; --text-secondary: #4b4f56; --pre-bg: #f5f5f5; --hr-color: #e0e0e0; --dot-color: #bbb; --dot-active-color: #717171; }";
    page += "@media (prefers-color-scheme: dark) {";
    page += ":root { --bg-color: #121212; --container-bg: #1e1e1e; --text-primary: #e0e0e0; --text-secondary: #b0b3b8; --pre-bg: #2a2a2a; --hr-color: #3e4042; --dot-color: #555; --dot-active-color: #ccc; }";
    page += "}";
    page += "body { background-color: var(--bg-color); color: var(--text-secondary); font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 1rem 0;}";
    page += ".container { background-color: var(--container-bg); padding: 2rem; border-radius: 8px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); text-align: left; width: 400px; height: 80vh; position: relative; display: flex; flex-direction: column; }";
    page += "@media (max-width: 768px) { .container { max-width: 80%; width: auto; height: 80vh; } }";
    page += "h1, h2 { color: var(--text-primary); margin-bottom: 1rem; text-align: center; }";
    page += "p { color: var(--text-secondary); font-size: 1.1rem; margin: 0.5rem 0; }";
    page += "strong { color: var(--text-primary); }";
    page += "hr { border: 0; height: 1px; background-color: var(--hr-color); margin: 1.5rem 0; }";
    page += ".carousel-container { position: relative; flex-grow: 1; overflow: hidden; }";
    page += ".carousel-slide { display: none; height: 100%; width: 100%; flex-basis: 100%; flex-shrink: 0; overflow-y: auto; padding-right: 15px; box-sizing: border-box; word-wrap: break-word; }";
    page += ".fade { animation-name: fade; animation-duration: 0.5s; }";
    page += "@keyframes fade { from {opacity: .4} to {opacity: 1} }";
    page += ".prev, .next { cursor: pointer; position: absolute; top: 50%; transform: translateY(-50%); width: auto; padding: 16px; color: var(--text-primary); font-weight: bold; font-size: 24px; transition: 0.3s; user-select: none; z-index: 10; }";
    page += ".prev { left: -50px; }";
    page += ".next { right: -50px; }";
    page += ".prev:hover, .next:hover { background-color: rgba(0,0,0,0.2); border-radius: 50%; }";
    page += ".dots { text-align: center; padding-top: 20px; }";
    page += ".dot { cursor: pointer; height: 15px; width: 15px; margin: 0 2px; background-color: var(--dot-color); border-radius: 50%; display: inline-block; transition: background-color 0.3s ease; }";
    page += ".active, .dot:hover { background-color: var(--dot-active-color); }";
    page += ".emoji-container { text-align: center; margin-top: 15px; margin-bottom: 15px; }";
    page += ".emoji { font-size: 4em; line-height: 1; display: inline-block; vertical-align: middle; }";
    page += ".button { background-color: #4CAF50; color: white; padding: 10px 20px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 10px 0; cursor: pointer; border-radius: 5px; }";
    page += ".button:hover { background-color: #45a049; }";
    page += ".button[disabled] { background-color: #555; color: #eee; border: 1px solid #eeeeee; cursor: not-allowed; }";
    page += ".center-button { text-align: center; }";
    page += "@media (max-width: 768px) { .container { max-width: 80%; width: auto; height: 80vh; } .prev, .next { top: auto; bottom: 5px; transform: translateY(0); } .prev { left: 10px; } .next { right: 10px; } }";
    page += "</style></head><body><div class='container'>";
    
    // --- Navigation Buttons ---
    page += "<a class='prev' onclick='changeSlide(-1)'>&#10094;</a>";
    page += "<a class='next' onclick='changeSlide(1)'>&#10095;</a>";

    // --- Carousel ---
    page += "<div class='carousel-container'>";
    
    // --- Slide 1: Estado del Dispositivo ---
    page += "<div class='carousel-slide fade'>";
    page += "<h2>Estado del Dispositivo</h2>";
    page += "<div class='emoji-container'><span class='emoji'>📟</span></div><br>";
    page += "<h3><strong>📅 Fecha:</strong> " + getFormattedDate() + "<br>";
    page += "<strong>⌚ Hora:</strong> <span id='current-time'>" + getFormattedTime() + "</span><br>";
    page += "<strong>🖥️ Hostname:</strong> " + id_Esp + "<br>";
    page += "<strong>🏠 IP Privada:</strong> " + localIP + "<br>";
    page += "<strong>↔️ M&aacute;scara:</strong> " + WiFi.subnetMask().toString() + "<br>";
    page += "<strong>🚪 Gateway:</strong> " + WiFi.gatewayIP().toString() + "<br>";
    page += "<strong>🌐 IP P&uacute;blica:</strong> " + publicIP + "<br>";
    page += "<strong>📶 WiFi RSSI:</strong> " + String(WiFi.RSSI()) + " dBm<br>";
    page += "<strong>🆔 MAC:</strong> " + WiFi.macAddress() + "<br>";
    page += "<strong>💡 Chip ID:</strong> " + getUniqueId() + "<br>";
    page += "<strong>💾 Flash:</strong> " + String(ESP.getFlashChipSize() / 1024) + " KB<br>";
    page += "<strong>🧠 Free Heap:</strong> " + String(ESP.getFreeHeap() / 1024.0, 2) + " KB<br>";
    page += "<strong>⚡ Uptime:</strong> " + uptime + "</h3>";
    page += "</div>";

    // --- Slide 2: Datos del Clima ---
    page += "<div class='carousel-slide fade'>";
    page += "<h2>Datos del Clima</h2>";
    page += "<div><p>" + formattedCavaData + "</p></div>";
    page += "</div>";

    // --- Slide 3: Redes WiFi Cercanas ---
    page += "<div class='carousel-slide fade'>";
    page += "<h2>Redes WiFi Cercanas</h2>";
    page += "<div class='emoji-container'><span class='emoji'>📡</span></div><br>";
    page += "<p><strong>Escaneado:</strong> " + lastWifiScanTime + "</p>";
    page += wifiNetworksList;
    page += "</div>";

    // --- Slide 4: Bluetooth (NUEVA DIAPOSITIVA) ---
    page += "<div class='carousel-slide fade'>";
    page += "<h2>Bluetooth (BLE)</h2>";
    page += "<div class='emoji-container'><span class='emoji'>🦷</span></div><br>";
    page += "<p><strong>Escaneado:</strong> " + lastBluetoothScanTime + "</p>";
    page += bluetoothDevicesList;
    page += "</div>";

    // --- Slide 5: Prueba de Velocidad ---
    page += "<div class='carousel-slide fade'>";
    page += "<h2>Prueba de Velocidad</h2>";
    page += "<div class='emoji-container'><span class='emoji'>🚀</span></div><br>";
    page += "<p><strong>&Uacute;ltima prueba:</strong> " + lastSpeedTestTime + "</p>";
    page += "<p><strong>Velocidad de Descarga:</strong> " + downloadSpeed + "</p>";
    page += "<div class='center-button'>";
    page += "<a href='/speedtest' id='speedtest-button' class='button' onclick='showWaiting()'>&#x1F680; Iniciar Prueba</a>";
    page += "</div>";
    page += "<p id='waiting-message' style='display:none; text-align:center;'>Por favor, espere mientras se realiza la prueba...</p>";
    page += "</div>";

    page += "</div>"; // end carousel-container

    // --- Dots ---
    page += "<div class='dots'>";
    page += "<span class='dot' onclick='currentSlide(1)'></span>";
    page += "<span class='dot' onclick='currentSlide(2)'></span>";
    page += "<span class='dot' onclick='currentSlide(3)'></span>";
    page += "<span class='dot' onclick='currentSlide(4)'></span>";
    page += "<span class='dot' onclick='currentSlide(5)'></span>"; // 5to punto
    page += "</div>";

    page += "</div>"; // end container

    // --- JavaScript ---
    page += "<script>";
    page += "let slideIndex = 1;";
    page += "showSlide(slideIndex);";
    page += "function changeSlide(n) { showSlide(slideIndex += n); }";
    page += "function currentSlide(n) { showSlide(slideIndex = n); }";
    page += "function showWaiting() {";
    page += "  document.getElementById('speedtest-button').setAttribute('disabled', 'true');";
    page += "  document.getElementById('speedtest-button').innerHTML = '⏳ Midiendo...';";
    page += "  document.getElementById('waiting-message').style.display = 'block';";
    page += "}";
    page += "function showSlide(n) {";
    page += "let i; let slides = document.getElementsByClassName('carousel-slide');";
    page += "let dots = document.getElementsByClassName('dot');";
    page += "if (n > slides.length) { slideIndex = 1; }";
    page += "if (n < 1) { slideIndex = slides.length; }";
    page += "for (i = 0; i < slides.length; i++) { slides[i].style.display = 'none'; }";
    page += "for (i = 0; i < dots.length; i++) { dots[i].className = dots[i].className.replace(' active', ''); }";
    page += "slides[slideIndex - 1].style.display = 'block';";
    page += "dots[slideIndex - 1].className += ' active';";
    page += "}";
    page += "setInterval(function() { changeSlide(1); }, 30000);"; 
    page += "function updateTime() { fetch('/time').then(response => response.text()).then(data => { if (data) document.getElementById('current-time').innerText = data; }); } setInterval(updateTime, 900000);";
    page += "</script>";
    
    page += "</body></html>";

    server.send(200, "text/html", page);
}

void handleSpeedTest() {
  handleRoot(); // Hack: refresca la pagina para mostrar el estado "midiendo" via JS
  testDownloadSpeed();
  // Idealmente se usaria AJAX, pero mantengo la logica simple original
}

void handleTimeRequest() {
  server.send(200, "text/plain", getFormattedTime());
}

// --- Setup y Loop ---

void setup() {
    delay(1000);
    Serial.begin(115200);
    Serial.println("\n\n====================================");
    Serial.println("   Iniciando WebServer ESP32 + BT   ");
    Serial.println("====================================");

    // Inicializar BLE
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan(); // Crear escáner
    pBLEScan->setActiveScan(true); // Escaneo activo (más rápido, más consumo)
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    serial_number = WiFi.macAddress();
    id_Esp = String(hostname_prefix) + leftRepCadena(serial_number);
    WiFi.setHostname(id_Esp.c_str());

    Serial.println("[INFO] Hostname configurado: " + id_Esp);

    WiFiManager wifiManager;
    Serial.println("[INFO] Buscando redes conocidas o iniciando AP de configuracion...");
    if (!wifiManager.autoConnect(id_Esp.c_str())) {
      Serial.println("[CRITICO] No se pudo conectar. Reiniciando...");
      ESP.restart();
    }
    Serial.println("[OK] WiFi Conectado correctamente.");

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.print("[INFO] Sincronizando hora NTP");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
      delay(500);
      Serial.print(".");
      now = time(nullptr);
    }
    Serial.println("\n[OK] Hora sincronizada: " + getFormattedTime());

    updateNetworkData();
    previousIpUpdate = millis();
    previousTimeUpdate = millis();

    server.on("/", handleRoot);
    server.on("/speedtest", handleSpeedTest);
    server.on("/time", handleTimeRequest);
    server.begin();

    Serial.println("------------------------------------");
    Serial.print("[OK] Servidor Web activo en: http://");
    Serial.print(WiFi.localIP());
    Serial.println(":3000");
    Serial.println("------------------------------------");
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - previousTimeUpdate >= timeInterval) {
      previousTimeUpdate = currentMillis;
    }

    if (currentMillis - previousIpUpdate >= ipInterval) {
      previousIpUpdate = currentMillis;
      updateNetworkData();
    }
    server.handleClient();
}
