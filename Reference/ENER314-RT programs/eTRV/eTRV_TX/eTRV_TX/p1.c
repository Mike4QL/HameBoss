#include <stdio.h>
#include <bcm2835.h>
#define PIN1 RPI_GPIO_P1_07
#define PIN2 RPI_GPIO_P1_08
#define PINSS RPI_GPIO_P1_26

// writes and reads the data by using transfernb. Flash led
int main(int argc, char **argv){
	if (!bcm2835_init())
		return 1;
	
	bcm2835_gpio_fsel(PINSS, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_spi_begin();	
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_128); 	// 64 = 256ns = 3.90625MHz
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); 				// CPOL = 0, CPHA = 0
	bcm2835_spi_chipSelect(BCM2835_SPI_CS1);					// chip select 1
	//bcm2835_spi_chipSelect(BCM2835_SPI_CS_NONE);				// no CS control
	printf("flash\n");
	
	char retVal, sendAddr = 0x83, sendVal = 90, tbuf[10], rbuf[10];
	while(getchar() != 'q'){

		sendVal += 4;
		printf("sendAddr=%#x, sendVal1=%d, sendVal2=%d, ", sendAddr, sendVal - 2, sendVal);
		tbuf[0] = sendAddr;
		tbuf[1] = sendVal - 2;
		tbuf[2] = sendVal;
		bcm2835_gpio_write(PINSS, LOW);
		bcm2835_spi_transfernb(tbuf, rbuf, 3);
		bcm2835_gpio_write(PINSS, HIGH);
		printf("retval0=%d, retVal1=%d, retVal2=%d\n", rbuf[0], rbuf[1], rbuf[2]);
		
		bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_OUTP);
		bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
		bcm2835_gpio_write(PIN1, HIGH);
		bcm2835_gpio_write(PIN2, HIGH);
		delay(500);
		bcm2835_gpio_write(PIN1, LOW);
		delay(500);
		printf("flash\n");
	}
	bcm2835_spi_end();
	return 0;
}
