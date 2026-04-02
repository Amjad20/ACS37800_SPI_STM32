/*
 * acs37800.h
 *
 *  Created on: 08.08.2023
 *      Author: yeold
 *
 *  Modified for SPI interface.
 *
 *  SPI frame format (per datasheet):
 *    - Bits [39:33] : 7-bit register address (MSB first)
 *    - Bit  [32]    : R/W indicator (1 = Read, 0 = Write)
 *    - Bits [31:0]  : 32-bit write data (or don't-care on reads)
 *  The device echoes back the response to the PREVIOUS command on MISO,
 *  so a read requires two SPI transactions: one to issue the read command,
 *  and a second (dummy) transaction to clock out the data.
 */

#ifndef INC_ACS37800_H_
#define INC_ACS37800_H_

#include <inttypes.h>
#include <stdbool.h>

/* Shadow Register Access of the EEPROM
 * Add the EEPROM offset to directly access it. */
#define ACS37800_EEPROM_OFFSET		-0x10
#define ACS37800_R_CURRENT_TRIM		0x1B
#define ACS37800_R_AVERAGE_SETTING	0x1C
#define ACS37800_R_DELAY_OVERCURR	0x1D
#define ACS37800_R_SAMPLING_SETTING	0x1E
#define ACS37800_R_I2C_CONFIG		0x1F	/* NOTE: register label kept for compatibility;
                                             this register holds N/BYPASS_N_EN in SPI mode too */

/* Other normal, volatile memory */
#define ACS37800_R_IRMS_VRMS		0x20	// 16 bits of IRMS / VRMS
#define ACS37800_R_PIMG_PACT		0x21
#define ACS37800_R_PFACTOR_PAPPAR	0x22
#define ACS37800_R_SAMPLES_USED		0x25
#define ACS37800_R_VIRMSAVGONESEC	0x26
#define ACS37800_R_VIRMSAVGONEMIN	0x27
#define ACS37800_R_PACTAVGONESEC	0x28
#define ACS37800_R_PACTAVGONEMIN	0x29
#define ACS37800_R_ICODES_VCODES	0x2A
#define ACS37800_R_PINSTANT			0x2C
#define ACS37800_R_STATUS			0x2D
#define ACS37800_R_ACCESS_CODE		0x2F
#define ACS37800_R_ACCESS_CODE_KEY	0x4F70656E
#define ACS37800_R_CUSTOMER_ACCESS	0x30

/* SPI frame helpers */
#define ACS37800_SPI_READ_FLAG		0x01	/* R/W bit set = Read */
#define ACS37800_SPI_WRITE_FLAG		0x00	/* R/W bit clear = Write */

/* Total SPI frame size: 1 byte (addr + R/W) + 4 bytes (data) = 5 bytes */
#define ACS37800_SPI_FRAME_SIZE		5

enum ACS_ERROR{
	ACS_NO_ERROR	= 0,
	ACS_IOERROR		= 1
};

enum ACS_STATUS{
	ACS_NO_EVENT		= 0x00,
	ACS_ZEROCROSSOUT	= 0x01,
	ACS_FAULTOUT		= 0x02,
	ACS_FAULTLATCHED	= 0x04,
	ACS_OVERVOLTAGE		= 0x08,
	ACS_UNDERVOLTAGE	= 0x10
};

enum ACS_PFACT_DETAILS{
	ACS_POSANGLE_LEADING	= 0x00,
	ACS_POSANGLE_LAGGING	= 0x01,
	ACS_POSFACTOR_GENERATED	= 0x00,
	ACS_POSFACTOR_CONSUMED	= 0x02
};

/*
 * Device handle.
 *
 * spi_device  : pointer to the HAL SPI handle (SPI_HandleTypeDef*)
 * cs_port     : GPIO port for the chip-select (CS) pin  (e.g. GPIOA)
 * cs_pin      : GPIO pin  for the chip-select           (e.g. GPIO_PIN_4)
 * maxVolt     : maximum line voltage (used for scaling)
 * maxCurrent  : rated current range of the device variant (e.g. 30 A)
 * senseRes    : R_SENSE value in ohms (milli-ohm resolution recommended)
 * divRes      : sum of isolation resistors (R_ISO) in ohms
 */
typedef struct {
	void*    spi_device;	/* SPI_HandleTypeDef* */
	void*    cs_port;		/* GPIO_TypeDef*      */
	uint16_t cs_pin;		/* GPIO pin mask       */
	uint8_t  maxVolt;
	uint8_t  maxCurrent;
	uint16_t senseRes;
	uint32_t divRes;
} acs37800_t;

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t qvo_fine : 9;
      uint32_t sns_fine : 10;
      uint32_t crs_sns : 3;
      uint32_t iavgselen : 1;
      uint32_t pavgselen : 1;
      uint32_t reserved : 2;
      uint32_t ECC : 6;
    } bits;
  } data;
} ACS37800_REGISTER_0B_t;

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t rms_avg_1 : 7;
      uint32_t rms_avg_2 : 10;
      uint32_t vchan_offset_code : 8;
      uint32_t reserved : 1;
      uint32_t ECC : 6;
    } bits;
  } data;
} ACS37800_REGISTER_0C_t;

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t reserved1 : 7;
      uint32_t ichan_del_en : 1;
      uint32_t reserved2 : 1;
      uint32_t chan_del_sel : 3;
      uint32_t reserved3 : 1;
      uint32_t fault : 8;
      uint32_t fltdly : 3;
      uint32_t reserved4 : 2;
      uint32_t ECC : 6;
    } bits;
  } data;
} ACS37800_REGISTER_0D_t;

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t vevent_cycs : 6;
      uint32_t reserved1 : 2;
      uint32_t overvreg : 6;
      uint32_t undervreg : 6;
      uint32_t delaycnt_sel : 1;
      uint32_t halfcycle_en : 1;
      uint32_t squarewave_en : 1;
      uint32_t zerocrosschansel : 1;
      uint32_t zerocrossedgesel : 1;
      uint32_t reserved2 : 1;
      uint32_t ECC : 6;
    } bits;
  } data;
} ACS37800_REGISTER_0E_t;

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t reserved1 : 2;
      uint32_t i2c_slv_addr : 7;		/* not used in SPI mode */
      uint32_t i2c_dis_slv_addr : 1;	/* not used in SPI mode */
      uint32_t dio_0_sel : 2;
      uint32_t dio_1_sel : 2;
      uint32_t n : 10;
      uint32_t bypass_n_en : 1;
      uint32_t reserved2 : 1;
      uint32_t ECC : 6;
    } bits;
  } data;
} ACS37800_REGISTER_0F_t;

//Volatile Registers : Bit Field definitions

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t vrms : 16;
      uint32_t irms : 16;
    } bits;
  } data;
} ACS37800_REGISTER_20_t;

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t pactive : 16;
      uint32_t pimag : 16;
    } bits;
  } data;
} ACS37800_REGISTER_21_t;

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t papparent : 16;
      uint32_t pfactor : 11;
      uint32_t posangle : 1;
      uint32_t pospf : 1;
    } bits;
  } data;
} ACS37800_REGISTER_22_t;

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t numptsout : 10;
    } bits;
  } data;
} ACS37800_REGISTER_25_t;

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t vrmsavgonesec : 16;
      uint32_t irmsavgonesec : 16;
    } bits;
  } data;
} ACS37800_REGISTER_26_t;

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t vrmsavgonemin : 16;
      uint32_t irmsavgonemin : 16;
    } bits;
  } data;
} ACS37800_REGISTER_27_t;

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t pactavgonesec : 16;
    } bits;
  } data;
} ACS37800_REGISTER_28_t;

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t pactavgonemin : 16;
    } bits;
  } data;
} ACS37800_REGISTER_29_t;

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t vcodes : 16;
      uint32_t icodes : 16;
    } bits;
  } data;
} ACS37800_REGISTER_2A_t;

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t pinstant : 16;
    } bits;
  } data;
} ACS37800_REGISTER_2C_t;

typedef struct
{
  union
  {
    uint32_t all;
    struct
    {
      uint32_t vzerocrossout : 1;
      uint32_t faultout : 1;
      uint32_t faultlatched : 1;
      uint32_t overvoltage : 1;
      uint32_t undervoltage : 1;
    } bits;
  } data;
} ACS37800_REGISTER_2D_t;

void acs_getRMS(acs37800_t* device, float * const pCurrent, float * const pVoltage);
void acs_getInstCurrVolt(acs37800_t* device, float * const pCurrent, float * const pVoltage);
void acs_setBybassNenable(acs37800_t* device, _Bool bypass, _Bool eeprom);
void acs_setNumberOfSamples(acs37800_t* device, uint32_t numOfSamples, _Bool eeprom);

#endif /* INC_ACS37800_H_ */
