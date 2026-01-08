// ESP32 WebServer Monitor v1.8 (Migrado a ESP32 + Bluetooth Scan)
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
#include <Preferences.h>

struct WifiNetwork {
  String ssid;
  int32_t rssi;
  wifi_auth_mode_t encryptionType;
};

// --- Variables Globales ---
Preferences preferences;
SemaphoreHandle_t dataMutex; // Mutex para proteger datos compartidos
TaskHandle_t networkTaskHandle; // Handle para la tarea de fondo

volatile bool requestWifiScan = false;
volatile bool requestBleScan = false;

String config_desc = "Casa";
String config_domain = "ifconfig.me";

const char* hostname_prefix = "Esp32-";
String serial_number;
String id_Esp;
String localIP;
String publicIP = "Obteniendo...";
String wifiNetworksList = "Escaneando...";
String bluetoothDevicesList = "Escaneando..."; // Nueva variable para BT
String lastWifiScanTime = "Nunca";
String lastBluetoothScanTime = "Nunca"; // Nueva variable de tiempo BT
String downloadSpeed = "No medido";
String lastSpeedTestTime = "Nunca";

// --- Objetos Globales ---
WebServer server(3000);
BLEScan* pBLEScan; // Puntero para el escáner BLE

// --- Contenido Web Estático (PROGMEM) ---
const char INDEX_HTML_HEAD[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang='es'><head>
<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>
<meta http-equiv='refresh' content='1200'>
<link rel='icon' href='data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><text y=%22.9em%22 font-size=%2290%22>📟</text></svg>'>
<title>Estado del Dispositivo (ESP32)</title>
<style>
:root { --bg-color: #f0f2f5; --container-bg: #ffffff; --text-primary: #1c1e21; --text-secondary: #4b4f56; --pre-bg: #f5f5f5; --hr-color: #e0e0e0; --dot-color: #bbb; --dot-active-color: #717171; --input-bg: #fff; --input-border: #ccc; }
@media (prefers-color-scheme: dark) { :root { --bg-color: #121212; --container-bg: #1e1e1e; --text-primary: #e0e0e0; --text-secondary: #b0b3b8; --pre-bg: #2a2a2a; --hr-color: #3e4042; --dot-color: #555; --dot-active-color: #ccc; --input-bg: #333; --input-border: #555; } }
body { background-color: var(--bg-color); color: var(--text-secondary); font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 1rem 0;}
.container { background-color: var(--container-bg); padding: 2rem; border-radius: 8px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); text-align: left; width: 400px; height: 80vh; position: relative; display: flex; flex-direction: column; }
h1, h2 { color: var(--text-primary); margin-bottom: 1rem; text-align: center; } p { color: var(--text-secondary); font-size: 1.1rem; margin: 0.5rem 0; } strong { color: var(--text-primary); } hr { border: 0; height: 1px; background-color: var(--hr-color); margin: 1.5rem 0; }
.carousel-container { position: relative; flex-grow: 1; overflow: hidden; } .carousel-slide { display: none; height: 100%; width: 100%; flex-basis: 100%; flex-shrink: 0; overflow-y: auto; padding-right: 15px; box-sizing: border-box; word-wrap: break-word; }
.fade { animation-name: fade; animation-duration: 0.5s; } @keyframes fade { from {opacity: .4} to {opacity: 1} }
.prev, .next { cursor: pointer; position: absolute; top: 50%; transform: translateY(-50%); width: auto; padding: 16px; color: var(--text-primary); font-weight: bold; font-size: 24px; transition: 0.3s; user-select: none; z-index: 10; } .prev { left: -50px; } .next { right: -50px; } .prev:hover, .next:hover { background-color: rgba(0,0,0,0.2); border-radius: 50%; }
.dots { text-align: center; padding-top: 20px; } .dot { cursor: pointer; height: 15px; width: 15px; margin: 0 2px; background-color: var(--dot-color); border-radius: 50%; display: inline-block; transition: background-color 0.3s ease; } .active, .dot:hover { background-color: var(--dot-active-color); }
.emoji-container { text-align: center; margin-top: 15px; margin-bottom: 15px; } .emoji { font-size: 4em; line-height: 1; display: inline-block; vertical-align: middle; }
.button { background-color: #4CAF50; color: white; padding: 10px 20px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 10px 0; cursor: pointer; border-radius: 5px; border: none;} .button:hover { background-color: #45a049; } .button[disabled] { background-color: #555; color: #eee; border: 1px solid #eeeeee; cursor: not-allowed; }
.center-button { text-align: center; } input[type=text] { width: 100%; padding: 12px 20px; margin: 8px 0; box-sizing: border-box; border: 1px solid var(--input-border); border-radius: 4px; background-color: var(--input-bg); color: var(--text-primary); }
@media (max-width: 768px) { .container { width: 95%; height: 80vh; padding: 1rem; } .prev, .next { top: auto; bottom: 5px; transform: translateY(0); } .prev { left: 10px; } .next { right: 10px; } }
</style></head><body><div class='container'>
<a class='prev' onclick='changeSlide(-1)'>&#10094;</a><a class='next' onclick='changeSlide(1)'>&#10095;</a><div class='carousel-container'>
)rawliteral";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600;
const int daylightOffset_sec = 0;

unsigned long previousTimeUpdate = 0;
unsigned long previousIpUpdate = 0;
unsigned long previousWifiUpdate = 0; // Nueva variable para rastrear el escaneo automático
const long timeInterval = 60000;      
const long ipInterval = 1740000;
const long wifiInterval = 300000;     // Escaneo automático cada 5 minutos (300s)

// --- Funciones Auxiliares ---
void loadSettings() {
  preferences.begin("app-config", true); // Modo lectura
  config_desc = preferences.getString("desc", "Casa");
  config_domain = preferences.getString("domain", "ifconfig.me");
  preferences.end();
}

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
  String cleanMac = "";
  for (int i = 0; i < mac.length(); i++) {
    if (isAlphaNumeric(mac[i])) {
      cleanMac += mac[i];
    }
  }
  if (cleanMac.length() >= 4) {
    return cleanMac.substring(cleanMac.length() - 4);
  } else {
    return "XXXX"; // Valor de respaldo
  }
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
  const char* host = config_domain.c_str();
  
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
      uint8_t buff[4096] = { 0 };
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

// --- Tarea de Segundo Plano (Core 0) ---
void networkTask(void * parameter) {
  for (;;) {
    unsigned long currentMillis = millis();
    
    // --- 1. Tareas Periódicas (Solo IP) ---
    if (currentMillis - previousIpUpdate >= ipInterval || previousIpUpdate == 0) {
      Serial.println("[" + getFormattedTime() + "] [BG-TASK] Actualizando IP...");
      
      String tempLocalIP = WiFi.localIP().toString();
      String tempPublicIP = "Error";
      WiFiClientSecure client;
      client.setInsecure();
      const char* host = config_domain.c_str(); 
      if (client.connect(host, 443)) {
        client.print(String("GET /ip HTTP/1.1\r\n") + "Host: " + host + "\r\n" + "User-Agent: ESP32-IP-Checker\r\n" + "Connection: close\r\n\r\n");
        if (client.find("\r\n\r\n")) {
          String s = client.readStringUntil('\n');
          s.trim();
          if (s.length() > 0) tempPublicIP = s;
        }
      }

      if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        localIP = tempLocalIP;
        publicIP = tempPublicIP;
        previousIpUpdate = currentMillis;
        xSemaphoreGive(dataMutex);
      }
    }

    // --- 2. Escaneo WiFi (Automático y Manual) ---
    if (requestWifiScan || (currentMillis - previousWifiUpdate >= wifiInterval || previousWifiUpdate == 0)) {
      Serial.println("[" + getFormattedTime() + "] [BG-TASK] Ejecutando Escaneo WiFi...");
      String tempWifiList = scanWifiNetworks();
      String tempWifiTime = getFormattedTime();
      
      if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        wifiNetworksList = tempWifiList;
        lastWifiScanTime = tempWifiTime;
        previousWifiUpdate = currentMillis; // Actualizar timer automático
        xSemaphoreGive(dataMutex);
      }
      requestWifiScan = false; // Reset flag manual
      Serial.println("[" + getFormattedTime() + "] [BG-TASK] Escaneo WiFi Completado.");
    }

    // --- 3. Escaneo Bluetooth Manual ---
    if (requestBleScan) {
      Serial.println("[" + getFormattedTime() + "] [BG-TASK] Ejecutando Escaneo Bluetooth Manual...");
      String tempBleList = scanBluetoothDevices();
      String tempBleTime = getFormattedTime();
      
      if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        bluetoothDevicesList = tempBleList;
        lastBluetoothScanTime = tempBleTime;
        xSemaphoreGive(dataMutex);
      }
      requestBleScan = false; // Reset flag
      Serial.println("[" + getFormattedTime() + "] [BG-TASK] Escaneo Bluetooth Completado.");
    }

    // Delay corto para respuesta rápida a botones
    vTaskDelay(pdMS_TO_TICKS(200)); 
  }
}

void handleSaveConfig() {
  if (server.hasArg("desc")) {
    String d = server.arg("desc");
    d.trim();
    if (d.length() > 0 && d.length() <= 50) {
      preferences.begin("app-config", false);
      preferences.putString("desc", d);
      preferences.end();
      config_desc = d;
    }
  }
  
  if (server.hasArg("domain")) {
    String dom = server.arg("domain");
    dom.trim();
    if (dom.length() > 0 && dom.length() <= 50) {
      preferences.begin("app-config", false);
      preferences.putString("domain", dom);
      preferences.end();
      config_domain = dom;
    }
  }

  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handleScanWifi() {
  requestWifiScan = true;
  unsigned long start = millis();
  while (requestWifiScan && millis() - start < 10000) {
    delay(100); // Esperar a que la tarea BG termine
  }
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handleScanBle() {
  requestBleScan = true;
  unsigned long start = millis();
  while (requestBleScan && millis() - start < 10000) {
    delay(100); // Esperar a que la tarea BG termine
  }
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

// --- Handler para el Servidor Web ---

void handleRoot() {

    // 1. Copia de seguridad de datos (Sección Crítica)

    String _uptime, _wifiList, _bleList, _pubIP, _locIP, _gw, _mask, _rssi, _mac, _wScanTime, _bScanTime, _spdTime, _spdVal;

    

    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {

        _wifiList = wifiNetworksList;

        _bleList = bluetoothDevicesList;

        _pubIP = publicIP;

        _locIP = localIP;

        _gw = WiFi.gatewayIP().toString();

        _mask = WiFi.subnetMask().toString();

        _rssi = String(WiFi.RSSI());

        _mac = WiFi.macAddress();

        _wScanTime = lastWifiScanTime;

        _bScanTime = lastBluetoothScanTime;

        _spdTime = lastSpeedTestTime;

        _spdVal = downloadSpeed;

        xSemaphoreGive(dataMutex);

    }



    unsigned long totalSeconds = millis() / 1000;

    unsigned long days = totalSeconds / 86400;

    unsigned long hours = (totalSeconds % 86400) / 3600;

    unsigned long minutes = ((totalSeconds % 86400) % 3600) / 60;

    unsigned long seconds = ((totalSeconds % 86400) % 3600) % 60;

    _uptime = "";

    if (days > 0) _uptime += String(days) + "d ";

    if (hours > 0 || days > 0) _uptime += String(hours) + "h ";

    if (minutes > 0 || hours > 0 || days > 0) _uptime += String(minutes) + "m ";

    _uptime += String(seconds) + "s";

    if (_uptime == "") _uptime = "0s";



        server.setContentLength(CONTENT_LENGTH_UNKNOWN);



        server.send(200, "text/html", "");



    



        // Enviar cabecera estática desde Flash (PROGMEM)



        server.sendContent_P(INDEX_HTML_HEAD);



    



        String chunk;



        chunk.reserve(2048); // Pre-reservar memoria para evitar fragmentación



        



        // --- Slide 1: Estado del Dispositivo ---



        chunk = "<div class='carousel-slide fade'><h2>Estado - " + config_desc + "</h2><div class='emoji-container'><span class='emoji'>📟</span></div><br>";



        chunk += "<h3><strong>📅 Fecha:</strong> " + getFormattedDate() + "<br>";



        chunk += "<strong>⌚ Hora:</strong> <span id='current-time'>" + getFormattedTime() + "</span><br>";



        chunk += "<strong>🖥️ Hostname:</strong> " + id_Esp + "<br>";



        chunk += "<strong>🏠 IP Privada:</strong> " + _locIP + "<br>";



        chunk += "<strong>↔️ M&aacute;scara:</strong> " + _mask + "<br>";



        chunk += "<strong>🚪 Gateway:</strong> " + _gw + "<br>";



        chunk += "<strong>🌐 IP P&uacute;blica:</strong> " + _pubIP + "<br>";



        chunk += "<strong>📶 WiFi RSSI:</strong> " + _rssi + " dBm<br>";



        chunk += "<strong>🆔 MAC:</strong> " + _mac + "<br>";



        chunk += "<strong>💡 Chip ID:</strong> " + getUniqueId() + "<br>";



        chunk += "<strong>💾 Flash:</strong> " + String(ESP.getFlashChipSize() / 1024) + " KB<br>";



        chunk += "<strong>🧠 Free Heap:</strong> " + String(ESP.getFreeHeap() / 1024.0, 2) + " KB<br>";



        chunk += "<strong>⚡ Uptime:</strong> " + _uptime + "</h3></div>";



        server.sendContent(chunk);



        // --- Slide 2: Redes WiFi Cercanas ---



        chunk = "<div class='carousel-slide fade'><h2>Redes WiFi Cercanas</h2><div class='emoji-container'><span class='emoji'>📡</span></div><br><p><strong>Escaneado:</strong> " + _wScanTime + "</p><div class='center-button'><a href='/scanwifi' id='scanwifi-button' class='button' onclick='showWaiting(\"scanwifi-button\", \"waiting-wifi\")'>🔄 Escanear Ahora</a></div><p id='waiting-wifi' style='display:none; text-align:center;'>Buscando redes...</p>" + _wifiList + "</div>";



        server.sendContent(chunk);



    



        // --- Slide 3: Bluetooth (BLE) ---



        chunk = "<div class='carousel-slide fade'><h2>Bluetooth (BLE)</h2><div class='emoji-container'><span class='emoji'>🦷</span></div><br><p><strong>Escaneado:</strong> " + _bScanTime + "</p><div class='center-button'><a href='/scanble' id='scanble-button' class='button' onclick='showWaiting(\"scanble-button\", \"waiting-ble\")'>🔄 Escanear Ahora</a></div><p id='waiting-ble' style='display:none; text-align:center;'>Buscando dispositivos...</p>" + _bleList + "</div>";



        server.sendContent(chunk);



    // --- Slide 4: Prueba de Velocidad ---

    chunk = "<div class='carousel-slide fade'><h2>Prueba de Velocidad</h2><div class='emoji-container'><span class='emoji'>🚀</span></div><br><p><strong>&Uacute;ltima prueba:</strong> " + _spdTime + "</p><p><strong>Velocidad de Descarga:</strong> " + _spdVal + "</p><div class='center-button'><a href='/speedtest' id='speedtest-button' class='button' onclick='showWaiting(\"speedtest-button\", \"waiting-message\")'>&#x1F680; Iniciar Prueba</a></div><p id='waiting-message' style='display:none; text-align:center;'>Por favor, espere mientras se realiza la prueba...</p></div>";

    server.sendContent(chunk);



    // --- Slide 5: Configuración (NUEVA) ---

    chunk = "<div class='carousel-slide fade'><h2>Configuraci&oacute;n</h2><div class='emoji-container'><span class='emoji'>⚙️</span></div><br><form action='/save' method='POST'><p><strong>Descripci&oacute;n del Dispositivo:</strong><br><input type='text' name='desc' value='" + config_desc + "' maxlength='50' placeholder='Ej: Casa'></p><p><strong>Dominio IP P&uacute;blica:</strong><br><input type='text' name='domain' value='" + config_domain + "' maxlength='50' placeholder='Ej: ifconfig.me'></p><div class='center-button'><button type='submit' class='button'>💾 Guardar Cambios</button></div></form></div>";

    server.sendContent(chunk);



    chunk = "</div><div class='dots'><span class='dot' onclick='currentSlide(1)'></span><span class='dot' onclick='currentSlide(2)'></span><span class='dot' onclick='currentSlide(3)'></span><span class='dot' onclick='currentSlide(4)'></span><span class='dot' onclick='currentSlide(5)'></span></div></div>";

    chunk += "<script>let slideIndex=1;showSlide(slideIndex);function changeSlide(n){showSlide(slideIndex+=n)}function currentSlide(n){showSlide(slideIndex=n)}function showWaiting(b,m){document.getElementById(b).setAttribute('disabled','true');document.getElementById(b).innerHTML='⏳ Trabajando...';document.getElementById(m).style.display='block'}function showSlide(n){let i;let s=document.getElementsByClassName('carousel-slide');let d=document.getElementsByClassName('dot');if(n>s.length){slideIndex=1}if(n<1){slideIndex=s.length}for(i=0;i<s.length;i++){s[i].style.display='none'}for(i=0;i<d.length;i++){d[i].className=d[i].className.replace(' active','')}s[slideIndex-1].style.display='block';d[slideIndex-1].className+=' active'}function updateTime(){fetch('/time').then(r=>r.text()).then(d=>{if(d)document.getElementById('current-time').innerText=d})}setInterval(updateTime,900000);</script></body></html>";

    server.sendContent(chunk);

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
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n====================================");
    Serial.println("   Iniciando WebServer ESP32 + BT   ");
    Serial.println("====================================");

    // Cargar Configuración
    loadSettings();

    // Inicializar BLE
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan(); // Crear escáner
    pBLEScan->setActiveScan(true); // Escaneo activo (más rápido, más consumo)
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    WiFi.mode(WIFI_STA); 
    delay(100); // Pequeña espera para inicializar hardware
    serial_number = WiFi.macAddress();
    
    // Si la MAC es nula o solo ceros, usar el Chip ID único del hardware
    if (serial_number == "" || serial_number == "00:00:00:00:00:00") {
      serial_number = getUniqueId();
    }
    
    Serial.println("[INFO] MAC para Hostname: " + serial_number);
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

    // Crear Mutex
    dataMutex = xSemaphoreCreateMutex();

    // Iniciar Tarea de Segundo Plano (Core 0)
    // Stack de 4KB (4096), prioridad 1
    xTaskCreatePinnedToCore(networkTask, "NetworkTask", 8192, NULL, 1, &networkTaskHandle, 0);

    // Initial update is handled by the task immediately

    server.on("/", handleRoot);
    server.on("/speedtest", handleSpeedTest);
    server.on("/scanwifi", handleScanWifi);
    server.on("/scanble", handleScanBle);
    server.on("/time", handleTimeRequest);
    server.on("/save", HTTP_POST, handleSaveConfig);
    server.begin();

    Serial.println("------------------------------------");
    Serial.print("[OK] Servidor Web activo en: http://");
    Serial.print(WiFi.localIP());
    Serial.println(":3000");
    Serial.println("------------------------------------");
}

void loop() {
    unsigned long currentMillis = millis();
    // Tarea de tiempo (ligera) puede quedarse aqui o moverse a background
    if (currentMillis - previousTimeUpdate >= timeInterval) {
      previousTimeUpdate = currentMillis;
    }

    // IP y escaneos ahora son manejados por FreeRTOS Task en Core 0
    // El Loop (Core 1) se dedica solo a atender clientes HTTP
    server.handleClient();
    
    // Pequeño yield para estabilidad del Watchdog
    delay(2);
}
