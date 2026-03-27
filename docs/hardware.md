# Hardware base del proyecto

## Plataforma principal

El proyecto se basa en un microcontrolador de la familia ESP32-S3 con pantalla TFT integrada y soporte de almacenamiento por MicroSD.

## Elementos principales

- **Microcontrolador:** ESP32-S3
- **Pantalla:** TFT a color
- **Almacenamiento:** tarjeta MicroSD
- **Interfaz:** gráfica y táctil según la variante de placa
- **Programación y energía:** USB

## Bloques funcionales documentados

### 1. Procesamiento embebido

El ESP32-S3 coordina la interfaz, el acceso a periféricos y la lógica general de operación del sistema.

### 2. Visualización

La pantalla TFT presenta menús, estados del sistema y mensajes operativos.

### 3. Almacenamiento local

La MicroSD permite guardar configuraciones, recursos y archivos de apoyo del sistema dentro del alcance autorizado del laboratorio.

### 4. Comunicación SPI

La pantalla y la tarjeta SD dependen de comunicación SPI o de una configuración equivalente según el diseño de la placa.

## Pines observados en el material entregado

```cpp
#define SD_MISO 39
#define SD_MOSI 40
#define SD_SCLK 38
#define SD_CS   47
```
