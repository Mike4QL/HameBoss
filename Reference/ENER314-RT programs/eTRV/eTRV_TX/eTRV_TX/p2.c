#include <stdio.h>
#include <stdlib.h>
#include <bcm2835.h>
#define PIN1 RPI_GPIO_P1_07
#define PIN2 RPI_GPIO_P1_08
#define PINSS RPI_GPIO_P1_26

// FIFO test
int main(int argc, char **argv){
	if (!bcm2835_init())
		return 1;
	
	bcm2835_gpio_fsel(PINSS, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_spi_begin();	
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_128); 	// 64 = 256ns = 3.90625MHz
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); 				// CPOL = 0, CPHA = 0
	bcm2835_spi_chipSelect(BCM2835_SPI_CS1);					// chip select 1
	//bcm2835_spi_chipSelect(BCM2835_SPI_CS_NONE);				// no CS control
	
	
	char sendAddr = 0x80, *tbuf, *rbuf;
	int startVal, i, n;
	{
		printf("please enter the starting val and number or bytes: ");
		scanf("%d %d", &startVal, &n);
		tbuf = (char*) malloc ((n + 21) * sizeof(char));
		rbuf = (char*) malloc ((n + 21) * sizeof(char));
		for (i=0; i<n; ++i)
			tbuf[i+1] = (char)startVal + i;
			
		printf("\nsendAddr=%#x, sVal=%d, bytes=%d, step=1\n", sendAddr, startVal, n);
		tbuf[0] = sendAddr;
		bcm2835_spi_writenb(tbuf, n+1);
		
		tbuf[0] = 0;
		bcm2835_spi_transfernb(tbuf, rbuf, n+11);
		for (i=1; i<n+11; ++i)
			printf("val[%d]=%d%c", i, (int)rbuf[i], i%5==0?'\n':'\t');
		printf("\n");		

	}
	bcm2835_spi_end();
	return 0;
}
