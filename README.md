# 📡 ESP32 WebServer Monitor

**Autor:** Juan Maioli  
**Versión:** 1.8 (Migrado a ESP32 + Bluetooth Scan)

Este proyecto es un monitor de sistema y red avanzado para el microcontrolador **ESP32**. Genera un servidor web local con una interfaz tipo carrusel que muestra estadísticas vitales, escaneo de redes y utilidades en tiempo real.

## ✨ Características Principales

*   **🖥️ Dashboard Web Interactivo:** Accesible vía navegador (Puerto 3000), con navegación manual táctil o por botones.
*   **📶 Escáner WiFi:** Detecta redes cercanas, mostrando SSID, intensidad (RSSI) y seguridad.
*   **🦷 Escáner Bluetooth (BLE):** Busca dispositivos Bluetooth Low Energy cercanos.
*   **🚀 Speedtest Integrado:** Prueba de velocidad de descarga real (descarga archivo de 5MB).
*   **🌐 Datos de Red:** Obtiene IP Pública, IP local, Gateway y Máscara de subred.
*   **🕒 Sincronización NTP:** Hora y fecha automáticas (Zona horaria Argentina GMT-3).
*   **🔌 Portal Cautivo (WiFiManager):** Si no encuentra red, crea un punto de acceso para configuración fácil sin tocar código.

## 🛠️ Requisitos de Hardware

*   **Placa:** ESP32 (Probado en ESP32-S3, compatible con WROOM/WROVER).
*   **Flash:** Recomendado 4MB o superior (Configuración óptima: 8MB).

## ⚙️ Configuración del Entorno (Arduino IDE)

Para compilar este proyecto, es **CRÍTICO** ajustar el esquema de partición debido al tamaño de las librerías de Bluetooth.

1.  **Gestor de Tarjetas:** Asegúrate de tener instalado el core `esp32` de Espressif.
2.  **Librerías Requeridas:**
    *   `WiFiManager` (por tzapu/tablatronix).
    *   `ESP32 BLE Arduino` (Incluida en el core normalmente).
    *   `HTTPClient`, `WiFiClientSecure`, `WebServer`.

### ⚠️ Parámetros de Compilación (Importante)

Configura tu IDE con estos valores para evitar errores de memoria:

| Opción | Valor Requerido |
| :--- | :--- |
| **Placa** | ESP32 Dev Module / ESP32S3 Dev Module |
| **Partition Scheme** | **Huge APP (3MB No OTA/1MB SPIFFS)** |
| **Flash Size** | **8MB** (o 4MB según tu chip real) |
| **Upload Speed** | 921600 (o 115200 si falla) |

> **Nota:** Si recibes el error *"text section exceeds available space"*, es porque no seleccionaste "Huge APP".

## 🚀 Uso e Instalación

1.  **Cargar Código:** Sube el sketch `Esp32_WebServer.ino` a tu placa.
2.  **Primera Conexión:**
    *   El ESP32 creará una red WiFi abierta llamada **`Esp32-XXXX`**.
    *   Conéctate con tu móvil/PC.
    *   Se abrirá el portal de configuración (o ve a `192.168.4.1`).
    *   Selecciona tu red WiFi local e ingresa la contraseña.
3.  **Acceso al Monitor:**
    *   Abre el Monitor Serial (115200 baudios) para ver la IP asignada.
    *   Navega a: `http://[IP-DEL-ESP32]:3000`

## 📊 Estructura del Carrusel Web

1.  **Estado:** Info del sistema (Uptime, RAM, Flash, IP, MAC).
2.  **WiFi:** Lista de redes ordenadas por señal.
3.  **Bluetooth:** Lista de dispositivos BLE detectados.
4.  **Speedtest:** Botón para iniciar prueba de velocidad.

## 🐛 Debugging

El sistema envía mensajes de diagnóstico al puerto serie:
*   `[INFO]`: Eventos normales (conexión, escaneos).
*   `[OK]`: Operaciones exitosas.
*   `[CRITICO]`: Errores graves o reinicios.

---
*Desarrollado para fines educativos y de monitoreo doméstico.*