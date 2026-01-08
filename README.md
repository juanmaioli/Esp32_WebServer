# 📡 ESP32 WebServer Monitor (FreeRTOS Dual-Core)

**Autor:** Juan Maioli  
**Versión:** 2.0 (Multitarea Real + Configuración Persistente)

Este proyecto es un monitor de sistema y red avanzado para el microcontrolador **ESP32**. Genera un servidor web local con una interfaz fluida tipo carrusel que muestra estadísticas vitales, escaneo de redes y utilidades en tiempo real.

> **🚀 Novedad v2.0:** Utiliza **FreeRTOS** para ejecutar escaneos de WiFi y Bluetooth en un núcleo secundario (Core 0), manteniendo el servidor web siempre receptivo en el núcleo principal (Core 1). ¡Cero bloqueos!

## ✨ Características Principales

*   **🖥️ Dashboard Web Fluido:** Accesible vía navegador (Puerto 3000), con navegación manual y diseño responsivo (Dark Mode).
*   **⚡ Arquitectura Dual-Core:** Las tareas pesadas (escaneos) corren en segundo plano sin congelar la interfaz web.
*   **⚙️ Configuración Persistente:** Edita la descripción del dispositivo y el proveedor de IP pública desde la web (guardado en Flash/NVS).
*   **📶 Escáner WiFi:** Detecta redes cercanas, mostrando SSID, intensidad (RSSI) y seguridad.
*   **🦷 Escáner Bluetooth (BLE):** Busca dispositivos Bluetooth Low Energy cercanos.
*   **🚀 Speedtest Integrado:** Prueba de velocidad de descarga real.
*   **🌐 Datos de Red:** Obtiene IP Pública (configurable), IP local, Gateway y Máscara de subred.
*   **🕒 Sincronización NTP:** Hora y fecha automáticas (Zona horaria Argentina GMT-3).
*   **🔌 Portal Cautivo (WiFiManager):** Si no encuentra red, crea un punto de acceso para configuración fácil.

## 🛠️ Requisitos de Hardware

*   **Placa:** ESP32 Dual Core (ESP32-WROOM, ESP32-S3, etc.).
*   **Flash:** Recomendado 4MB o superior (Configuración óptima: 8MB).

## ⚙️ Configuración del Entorno (Arduino IDE)

Para compilar este proyecto, es **CRÍTICO** ajustar el esquema de partición debido al tamaño de las librerías de Bluetooth y FreeRTOS.

1.  **Gestor de Tarjetas:** Asegúrate de tener instalado el core `esp32` de Espressif.
2.  **Librerías Requeridas:**
    *   `WiFiManager` (por tzapu/tablatronix).
    *   `ESP32 BLE Arduino`, `Preferences`, `FreeRTOS` (Nativas del core ESP32).
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
    *   Conéctate y configura tu WiFi local (Portal en `192.168.4.1`).
3.  **Acceso al Monitor:**
    *   Navega a: `http://[IP-DEL-ESP32]:3000`

## 📊 Estructura del Carrusel Web

1.  **Estado:** Info del sistema (Uptime, RAM, Flash, IP, MAC) y descripción personalizada.
2.  **WiFi:** Lista de redes ordenadas por señal.
3.  **Bluetooth:** Lista de dispositivos BLE detectados.
4.  **Speedtest:** Botón para iniciar prueba de velocidad.
5.  **Configuración:** Formulario para editar Descripción y Dominio de IP Pública.

## 🐛 Debugging

El sistema envía mensajes de diagnóstico al puerto serie (115200 baudios):
*   `[INFO]`: Eventos normales.
*   `[BG-TASK]`: Eventos de la tarea de segundo plano (Core 0).
*   `[OK]`: Operaciones exitosas.
*   `[CRITICO]`: Errores graves o reinicios.

---
*Desarrollado para fines educativos y de monitoreo doméstico.*