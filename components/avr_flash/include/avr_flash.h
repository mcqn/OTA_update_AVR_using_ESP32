#ifndef _AVR_FLASH_H
#define _AVR_FLASH_H

#include "avr_pro_mode.h"

// Error codes from the flashing
#define EFLASH_FAIL 100
#define EPROGMODE_FAIL  101
#define ESYNC_FAIL  102
#define EREAD_FAIL  103
#define EVERIFY_FAIL    104
#define ELOAD_ADDR_FAIL 105

//Increment the memory address for the next write operation
void incrementLoadAddress(char *loadAddress);

//Send the client MCU the memory address, to be written
int loadAddress(char addressHigh, char addressLow);

//Compare the client's memory after flashing, with the 'page' of data for verification purposes
int compare(uint8_t page[], uint8_t block[], int offset);

//UART write the flash memory address of the client MCU with the data 
int flashPage(char *address, char *data);
int stk500v2FlashPage(uint32_t address, char *data);

/**
 * @brief Write the code into the flash memory of the client MCU
 * 
 * The 'page' of data, parsed from the .hex file, is written into the 
 * flash memory of the client, block-by-block 
 * 
 * @param page data parsed from the .hex file
 * @param block_count Total no. of blocks in page (sets of 128 bytes each)
 *   
 * @return ESP_OK - success, ESP_FAIL - failed
 */
esp_err_t writeTask(uint8_t page[], int block_count);

esp_err_t stk500v2WriteTask(uint8_t page[], int block_count);

/**
 * @brief Read the flash memory of the client MCU, for verification
 * 
 * It reads the flash memory of the client block-by-block and 
 * checks it with the 'page' of data intended to be written
 * 
 * @param page data parsed from the .hex file
 * @param block_count Total no. of blocks in page (sets of 128 bytes each)
 *   
 * @return ESP_OK - success, ESP_FAIL - failed
 */
esp_err_t readTask(uint8_t page[], int block_count);
esp_err_t stk500v2ReadTask(uint8_t page[], int block_count);

#endif
