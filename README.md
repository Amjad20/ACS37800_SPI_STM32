# ACS37800_SPI_STM32

This repository provides a **SPI driver** for the **ACS37800** power monitoring IC from Allegro MicroSystems.  
The driver is written for **STM32 MCUs** using the **STM32 HAL** library and supports:

- Reading RMS current/voltage
- Reading instantaneous current/voltage
- Configuring the `BYPASS_N_EN` bit (zero‑crossing bypass)
- Adjusting the number of samples (N) used for RMS/active power calculations
- Writing/reading registers via SPI, including access to the EEPROM shadow registers

> **Note:** Most ACS37800 drivers online uses I²C, but this driver targets the **SPI variant** of the device.

---

## Hardware Connection (SPI)

| ACS37800 Pin | STM32 Pin        |
|--------------|------------------|
| `VCC`        | 3.3V or 5V       |
| `GND`        | GND              |
| `SCLK`       | SPI SCK          |
| `MOSI`       | SPI MOSI         |
| `MISO`       | SPI MISO         |
| `CS`         | Any GPIO output  |

**SPI Mode:**  
- **Mode 3** (CPOL = 1, CPHA = 1)

---

## Driver API

### Data Type

```c
typedef struct {
    void*    spi_device;   // SPI_HandleTypeDef*
    void*    cs_port;      // GPIO_TypeDef*
    uint16_t cs_pin;       // GPIO pin
    uint8_t  maxVolt;      // Maximum line voltage (not used in current scaling)
    uint8_t  maxCurrent;   // Rated current of the ACS37800 variant
    uint16_t senseRes;     // R_SENSE in ohms
    uint32_t divRes;       // Sum of isolation resistors (R_ISO) in ohms
} acs37800_t;
```

### Functions
| Function     | Description      |
|--------------|------------------|
| `void acs_getRMS(acs37800_t *dev, float *curr, float *volt)`        | Reads RMS current (A) and RMS voltage (V) from register 0x20.             |
| `void acs_getInstCurrVolt(acs37800_t *dev, float *curr, float *volt)`        | Reads instantaneous current/voltage from register 0x2A.              |
| `void acs_setBybassNenable(acs37800_t *dev, bool bypass, bool eeprom)`       | Sets the BYPASS_N_EN bit. If eeprom = true, also writes to the EEPROM shadow register.          |
| `void acs_setNumberOfSamples(acs37800_t *dev, uint32_t numSamples, bool eeprom)`       | Sets the number of samples (N, 10‑bit) used for RMS/active power. Valid range: 0–1023.         |

### How to Use
1. Add Files to Your STM32 Project
    - Copy acs37800.c and acs37800.h into your Src/ and Inc/ folders.

2. Configure SPI in CubeMX / CubeIDE
    - Enable SPI with full‑duplex master mode.
    - Set Data Size = 8 bits.
    - Choose Mode 3 (CPOL/CPHA as described).
    - Set Baud rate (try slow first, then higher speeds later).
    - Generate code.

3. Initialize the Driver Handle
```c
#include "acs37800.h"

acs37800_t acs = {
    .spi_device = &hspi1,
    .cs_port    = GPIOA,
    .cs_pin     = GPIO_PIN_4,
    .maxCurrent = 30,       // e.g. ACS37800KMACTR-030B3 variant
    .senseRes   = 2700,     // (check datasheet for Rsense)
    .divRes     = 4000000   // sum of isolation resistors
};
```

---

### Example Usage
```c
float current, voltage;
acs_getRMS(&acs, &current, &voltage);
printf("RMS Current = %.2f A, RMS Voltage = %.2f V\r\n", current, voltage);
```

---

### License
This driver is provided under the MIT License. Feel free to use and modify.
