/**
 * Following are the Reference links for the code
 * ['esp_avr_programmer' by rene-win](https://github.com/rene-win/esp_avr_programmer)
 * 
 * [STK500 AVR Protocol](http://www.amelek.gda.pl/avr/uisp/doc2525.pdf)
 */

#include "avr_pro_mode.h"

static const char *TAG_AVR_PRO = "avr_pro_mode";

//Functions for custom adjustments
void initUART(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);

    logI(TAG_AVR_PRO, "%s", "UART initialized");
}

void initGPIO(void)
{
    gpio_set_direction(RESET_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RESET_PIN, HIGH);
    logI(TAG_AVR_PRO, "%s", "GPIO initialized");
}

void initSPIFFS(void)
{
    logI(TAG_AVR_PRO, "%s", "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            logE(TAG_AVR_PRO, "%s", "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            logE(TAG_AVR_PRO, "%s", "Failed to find SPIFFS partition");
        }
        else
        {
            logE(TAG_AVR_PRO, "%s", "Failed to initialize SPIFFS");
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK)
    {
        logE(TAG_AVR_PRO, "%s", "Failed to get SPIFFS partition information");
    }
    else
    {
        logD(TAG_AVR_PRO, "Partition size: total: %d, used: %d", total, used);
    }
}

void resetMCU(void)
{
    logI(TAG_AVR_PRO, "%s", "Starting Reset Procedure");

    gpio_set_level(RESET_PIN, LOW);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    gpio_set_level(RESET_PIN, HIGH);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(RESET_PIN, LOW);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    gpio_set_level(RESET_PIN, HIGH);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    logI(TAG_AVR_PRO, "%s", "Reset Procedure finished");
}

void setupDevice(void)
{
    resetMCU();
    getSync();
    setProgParams();
    setExtProgParams();
    enterProgMode();
}

void endConn(void)
{
    extProgMode();
    resetMCU();
}

int getSync(void)
{
    logI(TAG_AVR_PRO, "%s", "Synchronizing");
    return execCmd(0x30);
}

int stk500v2GetSync(void)
{
    logI(TAG_AVR_PRO, "%s", __FUNCTION__);
    char b[] = { 0x01 };
    int tries = 0;
    while (tries++ < 5)
    {
        // Clear any previous data from the receive buffer first
        uart_flush(UART_NUM_1);
        if (sendSTK500v2Message(b, 1))
        {
            // Wait for our response
            char resp[11];
            uint16_t size = 11;
            if (getSTK500v2Response(resp, &size))
            {
                // Got response
                logI(TAG_AVR_PRO, "got sync on attempt %d", tries);
                return 1;
            }
        }
    }
    return 0;
}

int stk500v2EnterProgrammingMode(void)
{
    char enterProgmode[12];
    enterProgmode[0] = 0x10;
    enterProgmode[1] = 0xc8;
    enterProgmode[2] = 0x64;
    enterProgmode[3] = 0x19;
    enterProgmode[4] = 0x20;
    enterProgmode[5] = 0x00;
    enterProgmode[6] = 0x53;
    enterProgmode[7] = 0x03;
    enterProgmode[8] = 0xec;
    enterProgmode[9] = 0x53;
    enterProgmode[10] = 0x00;
    enterProgmode[11] = 0x00;
    if (sendSTK500v2Message(enterProgmode, 12))
    {
        // Wait for our response
        char resp2[2];
        uint16_t size2 = 2;
        if (getSTK500v2Response(resp2, &size2))
        {
            // Got response
            if ((resp2[0] == 0x10) && (resp2[1] == 0x00))
            {
                logI(TAG_AVR_PRO, "%s", "Entered programming mode");
                return 1;
            }
        }
    }
    return 0;
}

int stk500v2LeaveProgrammingMode(void)
{
    // Leave programming mode
    char leaveProgmode[3];
    leaveProgmode[0] = 0x11;
    leaveProgmode[1] = 0x01;
    leaveProgmode[2] = 0x01;
    if (sendSTK500v2Message(leaveProgmode, 3))
    {
        // Wait for our response
        char resp[2];
        uint16_t size = 2;
        if (getSTK500v2Response(resp, &size))
        {
            // Got response
            logI(TAG_AVR_PRO, "%s", "Left programming mode");
            return 1;
        }
    }
    return 0;
}

int stk500v2LoadAddress(uint32_t addr)
{
    char loadAddr[5];
    loadAddr[0] = 0x06;
    loadAddr[1] = addr >> 24;
    loadAddr[2] = addr >> 16;
    loadAddr[3] = addr >> 8;
    loadAddr[4] = addr;
    if (sendSTK500v2Message(loadAddr, 5))
    {
        // Wait for our response
        char resp[2];
        uint16_t size = 2;
        if (getSTK500v2Response(resp, &size))
        {
            // Got response
            logI(TAG_AVR_PRO, "%s", "Address loaded");
            return 1;
        }
    }
    return 0;
}

int setProgParams(void)
{
    char params[] = {0x86, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x03, 0xff, 0xff, 0xff, 0xff, 0x00, 0x80, 0x04, 0x00, 0x00, 0x00, 0x80, 0x00};
    logI(TAG_AVR_PRO, "%s", "Setting Prog Parameters");

    return execParam(0x42, params, sizeof(params));
}

int setExtProgParams(void)
{
    char params[] = {0x05, 0x04, 0xd7, 0xc2, 0x00};
    logI(TAG_AVR_PRO, "%s", "Setting ExProg Parameters");
    return execParam(0x45, params, sizeof(params));
}

int enterProgMode(void)
{
    logI(TAG_AVR_PRO, "%s", "Entering Pro Mode");
    return execCmd(0x50);
}

int extProgMode(void)
{
    logI(TAG_AVR_PRO, "%s", "Exiting Pro Mode");
    return execCmd(0x51);
}

int execCmd(char cmd)
{
    char bytes[] = {cmd, 0x20};
    return sendBytes(bytes, 2);
}

static uint8_t gMsgSequenceNumber = 0;
const int kSTK500v2MessageHeaderSize = 5;

int sendSTK500v2MessageWithData(char* msg, uint16_t msg_count, char* data, uint16_t data_count)
{
    char header[kSTK500v2MessageHeaderSize];
    header[0] = 0x1b;
    header[1] = gMsgSequenceNumber++;
    header[4] = 0x0e;
    // Fill in the size in the header
    uint16_t total_count = msg_count + data_count;
    header[2] = total_count >> 8;
    header[3] = total_count & 0xff;
    // Work out the checksum, XORing all the bytes inc. header
    char checksum = 0;
    for (int i = 0; i < kSTK500v2MessageHeaderSize; i++)
    {
        checksum ^= header[i];
    }
    for (int i = 0; i < msg_count; i++)
    {
        checksum ^= msg[i];
    }
    if (data)
    {
        for (int i = 0; i < data_count; i++)
        {
            checksum ^= data[i];
        }
    }
    // Now send the message
    int ret = sendData(TAG_AVR_PRO, header, kSTK500v2MessageHeaderSize);
    if (ret == kSTK500v2MessageHeaderSize)
    {
        ret = sendData(TAG_AVR_PRO, msg, msg_count);
        if (ret == msg_count)
        {
            if (data)
            {
                ret = sendData(TAG_AVR_PRO, data, data_count);
            }
            else
            {
                ret = data_count; // So the next if statement will be true
            }
            if (ret == data_count)
            {
                return sendData(TAG_AVR_PRO, &checksum, 1);
            }
        }
    }
    // Something went wrong, return an error
    return 0;
}

int sendSTK500v2Message(char* msg, uint16_t count)
{
    return sendSTK500v2MessageWithData(msg, count, NULL, 0);
}

// Get a response to a STK500v2 message
// param respBuffer - buffer to receive the response body
// param bufferSize - maximum number of bytes to read into respBuffer.  Upon return
//                    contains the number of bytes in the response
int getSTK500v2Response(char* respBuffer, uint16_t* bufferSize)
{
    int length = waitForSerialData(kSTK500v2MessageHeaderSize+1+*bufferSize, MAX_DELAY_MS);

    if (length > kSTK500v2MessageHeaderSize+1)
    {
        // Read in the header
        char header[kSTK500v2MessageHeaderSize];
        int rxBytes = uart_read_bytes(UART_NUM_1, header, kSTK500v2MessageHeaderSize, 1000 / portTICK_RATE_MS);
        // Check the constant values in the header
        if ((rxBytes == kSTK500v2MessageHeaderSize) && (header[0] == 0x1b) && (header[4] == 0x0e))
        {
            // Leading byte is correct, check the sequence number
            // It'll be one less than gMsgSequenceNumber because that will have been
            // incremented ready for the next message
            // Need to cast it so we handle wrapping properly
            if ((uint8_t)(header[1]+1) == gMsgSequenceNumber)
            {
                // It's a response to the right message, get the size
                uint16_t size = header[2] << 8 | header[3];
                if (size <= *bufferSize)
                {
                    // Read that many bytes in
                    *bufferSize = uart_read_bytes(UART_NUM_1, respBuffer, size, 1000 / portTICK_RATE_MS);
                    // Now the checksum
                    uint8_t msgChecksum;
                    uart_read_bytes(UART_NUM_1, &msgChecksum, 1, 1000 / portTICK_RATE_MS);
                    uint8_t calculatedChecksum = 0x00;
                    for (int i = 0; i < kSTK500v2MessageHeaderSize; i++)
                    {
                        calculatedChecksum ^= header[i];
                    }
                    for (int i = 0; i < *bufferSize; i++)
                    {
                        calculatedChecksum ^= respBuffer[i];
                    }
                    if (calculatedChecksum == msgChecksum)
                    {
                        // All is good!
                        return 1;
                    }
                    else
                    {
                        logE(TAG_AVR_PRO, "Message checksum failed.  Expected 0x%02X, got 0x%02X", calculatedChecksum, msgChecksum);
                    }
                }
                else
                {
                    logE(TAG_AVR_PRO, "Message too large.  Expected max of %d, got %d", *bufferSize, size);
                }
            }
            else
            {
                logE(TAG_AVR_PRO, "Incorrect sequence number.  Expected %d, got %d", (uint8_t)(gMsgSequenceNumber-1), header[1]);
            }
        }
        else
        {
            logE(TAG_AVR_PRO, "Incorrect message header.  Expected 0x1B and 0x0E, got 0x%02X and 0x%02X", header[0], header[4]);
        }
    }
    // else there's not enough room for header plus checksum
    return 0;
}

int execParam(char cmd, char *params, int count)
{
    char bytes[32];
    bytes[0] = cmd;

    int i = 0;
    while (i < count)
    {
        bytes[i + 1] = params[i];
        i++;
    }

    bytes[i + 1] = 0x20;
    return sendBytes(bytes, i + 2);
}

int sendBytes(char *bytes, int count)
{
    sendData(TAG_AVR_PRO, bytes, count);
    int length = waitForSerialData(MIN_DELAY_MS, MAX_DELAY_MS);

    if (length > 0)
    {
        uint8_t data[length];
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, length, 1000 / portTICK_RATE_MS);
        if (rxBytes > 0)
        {
            if (data[0] == SYNC && data[1] == OK)
            {
                logI(TAG_AVR_PRO, "%s", "Sync Success");
                return 1;
            }
            else
            {
                logE(TAG_AVR_PRO, "%s", "Sync Failure");
                return 0;
            }
        }
    }
    else
    {
        logE(TAG_AVR_PRO, "%s", "Serial Timeout");
    }
    return 0;
}

int waitForSerialData(int dataCount, int timeout)
{
    int timer = 0;
    int length = 0;
    while (timer < timeout)
    {
        uart_get_buffered_data_len(UART_NUM_1, (size_t *)&length);
        if (length >= dataCount)
        {
            return length;
        }
        vTaskDelay(250 / portTICK_PERIOD_MS);
        timer+=250;
    }
    return 0;
}

int sendData(const char *logName, const char *data, const int count)
{
    const int txBytes = uart_write_bytes(UART_NUM_1, data, count);
    //ESP_LOG_BUFFER_HEXDUMP(logName, data, count, ESP_LOG_DEBUG);
    return txBytes;
}

