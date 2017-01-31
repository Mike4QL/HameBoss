#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <bcm2835.h>
#define PIN1 RPI_GPIO_P1_07
#define PIN2 RPI_GPIO_P1_08
#define PINSS RPI_GPIO_P1_26
void initBuf(char*);
void printReg();
void setupRegisters();
void readFifoAndPrint(char*, char*);
// FIFO test
int main(int argc, char **argv){
	if (!bcm2835_init())
		return 1;
	
	char *tbuf, *rbuf;
	int i, buffSize = 0x72;
	tbuf = (char*) malloc (buffSize);
	rbuf = (char*) malloc (buffSize);
	
	bcm2835_gpio_fsel(PINSS, BCM2835_GPIO_FSEL_OUTP);
	while(1){
	bcm2835_spi_begin();	
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32); 	// 64 = 256ns = 3.90625MHz
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); 				// CPOL = 0, CPHA = 0
	bcm2835_spi_chipSelect(BCM2835_SPI_CS1);					// chip select 1
	
	setupRegisters();
	delay(500);											// wait until ready after mode switching
	//while(1){
		readFifoAndPrint(tbuf, rbuf);
		for (i=0; i<5; ++i){
			//printReg();
			;//delay(1000);
		}
	//}
	tbuf[0] = 0x01; tbuf[1] = 0x04; // standby mode
	bcm2835_spi_writenb(tbuf, 2);	// change mode
	bcm2835_spi_end();
	delay(50);
	}
	return 0;
}

typedef struct regDesc {
	char addr;
	char *desc;
} regDesc;

typedef struct regSet {
	char addr;
	char val;
} regSet;

regDesc regChk[]={{0x01, "opMode"}, {0x02, "dataMod"},  
	{0X07, "carrFrq2"}, {0x08, "carrFrq1"}, {0x09,"carrFrq0"},
	{0x0D,"Listen"}, {0x18, "regLna"}, {0x1E, "AfcFei"}, {0x1F, "AfcVal1"}, 
	{0x20, "AfcVal0"}, {0x21, "FeiVal1"}, {0x22, "FeiVal0"},
	{0x23, "RssiDone"}, {0x24, "RssiVal"}, {0x27, "RdyFlag"},
	{0x28, "FifoFlg"}, {0x29, "RssiTres"}, {0x37, "PktCfg"}, {0x38, "Payload"}, 
	{0x39, "NodeAddr"}};

regSet regSetup[] = {
	{0x01, 0x10},								// RX mode
	{0x05, 0x01}, {0x06, 0xEC}, 				// frequency deviation 5kHz 0x0052 -> 30kHz 0x01EC
	{0x07, 0x6C}, {0x08, 0x93}, {0x09, 0x33},	// carrier freq 9xxMHz 0xE4C000 -> 434.3MHz 0x6C9333
	{0x0B, 0x20},								// standard AFC routine -> improved AFC routine
	{0x18, 0x08},								// 200ohms, gain by AGC loop -> 50ohms
	{0x19, 0x43},								// channel filter bandwidth 10kHz -> 60kHz  page:26
	{0x1E, 0x14}								// AFC is performed each time rx mode is entered
	//{0x29, 0x99}								// RSSI threshold 0xE4 -> 0x99
	//{0x37, 0x90}								// CRC on + packet fixed length -> CRC on + packet variable length
	//{0x37, 0x00}								// CRC on -> CRC off
};

void printReg(){
	int size = sizeof(regChk)/sizeof(regDesc), i;
	char tbuf[2], rbuf[2];
	time_t currentTime;
	char* timeString;
	currentTime = time(NULL);
	timeString = ctime(&currentTime);
	printf("\n\n%s", timeString);
	for (i=0; i<size; ++i){
		tbuf[0] = regChk[i].addr;
		bcm2835_spi_transfernb(tbuf, rbuf, 2);
		printf("%s[%x]=%02x%c", regChk[i].desc, regChk[i].addr, rbuf[1], i%6==5?'\n':'\t');
	}
}

void setupRegisters(){
	int size = sizeof(regSetup)/sizeof(regSet), i;
	char tbuf[2];
	for (i=0; i<size; ++i){
		printf(" .");
		tbuf[0] = regSetup[i].addr | 0x80;
		tbuf[1] = regSetup[i].val;
		bcm2835_spi_writenb(tbuf, 2);
	}
}
	
void readFifoAndPrint(char *tbuf, char *rbuf){
	time_t currentTime;
	char *timeString;
	currentTime = time(NULL);
	timeString = ctime(&currentTime);
	printf("\n\t%s", timeString);
	tbuf[0] = 0x28;											// FIFO flags address
	bcm2835_spi_transfernb(tbuf, rbuf, 2);
	if (rbuf[1] & 0x40){
		int i = 0;

		while (rbuf[1] & 0x40){									// while FIFO not empty
			tbuf[0] = 0x00;										// FIFO data addr
			bcm2835_spi_transfernb(tbuf, rbuf, 2);				// read FIFO
			printf("FIFO[%x]=%02x%c", i, (int)rbuf[1], i%5==4?'\n':'\t');
			++i;
			tbuf[0] = 0x28;										// FIFO flags address
			bcm2835_spi_transfernb(tbuf, rbuf, 2);
		}
		printf("\n");
	}
}	
