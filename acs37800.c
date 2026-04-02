/*
 * acs37800.c
 *
 * Created: 08.08.2023
 *  Author: yeold
 *
 * Modified for SPI interface.
 *
 * SPI frame format (ACS37800 datasheet, section "Digital Communication"):
 *
 *   MOSI byte[0]  : bits[7:1] = 7-bit register address, bit[0] = R/W (1=Read, 0=Write)
 *   MOSI byte[4:1]: 32-bit payload, LSByte first (write data, or 0x00000000 for reads)
 *
 *   MISO byte[4:1]: 32-bit response to the PREVIOUS command.
 *
 * Because MISO carries the previous command's result, a register READ requires
 * two full 5-byte SPI transactions:
 *   1. Send the read command -> MISO contains stale/don't-care data (discarded).
 *   2. Send a dummy write   -> MISO now contains the register data we want.
 *
 * CS must be de-asserted (HIGH) between transactions.
 *
 * HAL assumption:
 *   - HAL_SPI_TransmitReceive() performs a full-duplex transfer.
 *   - CS is managed manually via GPIO (active LOW).
 *   - The SPI peripheral is configured for CPOL=0, CPHA=1 (Mode 1) or
 *     CPOL=1, CPHA=1 (Mode 3) — check your board schematic; the ACS37800
 *     latches MOSI on the rising edge of SCLK.
 */

#include "acs37800.h"
#include <stdbool.h>
#include <string.h>

#include "stm32U5xx_hal.h"

/* --------------------------------------------------------------------------
 * Private helpers
 * -------------------------------------------------------------------------- */

/**
 * @brief  Assert (pull LOW) the chip-select line.
 */
static inline void cs_assert(acs37800_t *device)
{
    HAL_GPIO_WritePin((GPIO_TypeDef *)device->cs_port,
                      device->cs_pin,
                      GPIO_PIN_RESET);
}

/**
 * @brief  De-assert (pull HIGH) the chip-select line.
 */
static inline void cs_deassert(acs37800_t *device)
{
    HAL_GPIO_WritePin((GPIO_TypeDef *)device->cs_port,
                      device->cs_pin,
                      GPIO_PIN_SET);
}

/**
 * @brief  Build a 5-byte SPI frame and transfer it.
 *
 * @param  device   Pointer to device handle.
 * @param  address  7-bit register address.
 * @param  rw       ACS37800_SPI_READ_FLAG (1) or ACS37800_SPI_WRITE_FLAG (0).
 * @param  txData   32-bit payload to send (write value, or 0 for reads).
 * @param  rxBuf    5-byte buffer that receives the MISO bytes.
 */
static void spi_transfer(acs37800_t *device,
                         uint8_t     address,
                         uint8_t     rw,
                         uint32_t    txData,
                         uint8_t    *rxBuf)
{
    uint8_t txBuf[ACS37800_SPI_FRAME_SIZE];

    /* Byte 0: [7:1] = address, [0] = R/W bit */
    txBuf[0] = (uint8_t)((address & 0x7F) | ((rw & 0x01) << 7));

    /* Bytes 1-4: 32-bit payload, little-endian (LSByte first) */
    txBuf[1] = (uint8_t)( txData        & 0xFF);
    txBuf[2] = (uint8_t)((txData >>  8) & 0xFF);
    txBuf[3] = (uint8_t)((txData >> 16) & 0xFF);
    txBuf[4] = (uint8_t)((txData >> 24) & 0xFF);

    cs_assert(device);
    HAL_Delay(5);
    HAL_SPI_TransmitReceive((SPI_HandleTypeDef *)device->spi_device,
                            txBuf, rxBuf,
                            ACS37800_SPI_FRAME_SIZE,
                            1000 /* timeout ms */);
    HAL_Delay(5);
    cs_deassert(device);
}

/**
 * @brief  Read a 32-bit register over SPI.
 *
 * Two transactions are required because the device returns the response to
 * the *previous* command on MISO (pipeline behaviour, see datasheet Fig. 30).
 *
 * @param  device   Pointer to device handle.
 * @param  address  Register address (7-bit).
 * @param  data     Output: received 32-bit register value.
 */
static void acs_readRegister(acs37800_t *device, uint16_t address, uint32_t *data)
{
    uint8_t rxBuf[ACS37800_SPI_FRAME_SIZE] = {0};

    /* Transaction 1: issue the read command; MISO carries old data — discard. */
    spi_transfer(device, (uint8_t)address, ACS37800_SPI_READ_FLAG, 0x00000000, rxBuf);

    /* Transaction 2: dummy write; MISO now carries the register data we requested. */
    spi_transfer(device, (uint8_t)address, ACS37800_SPI_READ_FLAG, 0x00000000, rxBuf);

    /* Reconstruct 32-bit value from bytes 1-4 (little-endian) */
    *data = (uint32_t) rxBuf[1]
          | (uint32_t)(rxBuf[2] <<  8)
          | (uint32_t)(rxBuf[3] << 16)
          | (uint32_t)(rxBuf[4] << 24);
}

/**
 * @brief  Write a 32-bit register over SPI.
 *
 * A single 5-byte transaction is sufficient for writes.
 *
 * @param  device   Pointer to device handle.
 * @param  address  Register address (7-bit).
 * @param  data     Pointer to the 32-bit value to write.
 */
static void acs_writeRegister(acs37800_t *device, uint16_t address, uint32_t *data)
{
    uint8_t rxBuf[ACS37800_SPI_FRAME_SIZE] = {0}; /* MISO discarded on writes */

    spi_transfer(device, (uint8_t)address, ACS37800_SPI_WRITE_FLAG, *data, rxBuf);
}

/* --------------------------------------------------------------------------
 * Public API  (unchanged behaviour; only the transport layer changed)
 * -------------------------------------------------------------------------- */

void acs_getRMS(acs37800_t *device, float * const pCurrent, float * const pVoltage)
{
    ACS37800_REGISTER_20_t store;
    acs_readRegister(device, ACS37800_R_IRMS_VRMS, &store.data.all);

    float volts = (float)store.data.bits.vrms;
    volts /= 55000.0f;
    volts *= 250.0f;
    volts /= 1000.0f;
    float resMult = (device->divRes + device->senseRes) / (float)device->senseRes;
    volts *= resMult;

    *pVoltage = volts;

    float amps = (float)store.data.bits.irms;
    amps /= 55000.0f;
    amps *= device->maxCurrent;

    *pCurrent = amps;
}

void acs_getInstCurrVolt(acs37800_t *device, float * const pCurrent, float * const pVoltage)
{
    ACS37800_REGISTER_2A_t store;
    acs_readRegister(device, ACS37800_R_ICODES_VCODES, &store.data.all);

    float volts = (float)store.data.bits.vcodes;
    volts /= 27500.0f;
    volts *= 250.0f;
    volts /= 1000.0f;
    float resMult = (device->divRes + device->senseRes) / (float)device->senseRes;
    volts *= resMult;

    *pVoltage = volts;

    float amps = (float)store.data.bits.icodes;
    amps /= 27500.0f;
    amps *= device->maxCurrent;

    *pCurrent = amps;
}

void acs_setBybassNenable(acs37800_t *device, _Bool bypass, _Bool eeprom)
{
    ACS37800_REGISTER_0F_t store;
    uint32_t key = ACS37800_R_ACCESS_CODE_KEY;

    acs_writeRegister(device, ACS37800_R_ACCESS_CODE, &key);
    acs_readRegister(device, ACS37800_R_I2C_CONFIG, &store.data.all);

    store.data.bits.bypass_n_en = bypass ? 1u : 0u;

    acs_writeRegister(device, ACS37800_R_I2C_CONFIG, &store.data.all);

    if (eeprom)
    {
        acs_readRegister(device,
                         (uint16_t)(ACS37800_R_I2C_CONFIG + ACS37800_EEPROM_OFFSET),
                         &store.data.all);

        store.data.bits.bypass_n_en = bypass ? 1u : 0u;

        acs_writeRegister(device,
                          (uint16_t)(ACS37800_R_I2C_CONFIG + ACS37800_EEPROM_OFFSET),
                          &store.data.all);
    }

    uint32_t zero = 0;
    acs_writeRegister(device, ACS37800_R_ACCESS_CODE, &zero);
    HAL_Delay(100);
}

void acs_setNumberOfSamples(acs37800_t *device, uint32_t numOfSamples, _Bool eeprom)
{
    ACS37800_REGISTER_0F_t store;
    uint32_t key = ACS37800_R_ACCESS_CODE_KEY;

    acs_writeRegister(device, ACS37800_R_ACCESS_CODE, &key);
    acs_readRegister(device, ACS37800_R_I2C_CONFIG, &store.data.all);

    store.data.bits.n = numOfSamples & 0x3FFu;

    acs_writeRegister(device, ACS37800_R_I2C_CONFIG, &store.data.all);

    if (eeprom)
    {
        acs_readRegister(device,
                         (uint16_t)(ACS37800_R_I2C_CONFIG + ACS37800_EEPROM_OFFSET),
                         &store.data.all);

        store.data.bits.n = numOfSamples & 0x3FFu;

        acs_writeRegister(device,
                          (uint16_t)(ACS37800_R_I2C_CONFIG + ACS37800_EEPROM_OFFSET),
                          &store.data.all);
    }

    uint32_t zero = 0;
    acs_writeRegister(device, ACS37800_R_ACCESS_CODE, &zero);
    HAL_Delay(100);
}
