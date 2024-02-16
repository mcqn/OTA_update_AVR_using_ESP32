#include "avr_flash.h"

//Functions for custom adjustments

static const char *TAG_AVR_FLASH = "avr_flash";

void incrementLoadAddress(char *loadAddress)
{
    loadAddress[1] += 0x40;
    if (loadAddress[1] == 0)
    {
        loadAddress[0] += 0x1;
    }
}

int loadAddress(char adrHi, char adrLo)
{
    char params[] = {adrHi, adrLo};
    return execParam(0x55, params, sizeof(params));
}

int compare(uint8_t page[], uint8_t block[], int offset)
{
    int c = 0;
    char temp[BLOCK_SIZE + 2];

    temp[0] = SYNC;
    temp[BLOCK_SIZE + 1] = OK;

    while (c < BLOCK_SIZE)
    {
        temp[c + 1] = page[offset + c];
        c++;
    }

    if (!memcmp(temp, block, BLOCK_SIZE + 2))
    {
        logI(TAG_AVR_FLASH, "%s", "Verification Success");
        return 1;
    }
    else
    {
        logI(TAG_AVR_FLASH, "%s", "Verification Failure");
        return 0;
    }
}

int stk500v2FlashPage(uint32_t address, char *data)
{
    char head[] = {0x13, (BLOCK_SIZE>>8), (BLOCK_SIZE & 0xff), 0xc1, 0x0a, 0x40, 0x4c, 0x20, 0x00, 0x00};
    //const char tail[] = {0x20};

    if (stk500v2LoadAddress(address))
    {
        //ESP_LOG_BUFFER_HEXDUMP(TAG_AVR_FLASH, data, BLOCK_SIZE, ESP_LOG_DEBUG);
        if (sendSTK500v2MessageWithData(head, sizeof(head), data, BLOCK_SIZE))
        {
            // Wait for a response
            char resp2[2];
            uint16_t size2 = 2;
            if (getSTK500v2Response(resp2, &size2))
            {
                // Got response
                if ((resp2[0] == 0x13) && (resp2[1] == 0x00))
                {
                    logI(TAG_AVR_FLASH, "%s", "Page written");
                    return 1;
                }
            }
        }
    }

    return 0;
}

int flashPage(char *address, char *data)
{
    const char head[] = {0x64, 0x00, 0x80, 0x46};
    const char tail[] = {0x20};

    loadAddress(address[1], address[0]);
    sendData(TAG_AVR_FLASH, head, sizeof(head));
    sendData(TAG_AVR_FLASH, data, BLOCK_SIZE);
    sendData(TAG_AVR_FLASH, tail, sizeof(tail));

    //ESP_LOG_BUFFER_HEXDUMP(TAG_AVR_FLASH, data, BLOCK_SIZE, ESP_LOG_DEBUG);

    int length = waitForSerialData(MIN_DELAY_MS, MAX_DELAY_MS);
    if (length > 0)
    {
        uint8_t data[length];
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, length, 1000 / portTICK_RATE_MS);
        if (rxBytes > 0)
        {
            if (data[0] == SYNC && data[1] == OK)
            {
                logI(TAG_AVR_FLASH, "%s", "Sync Success");
                return 1;
            }
            else
            {
                logE(TAG_AVR_FLASH, "%s", "Sync Failure");
                return 0;
            }
        }
    }
    else
    {
        logE(TAG_AVR_FLASH, "%s", "Serial Timeout");
    }

    return 0;
}

esp_err_t stk500v2WriteTask(uint8_t page[], int block_count)
{
    int page_start = 0, page_index = 0;

    char block[BLOCK_SIZE];
    uint32_t loadAddress = 0x80000000;

    resetMCU();
    if (stk500v2GetSync())
    {
        if (stk500v2EnterProgrammingMode())
        {
            while (block_count != 0)
            {
                logD(TAG_AVR_FLASH, "\nBlocks left: %d", block_count);
                memset(block, '\0', BLOCK_SIZE);
                int block_index = 0;

                while (page_index < page_start + BLOCK_SIZE)
                {
                    block[block_index] = page[page_index];
                    block_index++;
                    page_index++;
                }

                if (!stk500v2FlashPage(loadAddress, block))
                {
                    return -EFLASH_FAIL;
                }

                page_start += BLOCK_SIZE;
                block_count--;
                // BLOCK_SIZE is in bytes, address is in words
                loadAddress += BLOCK_SIZE/2;
            }
        }
        else
        {
            return -EPROGMODE_FAIL;
        }
    }
    else
    {
        return -ESYNC_FAIL;
    }

    return ESP_OK;
}

esp_err_t writeTask(uint8_t page[], int block_count)
{
    int page_start = 0, page_index = 0;

    char block[BLOCK_SIZE];
    char loadAddress[2] = {0x00, 0x00};

    setupDevice();

    while (block_count != 0)
    {
        logD(TAG_AVR_FLASH, "Blocks left: %d \n", block_count);
        memset(block, '\0', BLOCK_SIZE);
        int block_index = 0;

        while (page_index < page_start + BLOCK_SIZE)
        {
            block[block_index] = page[page_index];
            block_index++;
            page_index++;
        }

        if (!flashPage(loadAddress, block))
        {
            return ESP_FAIL;
        }

        page_start += BLOCK_SIZE;
        block_count--;

        incrementLoadAddress(loadAddress);
    }

    return ESP_OK;
}

esp_err_t stk500v2ReadTask(uint8_t page[], int block_count)
{
    const char head[] = {0x14, (BLOCK_SIZE >> 8), (BLOCK_SIZE & 0Xff), 0x20};
    uint32_t readAddress = 0x80000000;

    int offset = 0;
    char block[BLOCK_SIZE+3];  // Include space for the surrounding response message

    while (block_count != 0)
    {
        logD(TAG_AVR_FLASH, "Blocks left: %d \n", block_count);
        if (stk500v2LoadAddress(readAddress))
        {
            if (sendSTK500v2Message(head, sizeof(head)))
            {
                memset(block, '\0', BLOCK_SIZE+3);
                uint16_t size = BLOCK_SIZE+3;
                if (getSTK500v2Response(block, &size))
                {
                    if (block[0] == 0x14)
                    {
                        logI(TAG_AVR_FLASH, "%s", "Read Success");
                        if (memcmp(&page[offset], &block[2], BLOCK_SIZE))
                        {
                            return -EVERIFY_FAIL;
                        }

                        offset += BLOCK_SIZE;
                    }
                }
                else
                {
                    logE(TAG_AVR_FLASH, "%s", "Failed to read page");
                    return -EREAD_FAIL;
                }
            }
        }
        else
        {
            logE(TAG_AVR_FLASH, "%s", "Failed to load address for read");
            return -ELOAD_ADDR_FAIL;
        }

        // BLOCK_SIZE is in bytes, address is in words
        readAddress += BLOCK_SIZE/2;
        block_count--;
    }

    stk500v2LeaveProgrammingMode();
    return ESP_OK;
}

esp_err_t readTask(uint8_t page[], int block_count)
{
    const char head[] = {0x74, 0x00, 0x80, 0x46};
    const char tail[] = {0x20};
    char readAddress[2] = {0x00, 0x00};

    int offset = 0;

    while (block_count != 0)
    {
        logD(TAG_AVR_FLASH, "Blocks left: %d \n", block_count);
        loadAddress(readAddress[1], readAddress[0]);
        sendData(TAG_AVR_FLASH, head, sizeof(head));
        sendData(TAG_AVR_FLASH, tail, sizeof(tail));

        int length = waitForSerialData(MIN_DELAY_MS, MAX_DELAY_MS);
        if (length > 0)
        {
            uint8_t data[length];
            const int rxBytes = uart_read_bytes(UART_NUM_1, data, length, 1000 / portTICK_RATE_MS);
            if (rxBytes > 0)
            {
                if (data[0] == SYNC && data[BLOCK_SIZE + 1] == OK)
                {
                    logI(TAG_AVR_FLASH, "%s", "Sync Success");
                    if (!compare(page, data, offset))
                    {
                        return ESP_FAIL;
                    }

                    offset += BLOCK_SIZE;
                }
                else
                {
                    logE(TAG_AVR_FLASH, "%s", "Sync Failure");
                    return ESP_FAIL;
                }
            }
        }
        else
        {
            logE(TAG_AVR_FLASH, "%s", "Serial Timeout");
            return ESP_FAIL;
        }

        incrementLoadAddress(readAddress);
        block_count--;
    }

    return ESP_OK;
}

