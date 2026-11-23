#pragma once

// A equipe usou CRC-8 na V7. Para o ensaio inicial com soma módulo 256,
// selecione 1 e grave novamente OS DOIS ESP32.
#ifndef TCD_USAR_CHECKSUM
#define TCD_USAR_CHECKSUM 0
#endif

// Display no circuito B. 0 = OLED SSD1306 128x64 da montagem da equipe.
// 1 = LCD HD44780 16x2 com adaptador I2C PCF8574.
#ifndef TCD_USAR_LCD
#define TCD_USAR_LCD 0
#endif
#define TCD_ENDERECO_LCD 0x27
