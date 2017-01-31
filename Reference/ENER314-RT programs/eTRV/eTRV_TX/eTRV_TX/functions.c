#include <stdint.h>
#include <bcm2835.h>
#include <stdio.h>
char buf[0x72], returnBuf[0x72];

char* fifoFlush(){
	//printf("\nfifo flsh,");
	buf[0] = 0x28;											// FIFO flags address
	bcm2835_spi_transfern(buf, 2);
	if (buf[1] & 0x40){
		uint8_t i = 0;
		while (buf[1] & 0x40){									// while FIFO not empty
			//printf("\n%02x,", buf[1]);
			buf[0] = 0x00;										// FIFO data addr
			bcm2835_spi_transfern(buf, 2);				// read FIFO
			returnBuf[i++] = buf[1];
			buf[0] = 0x28;										// FIFO flags address
			bcm2835_spi_transfern(buf, 2);
		}
	}
	return returnBuf;
}
