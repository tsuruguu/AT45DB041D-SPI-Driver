#include "LPC17xx.h"
#include "at45db041d.h"
#include "PIN_LPC17xx.h"
#include <stdio.h>
#include <string.h>
#include "Open1768_LCD.h"
#include "LCD_ILI9325.h"
#include "asciiLib.h"
#include "TP_Open1768.h"

#define FLASH_RESET_PIN   21  // P0.21
#define FLASH_WP_PIN      29  // P4.29

#define TRACK_COUNT  4  

#define TP_MIN   150    
#define TP_MAX   4000  

const uint16_t track_start_page[TRACK_COUNT]   = {0,   512,  1024, 1536};
const uint16_t track_length_pages[TRACK_COUNT] = {512, 512,  512,  512};

void delay(uint32_t count) {
    for(volatile uint32_t i = 0; i < count; i++);
}

// ----- UART ------------------------------------------------------------------

void UART0_Init(void) {
    LPC_SC->PCONP |= (1 << 3); 
    PIN_Configure(0, 2, 1, 0, 0); 
    PIN_Configure(0, 3, 1, 0, 0); 

    LPC_UART0->LCR = 3 | (1 << 7); 
    LPC_UART0->DLM = 0;
    LPC_UART0->DLL = 27; 
    LPC_UART0->LCR &= ~(1 << 7);
    LPC_UART0->FCR = 1;

    LPC_SC->PCLKSEL0 &= ~(3 << 6); 
}

void UART0_TransmitChar(char ch) {
    while (!(LPC_UART0->LSR & (1 << 5)));
    LPC_UART0->THR = ch;
}

void UART0_Send(char *str) {
    while (*str != '\0') {        
        UART0_TransmitChar(*str);  
        str++;                  
    }
}

uint8_t UART0_ReceiveChar(void) {
    while (!(LPC_UART0->LSR & 0x01)); 
    return LPC_UART0->RBR;
}


// ----- SPI & FLASH AT45 --------------------------------------------------------

uint8_t SPI_Transfer(uint8_t data) {
    LPC_SSP0->DR = data;                  
    while (LPC_SSP0->SR & (1 << 4));     
    return (uint8_t)LPC_SSP0->DR;        
}

void SPI_CS_Low(void) {
    LPC_GPIO0->FIOCLR = (1 << 16);
}

void SPI_CS_High(void) {
    LPC_GPIO0->FIOSET = (1 << 16);
}


void SSP0_Init(void) {
    LPC_SC->PCONP |= (1 << 21);

    PIN_Configure(0, 15, 2, 0, 0); // SCK0
    PIN_Configure(0, 17, 2, 0, 0); // MISO0
    PIN_Configure(0, 18, 2, 0, 0); // MOSI0
    PIN_Configure(0, 16, 0, 0, 0); // CS
    PIN_Configure(0, FLASH_RESET_PIN, 0, 0, 0);
    PIN_Configure(4, FLASH_WP_PIN, 0, 0, 0);

    LPC_GPIO0->FIODIR |= (1 << 16) | (1 << FLASH_RESET_PIN);
    LPC_GPIO4->FIODIR |= (1 << FLASH_WP_PIN);

    SPI_CS_High();
    LPC_GPIO4->FIOSET = (1 << FLASH_WP_PIN);  

    LPC_GPIO0->FIOCLR = (1 << FLASH_RESET_PIN);
    delay(20000);
    LPC_GPIO0->FIOSET = (1 << FLASH_RESET_PIN);
    delay(80000);

    LPC_SC->PCLKSEL1 &= ~(3 << 10);  // PCLK_SSP0 = CCLK/4 = 25 MHz
    LPC_SSP0->CPSR = 2;               // f_SSP = 25 MHz / 2 = 12.5 MHz
    LPC_SSP0->CR0  = 7;               // 8-bit, SPI Mode 0
    LPC_SSP0->CR1  = (1 << 1);
}

#define UART_TIMEOUT_CYCLES  5000000UL  // ~200ms przy 100MHz, dostraj empirycznie

static uint8_t UART0_ReceiveChar_Timeout(bool *timed_out) {
    uint32_t t = 0;
    while (!(LPC_UART0->LSR & 0x01)) {
        if (++t > UART_TIMEOUT_CYCLES) {
            *timed_out = true;
            return 0;
        }
    }
    *timed_out = false;
    return LPC_UART0->RBR;
}


void UploadMusic(void) {
    uint8_t page_buf[256];
    uint16_t current_page = 0;
    bool timed_out = false;

    while (LPC_UART0->LSR & 0x01) {
        volatile uint8_t dummy = LPC_UART0->RBR;
    }

    UART0_Send("TRYB WGRYWANIA! Czekam na plik z komputera...\r\n");

    while (current_page < (TRACK_COUNT * 512)) {
        for (int i = 0; i < 256; i++) {
            if (i == 0) {
                uint32_t t = 0;
                while (!(LPC_UART0->LSR & 0x01)) {
                    if (current_page == 0) {
                        continue;
                    } else {
                        if (++t > 60000000UL) {
                            goto upload_done; 
                        }
                    }
                }
                page_buf[i] = LPC_UART0->RBR;
            } else {
                page_buf[i] = UART0_ReceiveChar_Timeout(&timed_out);
                if (timed_out) {
                    UART0_Send("\r\nBLAD: Timeout w srodku strony!\r\n");
                    goto upload_done;
                }
            }
        }

        AT45_WriteBuffer(AT45_BUFFER_1, 0, page_buf, 256);
        AT45_BufferToFlash(current_page, AT45_BUFFER_1, true);
        while (!AT45_IsReady());

        UART0_TransmitChar('O');
        UART0_TransmitChar('K');

        current_page++;
    }

upload_done:;

    static uint8_t probe[16];
    static char dbg[100];
    for (int t = 0; t < TRACK_COUNT; t++) {
        uint16_t deep_page = track_start_page[t] + 100;
        AT45_ReadMainMemory(deep_page, 0, probe, 16, AT45_READ_LOW_FREQ);
        sprintf(dbg, "Track %d @page %d: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                t, deep_page,
                probe[0], probe[1], probe[2], probe[3],
                probe[4], probe[5], probe[6], probe[7]);
        UART0_Send(dbg);
    }

    UART0_Send("\r\nZAKONCZONE WGRYWANIE!\r\n");
}



// ----- FUNKCJE GRAFICZNE LCD ---------------------------------------------

void PutPixel(uint16_t x, uint16_t y, uint16_t color) {
    lcdWriteReg(ADRX_RAM, x);
    lcdWriteReg(ADRY_RAM, y);
    lcdWriteIndex(DATA_RAM);  
    lcdWriteData(color);      
}

void BresenhamLine(const int x1, const int y1, const int x2, const int y2, uint16_t color) {
    int d, dx, dy, ai, bi, xi, yi;
    int x = x1, y = y1;

    if (x1 < x2) { xi = 1; dx = x2 - x1; }
    else { xi = -1; dx = x1 - x2; }

    if (y1 < y2) { yi = 1; dy = y2 - y1; }
    else { yi = -1; dy = y1 - y2; }

    PutPixel(x, y, color);

    if (dx > dy) {
        ai = (dy - dx) * 2;
        bi = dy * 2;
        d = bi - dx;
        while (x != x2) {
            if (d >= 0) { x += xi; y += yi; d += ai; }
            else { d += bi; x += xi; }
            PutPixel(x, y, color);
        }
    } else {
        ai = (dx - dy) * 2;
        bi = dx * 2;
        d = bi - dy;
        while (y != y2) {
            if (d >= 0) { x += xi; y += yi; d += ai; }
            else { d += bi; y += yi; }
            PutPixel(x, y, color);
        }
    }
}

void DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    BresenhamLine(x1, y1, x2, y1, color); // gora
    BresenhamLine(x2, y1, x2, y2, color); // prawo
    BresenhamLine(x2, y2, x1, y2, color); // dol
    BresenhamLine(x1, y2, x1, y1, color); // lewo
}

void LCD_DisplayChar(uint16_t x, uint16_t y, uint8_t ascii, uint16_t color, uint16_t background) {
    uint8_t buffer[16];
    uint8_t i, j;
    
    GetASCIICode(ASCII_8X16_System, buffer, ascii);
    
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 8; j++) {
            if ((buffer[i] >> (7 - j)) & 0x01) {
                PutPixel(x + j, y + i, color);
            } else {
                PutPixel(x + j, y + i, background);
            }
        }
    }
}

void LCD_DisplayString(uint16_t x, uint16_t y, char* ptr, uint16_t color, uint16_t background){
    while(*ptr != '\0'){
        LCD_DisplayChar(x,y,*ptr,color,background);
        x += 8;
        ptr++;
    }
}

void LCD_Clear(uint16_t color) {
    lcdWriteReg(0x0050, 0); // Window Horizontal Start
    lcdWriteReg(0x0051, 239); // Window Horizontal End
    lcdWriteReg(0x0052, 0); // Window Vertical Start
    lcdWriteReg(0x0053, 319); // Window Vertical End
    
    lcdWriteReg(ADRX_RAM, 0);
    lcdWriteReg(ADRY_RAM, 0);
    lcdWriteIndex(DATA_RAM);
    
    for(uint32_t i = 0; i < 76800; i++) {
        lcdWriteData(color);
    }
}

void DrawProgressBar(uint16_t current_page, uint16_t start_page, 
                     uint16_t length_pages) {
    const uint16_t BAR_X = 10;
    const uint16_t BAR_Y = 280;
    const uint16_t BAR_W = 220;
    const uint16_t BAR_H = 8;
    
    DrawRectangle(BAR_X, BAR_Y, BAR_X + BAR_W, BAR_Y + BAR_H, LCDWhite);
    
    if (length_pages == 0) return;
    
    uint16_t filled = (uint16_t)(((uint32_t)(current_page - start_page) 
                       * BAR_W) / length_pages);
    if (filled > BAR_W) filled = BAR_W;
    
    for (uint16_t x = BAR_X + 1; x < BAR_X + BAR_W; x++) {
        uint16_t color = (x < BAR_X + filled) ? LCDGreen : LCDBlack;
        for (uint16_t y = BAR_Y + 1; y < BAR_Y + BAR_H; y++) {
            PutPixel(x, y, color);
        }
    }
}


// ----- UI ---------------------------------------------------------------

void DrawMusicPlayerUI(void) {
    LCD_DisplayString(40, 20, "ODTWARZACZ WAV", LCDWhite, LCDBlack);
    
    LCD_DisplayString(20, 90, "Status: STOP", LCDRed, LCDBlack);

    // Lewy
    DrawRectangle(20, 200, 70, 250, LCDWhite);
    LCD_DisplayString(30, 217, "<<", LCDWhite, LCDBlack);

    // Srodek
    DrawRectangle(90, 200, 150, 250, LCDWhite);
    LCD_DisplayString(105, 217, "PLAY", LCDWhite, LCDBlack);

    // Prawy
    DrawRectangle(170, 200, 220, 250, LCDWhite);
    LCD_DisplayString(180, 217, ">>", LCDWhite, LCDBlack);
}

// ----- DAC ---------------------------------------------------------------

void DAC_Init_SmokeTest(void) {
    // 1. PINSEL1 
    LPC_PINCON->PINSEL1 &= ~(3 << 20); 
    LPC_PINCON->PINSEL1 |=  (2 << 20);

    // 2. PINMODE1 
    LPC_PINCON->PINMODE1 &= ~(3 << 20);
    LPC_PINCON->PINMODE1 |=  (2 << 20); 
}


volatile bool     g_track_changed  = false;  
volatile uint16_t g_current_page   = 0;
volatile uint16_t g_byte_offset    = 0;
volatile bool     g_buffer_loaded  = false;
volatile bool     g_play           = false;
volatile int      g_current_track  = 0;
volatile uint8_t  g_page_buffer[256];

void Timer0_Init(void) {
    LPC_SC->PCONP |= (1 << 1);          // wlacz zasilanie Timer0
    LPC_SC->PCLKSEL0 &= ~(3 << 2);      // PCLK_TIMER0 = CCLK/4 = 25 MHz
    LPC_TIM0->TCR  = 0x02;              // reset
    LPC_TIM0->PR   = 0;                 // prescaler = 0 (taktowanie co 1 cykl PCLK)
    LPC_TIM0->MR0  = 3124;              // 25 000 000 / (3124+1) = 8000 Hz
    LPC_TIM0->MCR  = 0x03;              // interrupt + reset przy MR0
    LPC_TIM0->TCR  = 0x01;              // start
    NVIC_EnableIRQ(TIMER0_IRQn);
}

void TIMER0_IRQHandler(void) {
    LPC_TIM0->IR = 0x01;  

    if (!g_play || g_current_track >= 5 ||
        track_length_pages[g_current_track] == 0) {
        LPC_DAC->DACR = (512 << 6);  
        return;
    }

    if (!g_buffer_loaded) {
        LPC_DAC->DACR = (512 << 6);
        return;
    }

    LPC_DAC->DACR = ((uint32_t)g_page_buffer[g_byte_offset] << 2) << 6;
    g_byte_offset++;

    if (g_byte_offset >= 256) {
        g_byte_offset   = 0;
        g_buffer_loaded = false;
        g_current_page++;

        if (g_current_page >= track_start_page[g_current_track]
                            + track_length_pages[g_current_track]) {
            g_current_track = (g_current_track + 1) % TRACK_COUNT;
            g_current_page  = track_start_page[g_current_track];
            g_track_changed = true; 
        }
    }
}

// ----- MAIN ---------------------------------------------------------------
int main(void) {
    uint8_t id[4] = {0};
    char buffer[64];

    SystemInit();
    UART0_Init();
    SSP0_Init();
    
		AT45_WakeUp();
		delay(50000);

		uint8_t st = AT45_GetStatus();
		if (!(st & 0x01)) {
				AT45_ConfigureBinaryPageSize();
				UART0_Send("UWAGA: Flash skonfigurowany na 256B, wymagany power cycle!\r\n");
				while(1); 
		}
		
    DAC_Init_SmokeTest();
    Timer0_Init();
    
    lcdConfiguration();
    init_ILI9325();
    touchpanelInit();
    LCD_Clear(LCDBlack);

    delay(50000);
    UART0_Send("--- System Rozpoczal Prace ---\r\n");

    AT45_GetDeviceID(id);
    sprintf(buffer, "Flash ID: %02X %02X %02X %02X\r\n", 
            id[0], id[1], id[2], id[3]);
    UART0_Send(buffer);

    DrawMusicPlayerUI();
    UART0_Send("UI Zbudowane. Czekam na dotyk...\r\n");

    int tp_x, tp_y;
    int lcd_x, lcd_y;
    long sum_x, sum_y; 
    const int samples = 10;        
    char track_str[32];

    g_current_track = 0;
    g_current_page  = track_start_page[0];
    g_byte_offset   = 0;
    g_buffer_loaded = false;
    g_play          = false;

    sprintf(track_str, "Utwor: %d/%d ", g_current_track + 1, TRACK_COUNT);
    LCD_DisplayString(20, 60, track_str, LCDWhite, LCDBlack);
    DrawProgressBar(g_current_page, 
                    track_start_page[g_current_track],
                    track_length_pages[g_current_track]);

    while (1) {
		
		if (LPC_UART0->LSR & 0x01) { 
            uint8_t cmd = LPC_UART0->RBR;
            if (cmd == 'U' || cmd == 'u') {  
                g_play = false;              
                LPC_DAC->DACR = (512 << 6);  
                
                UploadMusic();              
                
                g_current_track = 0;
                g_current_page  = track_start_page[0];
                g_byte_offset   = 0;
                g_buffer_loaded = false;
              
                LCD_Clear(LCDBlack);
                DrawMusicPlayerUI();
                sprintf(track_str, "Utwor: %d/%d ", g_current_track + 1, TRACK_COUNT);
                LCD_DisplayString(20, 60, track_str, LCDWhite, LCDBlack);
                DrawProgressBar(g_current_page, track_start_page[g_current_track], track_length_pages[g_current_track]);
            }
        }

        // --- Ladowanie strony w tle (ISR timera odtwarza, main laduje) ---
        if (g_play && !g_buffer_loaded) {
            AT45_ReadMainMemory(g_current_page, 0,
                                (uint8_t*)g_page_buffer,
                                256, AT45_READ_LOW_FREQ);
            g_buffer_loaded = true;

            if ((g_current_page % 8) == 0) {
                DrawProgressBar(g_current_page,
                                track_start_page[g_current_track],
                                track_length_pages[g_current_track]);
            }
        }

        // --- ISR zglosila zmiane utworu, odswiez LCD ---
        if (g_track_changed) {
            g_track_changed = false;
            sprintf(track_str, "Utwor: %d/%d ", g_current_track + 1, TRACK_COUNT);
            LCD_DisplayString(20, 60, track_str, LCDWhite, LCDBlack);
            DrawProgressBar(g_current_page,
                            track_start_page[g_current_track],
                            track_length_pages[g_current_track]);
            UART0_Send("Auto: nastepny utwor\r\n");
        }

        if (!g_play) {
            delay(1000);
        }

        // --- Odczyt dotyku ---
        if (!(LPC_GPIO0->FIOPIN & PIN_TP_INT)) {
            sum_x = 0;
            sum_y = 0;

            for (int i = 0; i < samples; i++) {
                sum_x += touchpanelReadX();
                sum_y += touchpanelReadY();
            }

            tp_x = sum_x / samples;
            tp_y = sum_y / samples;

			lcd_x = ((tp_y - TP_MIN) * 240 / (TP_MAX - TP_MIN));
			lcd_y = ((tp_x - TP_MIN) * 320 / (TP_MAX - TP_MIN));

            if (lcd_x < 0) lcd_x = 0; if (lcd_x > 239) lcd_x = 239;
            if (lcd_y < 0) lcd_y = 0; if (lcd_y > 319) lcd_y = 319;

            if (lcd_y >= 200 && lcd_y <= 250) {

                if (lcd_x >= 20 && lcd_x <= 70) {
                    // PREV
                    UART0_Send("Kliknieto: PREV\r\n");
                    g_current_track = (g_current_track - 1 + TRACK_COUNT) % TRACK_COUNT;
                    g_current_page  = track_start_page[g_current_track];
                    g_byte_offset   = 0;
                    g_buffer_loaded = false;
                    sprintf(track_str, "Utwor: %d/%d ", 
                            g_current_track + 1, TRACK_COUNT);
                    LCD_DisplayString(20, 60, track_str, 
                                      LCDWhite, LCDBlack);
                    DrawProgressBar(g_current_page,
                                    track_start_page[g_current_track],
                                    track_length_pages[g_current_track]);
                }
                else if (lcd_x >= 90 && lcd_x <= 150) {
                    // PLAY/STOP
                    UART0_Send("Kliknieto: PLAY/STOP\r\n");
                    if (!g_play) {
                        LCD_DisplayString(20, 90, "Status: PLAY", 
                                          LCDGreen, LCDBlack);
                        g_play = true;
                    } else {
                        LCD_DisplayString(20, 90, "Status: STOP", 
                                          LCDRed, LCDBlack);
                        g_play = false;
                        LPC_DAC->DACR = (512 << 6);
                    }
                }
                else if (lcd_x >= 170 && lcd_x <= 220) {
                    // NEXT
                    UART0_Send("Kliknieto: NEXT\r\n");
                    g_current_track = (g_current_track + 1) % TRACK_COUNT;
                    g_current_page  = track_start_page[g_current_track];
                    g_byte_offset   = 0;
                    g_buffer_loaded = false;
                    sprintf(track_str, "Utwor: %d/%d ", 
                            g_current_track + 1, TRACK_COUNT);
                    LCD_DisplayString(20, 60, track_str, 
                                      LCDWhite, LCDBlack);
                    DrawProgressBar(g_current_page,
                                    track_start_page[g_current_track],
                                    track_length_pages[g_current_track]);
                }

                delay(2000000); // debounce
            }
        }
    }
}
