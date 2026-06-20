#include "at45db041d.h"
#include <string.h>

extern uint8_t SPI_Transfer(uint8_t data);
extern void SPI_CS_Low(void);
extern void SPI_CS_High(void);


static void _AssembleAddress(AT45_Device_t *dev, uint16_t page,
                              uint16_t offset, uint8_t *addrBuf) {
    if (dev->isBinaryMode) {
        addrBuf[0] = (uint8_t)((page >> 8) & 0x07);  // 5 don't care + A18:A16
        addrBuf[1] = (uint8_t)(page & 0xFF);          // A15:A8
        addrBuf[2] = (uint8_t)(offset & 0xFF);        // A7:A0
    } else {
        addrBuf[0] = (uint8_t)((page >> 5) & 0x0F);  // 4 don't care + PA10:PA8
        addrBuf[1] = (uint8_t)((page << 3) | (offset >> 6));
        addrBuf[2] = (uint8_t)(offset & 0xFF);
    }
}

static AT45_Device_t _GetCurrentDev(void) {
    uint8_t st = AT45_GetStatus();
    AT45_Device_t dev;
    dev.isBinaryMode = (st & AT45_STATUS_PAGE_SIZE) ? true : false;
    dev.pageSize     = dev.isBinaryMode ? AT45_PAGE_SIZE_BINARY
                                        : AT45_PAGE_SIZE_DEFAULT;
    dev.pagesCount   = AT45_PAGES_COUNT;
    return dev;
}


AT45_Status_t AT45_Init(AT45_Device_t *dev) {
    uint8_t status = AT45_GetStatus();
    if (((status & AT45_STATUS_DENSITY) >> 2) != 0x07)
        return AT45_ERROR_SPI;
    dev->isBinaryMode = (status & AT45_STATUS_PAGE_SIZE) ? true : false;
    dev->pageSize     = dev->isBinaryMode ? AT45_PAGE_SIZE_BINARY
                                          : AT45_PAGE_SIZE_DEFAULT;
    dev->pagesCount   = AT45_PAGES_COUNT;
    return AT45_OK;
}

uint8_t AT45_GetStatus(void) {
    uint8_t status;
    SPI_CS_Low();
    SPI_Transfer(0xD7);
    status = SPI_Transfer(0xFF);
    SPI_CS_High();
    return status;
}

bool AT45_IsReady(void) {
    return (AT45_GetStatus() & AT45_STATUS_READY) ? true : false;
}

void AT45_GetDeviceID(uint8_t id_buf[4]) {
    SPI_CS_Low();
    SPI_Transfer(0x9F);
    for (int i = 0; i < 4; i++)
        id_buf[i] = SPI_Transfer(0xFF);
    SPI_CS_High();
}


AT45_Status_t AT45_ReadMainMemory(uint16_t page, uint16_t offset,
                                   uint8_t *data, uint16_t len,
                                   AT45_ReadType_t type) {
    if (page >= AT45_PAGES_COUNT) return AT45_ERROR_ADDR;
    uint8_t addr[3];
    AT45_Device_t dev = _GetCurrentDev();
    _AssembleAddress(&dev, page, offset, addr);

    while (!AT45_IsReady());

    SPI_CS_Low();
    SPI_Transfer((uint8_t)type);
    for (int i = 0; i < 3; i++) SPI_Transfer(addr[i]);

    if (type == AT45_READ_HIGH_FREQ)                      
        SPI_Transfer(0x00);
    else if (type == AT45_READ_LEGACY)                    
        for (int i = 0; i < 4; i++) SPI_Transfer(0x00);

    for (uint16_t i = 0; i < len; i++)
        data[i] = SPI_Transfer(0xFF);
    SPI_CS_High();
    return AT45_OK;
}

AT45_Status_t AT45_ReadBuffer(AT45_Buffer_t buffer, uint16_t offset,
                               uint8_t *data, uint16_t len) {
    uint8_t cmd = (buffer == AT45_BUFFER_1) ? 0xD4 : 0xD6;
    SPI_CS_Low();
    SPI_Transfer(cmd);
    SPI_Transfer(0x00);            
    SPI_Transfer((uint8_t)(offset >> 8));
    SPI_Transfer((uint8_t)offset);
    SPI_Transfer(0x00);             
    for (uint16_t i = 0; i < len; i++)
        data[i] = SPI_Transfer(0xFF);
    SPI_CS_High();
    return AT45_OK;
}


AT45_Status_t AT45_WriteBuffer(AT45_Buffer_t buffer, uint16_t offset,
                                uint8_t *data, uint16_t len) {
    uint8_t cmd = (buffer == AT45_BUFFER_1) ? 0x84 : 0x87;
    while (!AT45_IsReady());
    SPI_CS_Low();
    SPI_Transfer(cmd);
    SPI_Transfer(0x00);
    SPI_Transfer((uint8_t)(offset >> 8));
    SPI_Transfer((uint8_t)offset);
    for (uint16_t i = 0; i < len; i++)
        SPI_Transfer(data[i]);
    SPI_CS_High();
    return AT45_OK;
}

AT45_Status_t AT45_WriteMainMemory(uint16_t page, uint16_t offset,
                                    uint8_t *data, uint16_t len,
                                    AT45_Buffer_t buffer) {
    uint8_t cmd = (buffer == AT45_BUFFER_1) ? 0x82 : 0x85;
    uint8_t addr[3];
    AT45_Device_t dev = _GetCurrentDev();
    _AssembleAddress(&dev, page, offset, addr);

    while (!AT45_IsReady());
    SPI_CS_Low();
    SPI_Transfer(cmd);
    for (int i = 0; i < 3; i++) SPI_Transfer(addr[i]);
    for (uint16_t i = 0; i < len; i++)
        SPI_Transfer(data[i]);
    SPI_CS_High();
    return AT45_OK;
}


AT45_Status_t AT45_FlashToBuffer(uint16_t page, AT45_Buffer_t buffer) {
    uint8_t cmd = (buffer == AT45_BUFFER_1) ? 0x53 : 0x55;
    uint8_t addr[3];
    AT45_Device_t dev = _GetCurrentDev();
    _AssembleAddress(&dev, page, 0, addr);

    while (!AT45_IsReady());
    SPI_CS_Low();
    SPI_Transfer(cmd);
    for (int i = 0; i < 3; i++) SPI_Transfer(addr[i]);
    SPI_CS_High();
    return AT45_OK;
}

AT45_Status_t AT45_BufferToFlash(uint16_t page, AT45_Buffer_t buffer,
                                  bool eraseFirst) {
    uint8_t cmd;
    if (buffer == AT45_BUFFER_1)
        cmd = eraseFirst ? 0x83 : 0x88;
    else
        cmd = eraseFirst ? 0x86 : 0x89;

    uint8_t addr[3];
    AT45_Device_t dev = _GetCurrentDev();
    _AssembleAddress(&dev, page, 0, addr);

    while (!AT45_IsReady());
    SPI_CS_Low();
    SPI_Transfer(cmd);
    for (int i = 0; i < 3; i++) SPI_Transfer(addr[i]);
    SPI_CS_High();
    return AT45_OK;
}

AT45_Status_t AT45_CompareBufferToFlash(uint16_t page, AT45_Buffer_t buffer) {
    uint8_t cmd = (buffer == AT45_BUFFER_1) ? 0x60 : 0x61;
    uint8_t addr[3];
    AT45_Device_t dev = _GetCurrentDev();
    _AssembleAddress(&dev, page, 0, addr);

    while (!AT45_IsReady());
    SPI_CS_Low();
    SPI_Transfer(cmd);
    for (int i = 0; i < 3; i++) SPI_Transfer(addr[i]);
    SPI_CS_High();

    while (!AT45_IsReady()); 
    return (AT45_GetStatus() & AT45_STATUS_COMP) ? AT45_ERROR_VERIFY : AT45_OK;
}


AT45_Status_t AT45_PageErase(uint16_t page) {
    if (page >= AT45_PAGES_COUNT) return AT45_ERROR_ADDR;
    uint8_t addr[3];
    AT45_Device_t dev = _GetCurrentDev();
    _AssembleAddress(&dev, page, 0, addr);

    while (!AT45_IsReady());
    SPI_CS_Low();
    SPI_Transfer(0x81);
    for (int i = 0; i < 3; i++) SPI_Transfer(addr[i]);
    SPI_CS_High();
    return AT45_OK;
}

AT45_Status_t AT45_BlockErase(uint16_t block) {
    if (block >= AT45_BLOCKS_COUNT) return AT45_ERROR_ADDR;
    uint16_t page = block << 3; 
    uint8_t addr[3];
    AT45_Device_t dev = _GetCurrentDev();
    _AssembleAddress(&dev, page, 0, addr);

    while (!AT45_IsReady());
    SPI_CS_Low();
    SPI_Transfer(0x50);
    for (int i = 0; i < 3; i++) SPI_Transfer(addr[i]);
    SPI_CS_High();
    return AT45_OK;
}

AT45_Status_t AT45_ChipErase(void) {
    uint8_t id[4];
    AT45_GetDeviceID(id);
    (void)id;

    const uint8_t cmd[] = {0xC7, 0x94, 0x80, 0x9A};
    while (!AT45_IsReady());
    SPI_CS_Low();
    for (int i = 0; i < 4; i++) SPI_Transfer(cmd[i]);
    SPI_CS_High();
    return AT45_OK;
}

AT45_Status_t AT45_ChipErase_Safe(void) {
    for (uint16_t block = 0; block < AT45_BLOCKS_COUNT; block++) {
        AT45_Status_t st = AT45_BlockErase(block);
        if (st != AT45_OK) return st;
        while (!AT45_IsReady());
    }
    return AT45_OK;
}


AT45_Status_t AT45_ReadModifyWrite(uint16_t page, uint16_t offset,
                                    uint8_t *data, uint16_t len,
                                    AT45_Buffer_t buffer) {
    AT45_FlashToBuffer(page, buffer);
    while (!AT45_IsReady());
    AT45_WriteBuffer(buffer, offset, data, len);
    return AT45_BufferToFlash(page, buffer, true);
}

AT45_Status_t AT45_ConfigureBinaryPageSize(void) {
    const uint8_t cmd[] = {0x3D, 0x2A, 0x80, 0xA6};
    while (!AT45_IsReady());
    SPI_CS_Low();
    for (int i = 0; i < 4; i++) SPI_Transfer(cmd[i]);
    SPI_CS_High();
    return AT45_OK;
}



void AT45_PowerDown(bool deep) {
    (void)deep; 
    SPI_CS_Low();
    SPI_Transfer(0xB9); // Deep Power-Down
    SPI_CS_High();
}

void AT45_WakeUp(void) {
    SPI_CS_Low();
    SPI_Transfer(0xAB); // Resume from Deep Power-Down
    SPI_CS_High();
}



void AT45_ContinuousReadStart(uint16_t page, uint16_t offset) {
    uint8_t addr[3];
    AT45_Device_t dev = _GetCurrentDev();
    _AssembleAddress(&dev, page, offset, addr);

    SPI_CS_Low();
    SPI_Transfer(0x0B); // High Freq Continuous Read
    for (int i = 0; i < 3; i++) SPI_Transfer(addr[i]);
    SPI_Transfer(0x00); 
}

void AT45_ContinuousReadStop(void) {
    SPI_CS_High();
}