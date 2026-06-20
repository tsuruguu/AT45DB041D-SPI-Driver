#ifndef AT45DB041D_H
#define AT45DB041D_H

#include <stdint.h>
#include <stdbool.h>


typedef enum {
    AT45_OK = 0,
    AT45_ERROR_SPI,
    AT45_ERROR_ADDR,
    AT45_ERROR_CHIP_BUSY,
    AT45_ERROR_VERIFY
} AT45_Status_t;


#define AT45_PAGE_SIZE_DEFAULT  264
#define AT45_PAGE_SIZE_BINARY   256
#define AT45_PAGES_COUNT        2048
#define AT45_BLOCKS_COUNT       256  

#define AT45_STATUS_READY       (1 << 7) 
#define AT45_STATUS_COMP        (1 << 6) 
#define AT45_STATUS_DENSITY     (0x0F << 2) 
#define AT45_STATUS_PROTECT     (1 << 1) 
#define AT45_STATUS_PAGE_SIZE   (1 << 0) 


typedef enum {
    AT45_READ_LOW_FREQ       = 0x03, 
    AT45_READ_HIGH_FREQ      = 0x0B,
    AT45_READ_LEGACY         = 0xE8  
} AT45_ReadType_t;

typedef enum {
    AT45_BUFFER_1 = 1,
    AT45_BUFFER_2 = 2
} AT45_Buffer_t;


typedef struct {
    uint16_t pageSize;      
    uint16_t pagesCount;    
    bool isBinaryMode;      
} AT45_Device_t;


AT45_Status_t AT45_Init(AT45_Device_t *dev);
uint8_t       AT45_GetStatus(void);
bool          AT45_IsReady(void);
void          AT45_GetDeviceID(uint8_t id_buf[4]);


AT45_Status_t AT45_ReadMainMemory(uint16_t page, uint16_t offset, uint8_t *data, uint16_t len, AT45_ReadType_t type);

AT45_Status_t AT45_WriteMainMemory(uint16_t page, uint16_t offset, uint8_t *data, uint16_t len, AT45_Buffer_t buffer);

AT45_Status_t AT45_WriteBuffer(AT45_Buffer_t buffer, uint16_t offset, uint8_t *data, uint16_t len);
AT45_Status_t AT45_ReadBuffer(AT45_Buffer_t buffer, uint16_t offset, uint8_t *data, uint16_t len);


AT45_Status_t AT45_FlashToBuffer(uint16_t page, AT45_Buffer_t buffer);

AT45_Status_t AT45_BufferToFlash(uint16_t page, AT45_Buffer_t buffer, bool eraseFirst);

AT45_Status_t AT45_CompareBufferToFlash(uint16_t page, AT45_Buffer_t buffer);


AT45_Status_t AT45_PageErase(uint16_t page);
AT45_Status_t AT45_BlockErase(uint16_t block);
AT45_Status_t AT45_ChipErase(void);
AT45_Status_t AT45_ChipErase_Safe(void);

AT45_Status_t AT45_ReadModifyWrite(uint16_t page, uint16_t offset, uint8_t *data, uint16_t len, AT45_Buffer_t buffer);

void AT45_PowerDown(bool deep);
void AT45_WakeUp(void);

AT45_Status_t AT45_ConfigureBinaryPageSize(void);

void AT45_ContinuousReadStart(uint16_t page, uint16_t offset);
void AT45_ContinuousReadStop(void);

#endif