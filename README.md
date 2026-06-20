# AT45DB041D SPI DataFlash Library

[![Language](https://img.shields.io/badge/Language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

A lightweight, hardware-independent C library for the **Atmel/Adesto AT45DB041D** (4-Mbit) SPI DataFlash memory.

## Overview
The AT45DB041D is a 2.5V or 2.7V, serial-interface Flash memory supporting RapidS™ SPI compatible frequencies up to 66 MHz. Its memory is organized as 2,048 pages of 256 or 264 bytes each. It features two SRAM buffers, allowing data to be received while a page in the main memory is being reprogrammed.

This library provides a complete set of functions to interact with the chip, including reading, writing, erasing, and power management.

## Features
* **Hardware Independent:** Porting to any microcontroller (STM32, AVR, ESP32, etc.) requires implementing just 3 simple SPI wrapper functions.
* **Dual Page Size Support:** Automatically detects and supports both Standard DataFlash page size (264 bytes) and "Power of 2" binary page size (256 bytes).
* **SRAM Buffer Management:** Full support for read/write operations using both internal SRAM buffers (Buffer 1 and Buffer 2).
* **Erase Operations:** Support for Page, Block, Sector, and Chip erase commands.
* **Power Management:** Deep Power-Down and Wake-Up features to minimize energy consumption.
* **Read-Modify-Write:** Automated EEPROM-like emulation using internal SRAM buffers.

## Porting & Integration
To use this library, you do not need to modify the core `.c` and `.h` files. Instead, you must provide implementations for three external hardware-specific functions in your main application code (or HAL layer).

Define the following functions in your project:

```c
// Example implementation for STM32 HAL
#include "at45db041d.h"

extern SPI_HandleTypeDef hspi1;

// 1. Send and receive 1 byte over SPI
uint8_t SPI_Transfer(uint8_t data) {
    uint8_t rx_data = 0;
    HAL_SPI_TransmitReceive(&hspi1, &data, &rx_data, 1, HAL_MAX_DELAY);
    return rx_data;
}

// 2. Set Chip Select (CS) pin LOW
void SPI_CS_Low(void) {
    HAL_GPIO_WritePin(SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_RESET);
}

// 3. Set Chip Select (CS) pin HIGH
void SPI_CS_High(void) {
    HAL_GPIO_WritePin(SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_SET);
}
```


## Usage Examples

1. **Initialization**
   
Initializes the chip and detects its current page size configuration.

```c
AT45_Device_t flashDev;

if (AT45_Init(&flashDev) == AT45_OK) {
    // Initialization successful!
    // flashDev.pageSize will be 256 or 264
    // flashDev.isBinaryMode will be true or false
}
```

2. **Writing to Main Memory (via SRAM Buffer)**
   
Writes an array of data to a specific page using one of the internal buffers.

```c
uint8_t myData[10] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
uint16_t targetPage = 10;
uint16_t offsetInPage = 0;

// Write data directly to Flash memory utilizing SRAM Buffer 1
AT45_Status_t status = AT45_WriteMainMemory(targetPage, offsetInPage, myData, sizeof(myData), AT45_BUFFER_1);
```

3. **Reading from Main Memory**
   
Reads data back from the flash memory array.

```c
uint8_t readBuffer[10];
uint16_t targetPage = 10;
uint16_t offsetInPage = 0;

// Read memory at high frequency
AT45_Status_t status = AT45_ReadMainMemory(targetPage, offsetInPage, readBuffer, sizeof(readBuffer), AT45_READ_HIGH_FREQ);
```


4. **Erasing the Chip**
   
You can erase specific pages, blocks, or the entire chip.

```c
// Erase a single page
AT45_PageErase(10);

// Erase a 2KB Block (8 pages)
AT45_BlockErase(2);

// Safely erase the entire chip (iterates through blocks)
AT45_ChipErase_Safe();
```

5. **Configuring Binary Page Size (256 bytes)**
   
By default, the chip ships with a 264-byte page size. You can permanently configure it to 256 bytes (Binary Mode).
**WARNING:** This is a ONE-TIME programmable register. Once configured to 256 bytes, it cannot be reverted to 264 bytes. A power cycle is required after calling this function.

```c
// Permanently set page size to 256 bytes
AT45_ConfigureBinaryPageSize();
```

---

## Documentation
For complete details on the hardware capabilities, timings, and opcodes, please refer to the official AT45DB041D Datasheet included in this repository.

---

## License
This library is open-sourced under the MIT License.











