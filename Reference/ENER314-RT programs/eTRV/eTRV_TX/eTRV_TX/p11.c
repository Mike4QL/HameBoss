#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <bcm2835.h>
#include "decoder.h"

#define PIN1 				RPI_V2_GPIO_P1_15
#define PIN2 				RPI_V2_GPIO_P1_13
#define TRUE				1
#define FALSE				0
#define STANDBY 			0x04
#define TRANSMITER 			0x0C
#define RECEIVER 			0x10
#define SIZE_PRODID			1
#define SIZE_MSGLEN			1
#define SIZE_ENCRYPTPIP		2
#define SIZE_SENSORID		3
#define SIZE_PARAMSET		1
#define SIZE_DATA_PARAMID	1
#define SIZE_DATA_TYPEDESC	1
#define SIZE_CRC			2
#define BYTES_TILL_MSG_LEN	2
#define ADDR_FIFO			0x00
#define ADDR_IRQFLAGS1		0x27
#define ADDR_IRQFLAGS2		0x28
#define ADDR_MODE			0x01
#define ADDR_DATAMODUL		0x02
#define ADDR_FIFOTHRESH		0x3C
#define ADDR_PAYLOADLEN		0x38
#define MASK_MODEREADY		0x80
#define MASK_FIFONOTEMPTY	0x40
#define MASK_FIFOLEVEL		0x20
#define MASK_FIFOOVERRUN	0x10
#define MASK_PACKETSENT		0x08
#define MASK_TXREADY		0x20
#define MASK_PACKETMODE		0x60
#define MASK_MODULATION		0x18
#define MASK_PAYLOADRDY		0x04
static char buf[0x02];

typedef struct regSet_t {
	char addr;
	char val;
} regSet_t;

 void setupRegisters();
 uint8_t readFifoAll(uint8_t*);
 void regReadN(uint8_t* , uint8_t, uint8_t);
 void regWriteN(uint8_t*, uint8_t, uint8_t);
 uint8_t regRead(uint8_t);
 void regWrite(uint8_t, uint8_t);
 void changeMode(uint8_t);
 void assertRegVal(uint8_t, uint8_t, uint8_t, char*);
 void waitFor (uint8_t, uint8_t, uint8_t);

// Send OOK modulated data by using HopeRF transmiter
int main(int argc, char **argv){
	if (!bcm2835_init())
		return 1;

	uint8_t *rbuf;
	uint8_t buffSize = 0xFF;
	rbuf = (uint8_t*) malloc (buffSize);
	
	bcm2835_spi_begin();	
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32); 	// 32 = 512ns = 7.80625MHz
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); 				// CPOL = 0, CPHA = 0
	bcm2835_spi_chipSelect(BCM2835_SPI_CS1);					// chip select 1

	setupRegisters();
	waitFor(ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);				// wait until ready after mode switching
	
	// LED RELATED
	bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(PIN1, HIGH);
	bcm2835_gpio_write(PIN2, LOW);
	
	int i;
	uint8_t ledState = 1;

	readFifoAll(rbuf);
	while(1){

		assertRegVal(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY, FALSE, "FIFO empty at the beginning");
		assertRegVal(ADDR_MODE, STANDBY, TRUE, "Mode should be standby");
		changeMode(TRANSMITER);									// Switch to TX mode
		waitFor (ADDR_IRQFLAGS1, MASK_MODEREADY | MASK_TXREADY, TRUE);		// wait for ModeReady + TX ready

		rbuf[1] = 0x80;				// Preambule 32b enclosed in sync words
		rbuf[2] = 0x00;
		rbuf[3] = 0x00;
		rbuf[4] = 0x00;
		for (i = 4; i < 14; ++i){
			rbuf[i + 1] = 8 + (i&1) * 6 + 128 + (i&2) * 48;				// address 20b * 4 = 10 Bytes
		}
		if (ledState == 0)
		{
			//rbuf[15] = 0xEE;		// D0-high, D1-h		// All on
			//rbuf[16] = 0x8E;		// D2-h, D3-h
			rbuf[15] = 0xEE;		// D0-high, D1-h		// S1 on
			rbuf[16] = 0xEE;		// D2-h, D3-h
		}
		else
		{
			//rbuf[15] = 0xEE;		// D0-high, D1-h		// All off
			//rbuf[16] = 0x88;		// D2-h, D3-h
			rbuf[15] = 0xEE;		// D0-high, D1-h		// S1 off
			rbuf[16] = 0xE8;		// D2-h, D3-h
		}
		
		regWriteN(rbuf + 4, 0, 13);		// Send four same messages
		
		for (i = 0; i < 8; ++i)
		{
			waitFor (ADDR_IRQFLAGS2, MASK_FIFOLEVEL, FALSE);
			regWriteN(rbuf, 0, 17);			// +4 sync bytes
		}


		waitFor (ADDR_IRQFLAGS2, MASK_PACKETSENT, TRUE);		// wait for Packet sent
		assertRegVal(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY | MASK_FIFOOVERRUN, FALSE, "are all bytes sent?");
		changeMode(STANDBY);									// Switch to standby mode
		waitFor (ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);			// wait for ModeReady
		
		if (ledState == 1){
			ledState = 0;
			printf("\nSet LOW");
			bcm2835_gpio_write(PIN1, LOW);
		} else {
			ledState = 1;
			printf("\nSet HIGH");
			bcm2835_gpio_write(PIN1, HIGH);
		}
		
		delay(1000);
	}
	bcm2835_spi_end();
	return 0;
}

static regSet_t regSetup[] = {
	{0x02, 0x08},			/* OOK */			// modulation scheme OOK
	//{0x05, 0x01}, {0x06, 0xEC}, 				// frequency deviation 5kHz 0x0052 -> 30kHz 0x01EC
	//{0x05, 0x03}, {0x06, 0xD8}, 				// frequency deviation 5kHz 0x0052 -> 60kHz 0x03D8
	{0x05, 0x00}, {0x06, 0x00}, 				// frequency deviation 5kHz 0x0052 -> 0kHz 0x0
	{0x07, 0x6C}, {0x08, 0x93}, {0x09, 0x33},	// carrier freq 9xxMHz 0xE4C000 -> 434.3MHz 0x6C9333
	{0x0B, 0x20},								// standard AFC routine -> improved AFC routine
	{0x18, 0x08},								// 200ohms, gain by AGC loop -> 50ohms
	//{0x18, 0x0E},								// 200ohms, gain by AGC loop -> 50ohms, H gain - 48dB
	{0x19, 0x41},			/* OOK */			// channel filter bandwidth 10kHz -> 120kHz  page:26 
	//{0x1E, 0x14},								// AFC is performed each time rx mode is entered
	{0x2C, 0x00}, {0x2D, 0x03},					// preamble size MSB, LSB 3 -> 5
	//{0x29, 220},								// RSSI threshold 0xE4 -> 0x??
	{0x2E, 0x98},								// sync config x80 | x18 => 
	{0x2F, 0x80},								// sync value 1
	{0x30, 0x00},								// sync value 2
	{0x31, 0x00},								// sync value 3
	{0x32, 0x00},								// sync value 4
	{0x37, 0x00},								// CRC on -> CRC off, manchester
	{0x38, 13 + 8 * 17},									// payload length
	//{0x39, 0x12},								// Node address
	{0x3C, 30},									// FIFO threshold
	//{0x3D, 0x12},								// packet config packet RX delay = 0 and autorestart ON x02 => delay = x?2 and ON
	{0x01, 0x04}								// RECEIVER mode
}; 
 void setupRegisters(){
	uint8_t size = sizeof(regSetup)/sizeof(regSet_t), i;
	for (i=0; i<size; ++i){
		regWrite(regSetup[i].addr, regSetup[i].val);
	}
}
 uint8_t readFifoAll(uint8_t *rbuf){
	uint8_t i = 0;
	if (regRead(0x28) & 0x40)				// FIFO FLAG FifoNotEmpty
	{
		do
		{									// while FIFO not empty
			rbuf[i++] = regRead(0x00);
		} while (regRead(0x28) & 0x40);
	}
	return i;
}
 void regReadN(uint8_t *retBuf, uint8_t addr, uint8_t size){
	retBuf[0] = addr;
	bcm2835_spi_transfern((char*)retBuf, size);
}
 void regWriteN(uint8_t *retBuf, uint8_t addr, uint8_t size){
	char temp = retBuf[0];
	retBuf[0] = addr | 0x80;
	bcm2835_spi_writenb((char*)retBuf, size);
	retBuf[0] = temp;
}
 uint8_t regRead(uint8_t addr){
	buf[0] = addr;
	bcm2835_spi_transfern(buf, 2);
	return buf[1];
}
 void regWrite(uint8_t addr, uint8_t val){
	buf[0] = addr | 0x80;
	buf[1] = val;
	bcm2835_spi_writenb(buf, 2);
	return;
}
 void changeMode(uint8_t mode){
	buf[0] = 0x81;
	buf[1] = mode;
	bcm2835_spi_writenb(buf, 2);
}
 void assertRegVal(uint8_t addr, uint8_t mask, uint8_t val, char *desc){
	buf[0] = addr;
	bcm2835_spi_transfern(buf, 2);
	if (val){
		if ((buf[1] & mask) != mask)
			printf("ASSERTION FAILED: addr:%02x, expVal:%02x(mask:%02x) != val:%02x, desc: %s\n", addr, val, mask, buf[1], desc);
	} else {
		if ((buf[1] & mask) != 0)
			printf("ASSERTION FAILED: addr:%02x, expVal:%02x(mask:%02x) != val:%02x, desc: %s\n", addr, val, mask, buf[1], desc);
	}
}
 void waitFor (uint8_t addr, uint8_t mask, uint8_t val){
	uint32_t cnt = 1; 
	uint8_t ret;
	do {
		++cnt;
		if (cnt % 800000 == 0){
			if (cnt > 4000000)
			{
				printf("timeout inside a while for addr %02x\n", addr);
				break;
			}
			else
			{
				//printf("wait long inside a while for addr %02x\n", addr);
			}
		}
		ret = regRead(addr);
	} while ((ret & mask) != (val ? mask : 0));
}

