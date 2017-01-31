#include <stdio.h>
#include <stdlib.h>
//#include <time.h>
#include <bcm2835.h>
#include "decoder.h"
//#include "functions.c"

#define PIN1 RPI_GPIO_P1_07
#define PIN2 RPI_GPIO_P1_08
#define PINSS RPI_GPIO_P1_26
void setupRegisters();
void sendMessage(char*);
void readFifo(char *);

char buf[0x72], returnBuf[0x72];
#define TRUE				1
#define FALSE				0
#define STANDBY 			0x04
#define TRANSMITER 			0x0C
#define RECEIVER 			0x10
#define ADDR_IRQFLAGS1		0x27
#define ADDR_IRQFLAGS2		0x28
#define ADDR_MODE			0x01
#define ADDR_DATAMODUL		0x02
#define ADDR_FIFOTHRESH		0x3C
#define ADDR_PAYLOADLEN		0x38
#define MASK_MODEREADY		0x80
#define MASK_FIFONOTEMPTY	0x40
#define MASK_FIFOOVERRUN	0x10
#define MASK_PACKETSENT		0x08
#define MASK_TXREADY		0x20
#define MASK_PACKETMODE		0x60
#define MASK_MODULATION		0x18
char* fifoFlush(){
	buf[0] = 0x28;												// FIFO flags address
	bcm2835_spi_transfern(buf, 2);
	if (buf[1] & 0x40){
		uint8_t i = 0;
		while (buf[1] & 0x40){									// while FIFO not empty
			buf[0] = 0x00;										// FIFO data addr
			bcm2835_spi_transfern(buf, 2);						// read FIFO
			returnBuf[i++] = buf[1];
			buf[0] = 0x28;										// FIFO flags address
			bcm2835_spi_transfern(buf, 2);
		}
	}
	return returnBuf;
}

void regReadBuf(char *retBuf, uint8_t addr, uint8_t size){
	buf[0] = addr;
	bcm2835_spi_transfernb(buf, retBuf, size);
}

uint8_t regRead(uint8_t addr){
	buf[0] = addr;
	bcm2835_spi_transfern(buf, 2);
	return buf[1];
}

void regWrite(uint8_t addr, uint8_t val){
	buf[0] = addr | 0x80;
	buf[1] = val;
	bcm2835_spi_transfern(buf, 2);
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
		if (cnt % 30000 == 0){
			if (cnt > 200000)
			{
				printf("timeout inside a while for addr %02x\n", addr);
				break;
			}
			else
				printf("wait long inside a while for addr %02x\n", addr);
		}
		ret = regRead(addr);
	} while ((ret & mask) != (val ? mask : 0));
	return;
}

void decryptMsg(char *buf, uint8_t size){
	uint8_t i;
	seed(0xEF, (uint16_t)(buf[2]<<8)|buf[3]);
	for (i = 4; i < size; ++i)
		buf[i] = decrypt(buf[i]);	
}

void encryptMsg(char *buf, uint8_t size){
	uint8_t i;
	seed(0xEF, (uint16_t)(buf[2]<<8)|buf[3]);
	for (i = 4; i < size; ++i)
		buf[i] = decrypt(buf[i]);	
}

void setupCrc(char *buf){
	uint16_t val, size = buf[1];
	val = crc((uint8_t*)buf + 4, size - 6);
	buf[size - 2] = (char)(val >> 8);
	buf[size - 1] = (char)(val & 0x00FF);
}

// receive and display data
int main(int argc, char **argv){
	if (!bcm2835_init())
		return 1;
	int8_t buffSize = 0x72, i;
	char *tbuf, *rbuf;
	tbuf = (char*) malloc (buffSize);
	rbuf = (char*) malloc (buffSize);

	bcm2835_gpio_fsel(PINSS, BCM2835_GPIO_FSEL_OUTP);
	
	bcm2835_spi_begin();	
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32); 	// 64 = 256ns = 3.90625MHz
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); 				// CPOL = 0, CPHA = 0
	bcm2835_spi_chipSelect(BCM2835_SPI_CS1);					// chip select 1

	setupRegisters();
	delay(50);											// wait until ready after mode switching
	
	// LED RELATED
	bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(PIN1, LOW);
	bcm2835_gpio_write(PIN2, LOW);

	while(1){

		tbuf[0] = 0x28;											// FIFO flags address
		bcm2835_spi_transfernb(tbuf, rbuf, 2);
		if (rbuf[1] & 0x04){									// payload ready
			readFifo(rbuf);
			uint8_t size = rbuf[1];
			printf("packet received\n");
			decryptMsg(rbuf, size);
			rbuf[1] += 3;
			size = rbuf[1];
			rbuf[size -6] = 0x73;			// Switch state
			rbuf[size -5] = 0;
			rbuf[size -4] = 1;
			rbuf[size -3] = 0;				// CRC type
			rbuf[6] = 0x77;					// Sensor ID
			rbuf[0] = 0x12;					// Product ID
			setupCrc(rbuf);
			encryptMsg(rbuf, size);
			sendMessage(rbuf);	
			
			decryptMsg(rbuf, size);	
			printf("\n");
			for (i=0; i < 29 ; ++i)
				printf("%02x, ",rbuf[i]);
		}
		
	}
	bcm2835_spi_end();
	return 0;
}

typedef struct regSet {
	char addr;
	char val;
} regSet;

 regSet regSetup[] = {
	{0x05, 0x01}, {0x06, 0xEC}, 				// frequency deviation 5kHz 0x0052 -> 30kHz 0x01EC
	{0x07, 0x6C}, {0x08, 0x93}, {0x09, 0x33},	// carrier freq 9xxMHz 0xE4C000 -> 434.3MHz 0x6C9333
	//{0x07, 0x6C}, {0x08, 0x80}, {0x09, 0x00},	// carrier freq 9xxMHz 0xE4C000 -> 433MHz 0x6C8000
	{0x0B, 0x20},								// standard AFC routine -> improved AFC routine
	{0x18, 0x00},								// 200ohms, gain by AGC loop -> 50ohms
	//{0x18, 0x0E},								// 200ohms, gain by AGC loop -> 50ohms, H gain - 48dB
	{0x19, 0x43},								// channel filter bandwidth 10kHz -> 60kHz  page:26
	//{0x1E, 0x2c},								// AFC is performed each time rx mode is entered
	//{0x29, 220},								// RSSI threshold 0xE4 -> 0x??
	{0x2C, 0x00}, {0x2D, 0x05},					// preamble size MSB, LSB 3 -> 5
	{0x2E, 0x88},								// sych config x80 | x18 => 
	{0x2F, 0x2D},								// synch value 1
	{0x30, 0xD4},								// synch value 2
	//{0x37, 0x90},								// CRC on + packet fixed length -> CRC on + packet variable length
	{0x37, 0x22},								// CRC on -> CRC off, manchester
	{0x38, 26},									// Payload length
	{0x39, 0x01},								// Node address
	{0x3C, 25},									// FIFO threshold
	//{0x3D, 0x10},								// packet config packet RX delay = 0 and autorestart ON x02 => delay = x?2 and ON
	//{0x6F, 0x20},
	{0x01, 0x10}								// RX mode
}; 

void setupRegisters(){
	uint8_t size = sizeof(regSetup)/sizeof(regSet), i;
	char tbuf[2];
	for (i=0; i<size; ++i){
		//printf(" .");
		tbuf[0] = regSetup[i].addr | 0x80;
		tbuf[1] = regSetup[i].val;
		bcm2835_spi_writenb(tbuf, 2);
	}
}

void readFifo(char *rbuf){
	char addr[2], val[2];
	addr[0] = 0x28;											// FIFO flags address
	bcm2835_spi_transfernb(addr, val, 2);
	if (val[1] & 0x40){
		uint8_t i = 0;
		while (val[1] & 0x40){									// while FIFO not empty
			addr[0] = 0x00;										// FIFO data addr
			bcm2835_spi_transfernb(addr, val, 2);				// read FIFO
			rbuf[i++] = val[1];
			addr[0] = 0x28;										// FIFO flags address
			bcm2835_spi_transfernb(addr, val, 2);
		}
	}
}

void sendMessage(char *sendBuf){
	char tbuf[0x72], rbuf[0x72], rbuf2[0x72];
	static uint8_t ledState = 0;
	uint8_t i, size;

	// LED RELATED
	if (ledState == 1){
		ledState = 0;
		bcm2835_gpio_write(PIN1, LOW);
	} else {
		ledState = 1;
		bcm2835_gpio_write(PIN1, HIGH);
	}

	tbuf[0] = 0x92;
	tbuf[1] = 0x0f;
	bcm2835_spi_writenb(tbuf, 2);
	
	fifoFlush();
	assertRegVal(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY, FALSE, "FIFO flushed");
	assertRegVal(ADDR_MODE, RECEIVER, TRUE, "FIFO flushed");
	
	// PRINTING REGISTERS
	regReadBuf(rbuf, 0x01, 0x61);					// read registers 1
	for (i = 1; i < 0x61; i++)
	{
		printf("[%02x]=%02x%c", i, (int)rbuf[i], i%8==0?'\n':'\t');		
	}
	

	if (FALSE)
	{
		delay(500);

		regReadBuf(rbuf2, 0x01, 0x61);					// read registers 2
		for (i = 1; i < 0x61; i++)
			printf("[%02x]=%02x%c", i, (int)rbuf2[i], i%8==0?'\n':'\t');
		printf("\n");
		
		for (i = 1; i < 0x61; i++)
			if (rbuf[i] != rbuf2[i])					// compare registers 1 and 2
				printf("different value detected!! addr %02x\n", i); 
	}
	// END PRINTING REGISTERS	
	
	changeMode(STANDBY);									// Switch to standby mode
	waitFor (ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);			// wait for ModeReady
	
	assertRegVal(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY, FALSE, "FIFO flushed, than mode changed to standby");
	assertRegVal(ADDR_MODE, STANDBY, TRUE, "FIFO flushed, than mode changed to standby");
	
	size = sendBuf[1];
	regWrite(ADDR_FIFOTHRESH, size - 1);
	regWrite(ADDR_PAYLOADLEN, size);
	
	tbuf[0] = 0x80;
	for (i = 1; i <= size; i++)								// Fill FIFO
		tbuf[i] = sendBuf[i-1];
	bcm2835_spi_writenb(tbuf, size + 1);

	assertRegVal(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY, TRUE, "data pushed into FIFO");
	assertRegVal(ADDR_IRQFLAGS2, MASK_FIFOOVERRUN, FALSE, "data pushed into FIFO");
	
	changeMode(TRANSMITER);									// Switch to TX mode

	assertRegVal(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY, TRUE, "data pushed into FIFO and mode changed to TX");
	assertRegVal(ADDR_IRQFLAGS1, MASK_TXREADY, FALSE, "data pushed into FIFO and mode changed to TX");
	assertRegVal(ADDR_DATAMODUL, (MASK_PACKETMODE | MASK_MODULATION), FALSE, "data pushed into FIFO and mode changed to TX");
	
	waitFor (ADDR_IRQFLAGS1, MASK_MODEREADY | MASK_TXREADY, TRUE);		// wait for ModeReady + TX ready

	assertRegVal(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY, TRUE, "TX mode just ready");
	assertRegVal(ADDR_IRQFLAGS1, MASK_TXREADY, TRUE, "TX mode just ready");
	assertRegVal(ADDR_MODE, TRANSMITER, TRUE, "TX mode just ready");

	waitFor (ADDR_IRQFLAGS2, MASK_PACKETSENT, TRUE);					// wait for Packet sent

	assertRegVal(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY | MASK_FIFOOVERRUN, FALSE, "are all bytes sent?");

	changeMode(STANDBY);												// Switch to standby mode
	waitFor (ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);						// wait for ModeReady
	assertRegVal(ADDR_MODE, STANDBY, TRUE, "near the end");

	changeMode(RECEIVER);												// Switch to RX mode
	waitFor (ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);						// wait for ModeReady
	assertRegVal(ADDR_MODE, RECEIVER, TRUE, "near he end");
}
