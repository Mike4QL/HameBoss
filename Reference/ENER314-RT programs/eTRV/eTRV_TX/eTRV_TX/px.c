#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <bcm2835.h>
#include "decoder.h"

#define PIN1 				RPI_GPIO_P1_07
#define PIN2 				RPI_GPIO_P1_08

static char buf[0x02];


typedef struct regSet_t {
	char addr;
	char val;
} regSet_t;

 void setupRegisters();
 uint8_t readFifoAll(uint8_t*);
 void regReadN(char* , uint8_t, uint8_t);
 void regWriteN(char*, uint8_t, uint8_t);
 uint8_t regRead(uint8_t);
 void regWrite(uint8_t, uint8_t);


int main(int argc, char **argv){
	if (!bcm2835_init())
		return 1;
	
	time_t currentTime;
	currentTime = time(NULL);
	struct tm *tmp;
	uint16_t payloadCnt = 0;
	char *rbuf;

	uint8_t buffSize = 0xFF;
	rbuf = (char*) malloc (buffSize);
	
	bcm2835_spi_begin();	
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32); 	// 32 = 512ns = 7.80625MHz
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); 				// CPOL = 0, CPHA = 0
	bcm2835_spi_chipSelect(BCM2835_SPI_CS1);					// chip select 1

	setupRegisters();
	delay(100);

	int i;

	//readFifoAll(rbuf);
	if(1){


		
		for (i = 1; i <= 7; ++i)		// address 20b
			rbuf[i] = i;
		for (i = 0; i<=7; ++i)
			printf("%02x, ", rbuf[i]);
	
		rbuf[0] = 0x80;
		bcm2835_spi_transfern(rbuf, 7);
		
		delay(100);
		rbuf[0] = 0;
		bcm2835_spi_transfern(rbuf, 7);
		for (i = 0; i<=7; ++i)
			printf("%02x, ", rbuf[i]);
		

		delay(1000);


	}
	free(rbuf);
	bcm2835_spi_end();
	return 0;
}

static regSet_t regSetup[] = {
	{0x05, 0x01}, {0x06, 0xEC}, 				// frequency deviation 5kHz 0x0052 -> 30kHz 0x01EC
	{0x07, 0x6C}, {0x08, 0x93}, {0x09, 0x33},	// carrier freq 9xxMHz 0xE4C000 -> 434.3MHz 0x6C9333
	{0x0B, 0x20},								// standard AFC routine -> improved AFC routine
	{0x18, 0x08},								// 200ohms, gain by AGC loop -> 50ohms
	//{0x18, 0x0E},								// 200ohms, gain by AGC loop -> 50ohms, H gain - 48dB
	{0x19, 0x43},								// channel filter bandwidth 10kHz -> 60kHz  page:26
	//{0x1E, 0x14},								// AFC is performed each time rx mode is entered
	{0x2C, 0x00}, {0x2D, 0x05},					// preamble size MSB, LSB 3 -> 5
	//{0x29, 220},								// RSSI threshold 0xE4 -> 0x??
	{0x2E, 0x88},								// sych config x80 | x18 => 
	{0x2F, 0x2D},								// synch value 1
	{0x30, 0xD4},								// synch value 2
	{0x37, 0xA0},								// CRC on -> CRC off, manchester
	{0x38, 0xFF},								// payload length
	//{0x39, 0x12},								// Node address
	//{0x3D, 0x12},								// packet config packet RX delay = 0 and autorestart ON x02 => delay = x?2 and ON
	{0x01, 0x04}								// RX mode
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
			printf("F%02x = %02x, \n", regRead(0x28), regRead(0));
			delay(100);
			rbuf[i++] = regRead(0x00);
		} while (regRead(0x28) & 0x40);
	}
	return i;
}
 void regReadN(char *retBuf, uint8_t addr, uint8_t size){
	retBuf[0] = addr;
	bcm2835_spi_transfern(retBuf, size);
}
 void regWriteN(char *retBuf, uint8_t addr, uint8_t size){
	retBuf[0] = addr | 0x80;
	bcm2835_spi_transfern(retBuf, size);
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
 
