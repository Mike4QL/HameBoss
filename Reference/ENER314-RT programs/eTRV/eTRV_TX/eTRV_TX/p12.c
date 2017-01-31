#include <stdio.h>
#include <bcm2835.h>

#define D0 RPI_V2_GPIO_P1_11
#define D1 RPI_V2_GPIO_P1_15
#define D2 RPI_V2_GPIO_P1_16
#define D3 RPI_V2_GPIO_P1_13
#define MODSEL RPI_V2_GPIO_P1_18
#define ENABLE RPI_V2_GPIO_P1_22

void send(uint8_t);

// Send OOK modulated data by using Energenie RF transmiter
int main(int argc, char **argv){
	if (!bcm2835_init())
		return 1;
	
	bcm2835_gpio_fsel(D0, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(D1, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(D2, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(D3, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(MODSEL, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(ENABLE, BCM2835_GPIO_FSEL_OUTP);

	bcm2835_gpio_write(ENABLE, LOW);
	bcm2835_gpio_write(MODSEL, LOW);
	static uint8_t dVal;
	while(1){
	
		dVal = 0x0b;		// All on
		send(dVal);
		delay(2000);
		dVal = 0x03;		// All off
		send(dVal);
		delay(2000);
		dVal = 0x0f;		// S1 on
		send(dVal);
		delay(2000);
		dVal = 0x07;		// S1 off
		send(dVal);
		delay(2000);		
	}
	bcm2835_spi_end();
	return 0;
}

void send(uint8_t dVal){
	bcm2835_gpio_write(D0, dVal & 0x01 ? HIGH : LOW);
	bcm2835_gpio_write(D1, dVal & 0x02 ? HIGH : LOW);
	bcm2835_gpio_write(D2, dVal & 0x04 ? HIGH : LOW);
	bcm2835_gpio_write(D3, dVal & 0x08 ? HIGH : LOW);
	
	delay(100);
	bcm2835_gpio_write(ENABLE, HIGH);						// enable modulator
	delay(250);
	bcm2835_gpio_write(ENABLE, LOW);						// disable modulator
}
