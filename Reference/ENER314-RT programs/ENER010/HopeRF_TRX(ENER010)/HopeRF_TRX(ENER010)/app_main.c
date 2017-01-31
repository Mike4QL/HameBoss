#include "app_main.h"
#include "dev_HRF.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <bcm2835.h>
#include <stdbool.h>
uint8_t relayState;
// receive in variable length packet mode, display and send to R1 and Legacy socket.
int main(int argc, char **argv){
	if (!bcm2835_init())
		return 1;
		
	time_t currentTime, legacyTime, monitorControlTime;
	
	// LED INIT
	bcm2835_gpio_fsel(LEDG, BCM2835_GPIO_FSEL_OUTP);			// LED green
	bcm2835_gpio_fsel(LEDR, BCM2835_GPIO_FSEL_OUTP);			// LED red
	bcm2835_gpio_write(LEDG, LOW);
	bcm2835_gpio_write(LEDR, LOW);
	// SPI INIT
	bcm2835_spi_begin();	
	bcm2835_spi_setClockDivider(SPI_CLOCK_DIVIDER_26); 			// 250MHz / 26 = 9.6MHz
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); 				// CPOL = 0, CPHA = 0
	bcm2835_spi_chipSelect(BCM2835_SPI_CS1);					// chip select 1

	HRF_config_FSK();
	HRF_wait_for(ADDR_IRQFLAGS1, MASK_MODEREADY, true);			// wait until ready after mode switching
	HRF_clr_fifo();

	legacyTime = time(NULL);
	monitorControlTime = time(NULL);
	while (1){
		currentTime = time(NULL);
		
		HRF_receive_FSK_msg();
		
		if (difftime(currentTime, legacyTime) >= 5)			// Number of seconds between Legacy message send
		{
			
			legacyTime = time(NULL);
			printf("send LEGACY message:\t");
			static bool switchState = false;
			switchState = !switchState;
			bcm2835_gpio_write(LEDR, switchState);
			if (relayState == 9){
				relayState = 0;}
				else {
					++relayState;}
			HRF_send_OOK_msg(relayState);
			
				
			
		}
		
		if (difftime(currentTime, monitorControlTime) >= 10)	// Number of seconds between R1 message send
		{
			monitorControlTime = time(NULL);
			static bool switchState = false;
			switchState = !switchState;
			printf("send MONITOR + CONTROL message:\trelay %s\n", switchState ? "ON" : "OFF");
			bcm2835_gpio_write(LEDG, switchState);
			HRF_send_FSK_msg(HRF_make_FSK_msg(0x01, 3, PARAM_SW_STATE | 0x80, 1, switchState));
		}
	}
	bcm2835_spi_end();
	return 0;
}




