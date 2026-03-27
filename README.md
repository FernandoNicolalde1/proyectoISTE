# proyectoISTE

## Propuesta tecnológica ISTE - ESP32-S3 con pantalla TFT y MicroSD

Este repositorio documenta el desarrollo académico de una propuesta tecnológica basada en un módulo ESP32-S3 con pantalla TFT de 2.8 pulgadas y almacenamiento MicroSD. El proyecto se presenta dentro de la carrera de Sistemas de Información y Ciberseguridad del Instituto Superior Tecnológico Universitario España.

## Propósito del proyecto

El objetivo del trabajo es documentar el diseño, integración y validación de un prototipo embebido con interfaz gráfica, almacenamiento local, exploración controlada del entorno WiFi y registro de eventos en laboratorio. El contenido público del repositorio mantiene un enfoque académico, ético y defensivo.

## Alcance público del repositorio

Este repositorio contiene únicamente documentación, estructura de proyecto y fragmentos de firmware orientados a:

- Inicialización del hardware.
- Manejo de pantalla TFT.
- Gestión de tarjeta MicroSD.
- Navegación de menús y estados de la interfaz.
- Explicación del entorno de desarrollo Arduino IDE.
- Descripción del flujo de compilación y carga.

No se publican instrucciones operativas ni implementaciones orientadas a captura de credenciales, desautenticación de terceros, phishing ni despliegue ofensivo en redes reales.

## Hardware base documentado

- Microcontrolador: ESP32-S3
- Pantalla: TFT 2.8" 240x320
- Almacenamiento: MicroSD por SPI
- Interfaz de usuario: táctil / menú gráfico
- Alimentación y programación: USB Type-C

## Entorno de desarrollo

El proyecto está pensado para Arduino IDE con soporte para placas ESP32.

### Requisitos de software

- Arduino IDE 2.x
- Paquete de placas ESP32 de Espressif
- Librerías según el proyecto cargado

### Librerías principales observadas en el firmware base

- `TFT_eSPI`
- `WiFi`
- `SD`
- `SPI`
- `USB`
- `USBMSC`
- `DNSServer`
- `WebServer`
- `esp_wifi.h`
- `touch.h`

## Estructura esperada del proyecto

```text
proyectoISTE/
├─ README.md
├─ docs/
├─ src/
├─ lib/
└─ assets/
```

## Flujo general de trabajo en Arduino IDE

1. Instalar Arduino IDE 2.x.
2. Agregar el soporte de tarjetas ESP32.
3. Seleccionar la placa ESP32-S3 correspondiente.
4. Configurar puerto, velocidad y opciones de compilación.
5. Verificar el sketch.
6. Cargar el firmware al dispositivo.
7. Validar inicialización de pantalla, SD y menú principal.

## Fragmentos representativos del firmware

### Inicialización general

```cpp
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  touch_init(tft.width(), tft.height(), tft.getRotation());

  sdSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  SD.begin(SD_CS, sdSPI);

  drawMenu();
}
```

### Renderizado del menú principal

```cpp
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
}
```

Estos fragmentos muestran la lógica de arranque, el uso de la pantalla, la inicialización del almacenamiento y el control del estado general de la aplicación. El análisis completo se desarrolla en el documento académico asociado.

## Estado del repositorio

Repositorio inicializado con documentación base. El siguiente paso es organizar:

- Documento final del proyecto.
- Recursos gráficos.
- Diagramas de arquitectura.
- Código sanitizado para revisión académica.

## Uso responsable

Este material se comparte con fines de estudio, documentación y evaluación académica en entornos autorizados. Cualquier prueba sobre redes, dispositivos o servicios debe realizarse únicamente con autorización expresa y en laboratorio controlado.
