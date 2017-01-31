#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <bcm2835.h>
#include "decoder.h"

#define PIN1 RPI_GPIO_P1_07
#define PIN2 RPI_GPIO_P1_08
#define PINSS RPI_GPIO_P1_26
void initBuf(char*);
void printReg();
void setupRegisters();
void readFifo(char*);
void printFifo(char*);
// FIFO test
int main(int argc, char **argv){
	if (!bcm2835_init())
		return 1;
	
	FILE *fpLog, *fpData;
	remove("/home/pi/Desktop/CProj/log.txt");
	remove("/home/pi/Desktop/CProj/data.txt");
	char *tbuf, *rbuf;
	int i, buffSize = 0x72;
	tbuf = (char*) malloc (buffSize);
	rbuf = (char*) malloc (buffSize);

	bcm2835_gpio_fsel(PINSS, BCM2835_GPIO_FSEL_OUTP);
	
	bcm2835_spi_begin();	
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32); 	// 64 = 256ns = 3.90625MHz
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); 				// CPOL = 0, CPHA = 0
	bcm2835_spi_chipSelect(BCM2835_SPI_CS1);					// chip select 1

	setupRegisters();
	delay(50);											// wait until ready after mode switching
	
	while(1){

		tbuf[0] = 0x28;											// FIFO flags address
		bcm2835_spi_transfernb(tbuf, rbuf, 2);
		if (rbuf[1] & 0x04){									// payload ready
		//if (rbuf[1] & 0x40){									// FIFO not empty
			static int payloadCnt = 1;
			//printReg();
			fpLog = fopen("/home/pi/Desktop/CProj/log.txt", "a");
			fpData = fopen("/home/pi/Desktop/CProj/data.txt", "a");
			if(fpLog && fpData == 0){
				printf("can't open file");
				break;
			}
			time_t currentTime;
			currentTime = time(NULL);
			struct tm *tmp = gmtime(&currentTime);
			fprintf(fpLog, "%02d:%02d:%02d %d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, payloadCnt);
			fprintf(fpData, "%02d:%02d:%02d %d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, payloadCnt++);
			
			readFifo(rbuf);
			printFifo(rbuf);

			if (rbuf[2] > rbuf[0])
				rbuf[2] = rbuf[0];
			if (rbuf[2] > 28)
				rbuf[2] = 28;
			rbuf[0] = rbuf[1];
			rbuf[1] = rbuf[2];
			for (i = 2; i < rbuf[1]; ++i)
				rbuf[i] = rbuf[i+1];

			if (0 == 0){
				printf("\nproduct_ID=%02x, msg_len=%d, PIP=%04x\n", rbuf[0], rbuf[1], (uint16_t)(rbuf[2]<<8)|rbuf[3]);
				fprintf(fpLog, " %02x %d %02x %02x", rbuf[0], rbuf[1], rbuf[2], rbuf[3]);
				int size = rbuf[1], dataVal = 0, dataLen = 0, type, crcFlag = 0;
				seed(0xEF, (uint16_t)(rbuf[2]<<8)|rbuf[3]);

				for (i = 4; i < size; ++i){
					rbuf[i] = decrypt(rbuf[i]);
					char val = rbuf[i];
					fprintf(fpLog, " %02x", val);
					printf("Msg[%d]=%02x%c", i-3, val, i%5==3?'\n':'\t');
					if (i < 8)
						continue;
					if (dataLen > 0){
						dataVal = dataVal * 256 + val;
						--dataLen;
						if (dataLen == 0){
							if (crcFlag == 0){
								fprintf(fpData, " %#x", dataVal);
							} else {
								if ((int16_t)dataVal == crc((uint8_t*)rbuf + 4, size - 6))
									fprintf(fpData, " PASS");
								else
									fprintf(fpData, " FAIL: expected:%04x result:%04x", dataVal, (int16_t)crc((uint8_t*)rbuf + 4, size - 6));
							}
							dataVal = 0;
						}
					} else if (dataLen == 0){
						dataLen = -1;
						switch (val){
							case 0x6A:
								fprintf(fpData, " Join");
								break;
							case 0x70:
								fprintf(fpData, " Power");
								break;
							case 0x71:
								fprintf(fpData, " Reactive_P");
								break;
							case 0x76:
								fprintf(fpData, " Voltage");
								break;
							case 0x69:
								fprintf(fpData, " Current");
								break;
							case 0x66:
								fprintf(fpData, " Frequency");
								break;
							case 0x73:
								fprintf(fpData, " Switch state");
								break;
							case 0x00:
								fprintf(fpData, " CRC");
								crcFlag = 1;
								dataLen = 2;
								break;
							default:
								fprintf(fpData, " Unknown");
						}
					} else {
						type = val >> 4;
						dataLen = val & 0x0F;
						fprintf(fpData, " %#01x", type);
					}
				}
				fprintf(fpLog, "\n");
				fprintf(fpData, "\n");
			} else {
				printf("\ncorrupted payload\n");
			}
			fclose(fpLog);
			fclose(fpData);
		}
	}
	bcm2835_spi_end();
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
	{0x0D,"Listen"}, {0x11, "paLevel"}, {0x18, "regLna"}, {0x19, "filtBand"}, 
	{0x1E, "AfcFei"}, {0x1F, "AfcVal1"}, 
	{0x20, "AfcVal0"}, {0x21, "FeiVal1"}, {0x22, "FeiVal0"},
	{0x23, "RssiDone"}, {0x24, "RssiVal"}, {0x27, "RdyFlag"},
	{0x28, "FifoFlg"}, {0x2E, "SyncConf"}, {0x37, "PktCfg"}, {0x38, "Payload"}, 
	{0x39, "NodeAddr"}};

 regSet regSetup[] = {
	{0x05, 0x01}, {0x06, 0xEC}, 				// frequency deviation 5kHz 0x0052 -> 30kHz 0x01EC
	{0x07, 0x6C}, {0x08, 0x93}, {0x09, 0x33},	// carrier freq 9xxMHz 0xE4C000 -> 434.3MHz 0x6C9333
	//{0x07, 0x6C}, {0x08, 0x80}, {0x09, 0x00},	// carrier freq 9xxMHz 0xE4C000 -> 433MHz 0x6C8000
	{0x0B, 0x20},								// standard AFC routine -> improved AFC routine
	{0x18, 0x08},								// 200ohms, gain by AGC loop -> 50ohms
	//{0x18, 0x0E},								// 200ohms, gain by AGC loop -> 50ohms, H gain - 48dB
	{0x19, 0x43},								// channel filter bandwidth 10kHz -> 60kHz  page:26
	//{0x1E, 0x14},								// AFC is performed each time rx mode is entered
	//{0x2C, 0x00}, {0x2D, 0x05},					// preamble size MSB, LSB 3 -> 5
	//{0x29, 220},								// RSSI threshold 0xE4 -> 0x??
	{0x2D, 0x04},								// preamble size
	{0x2E, 0x88},								// sych config x80 | x18 => 
	{0x2F, 0x2D},								// synch value 1
	{0x30, 0xD4},								// synch value 2
	//{0x37, 0x90},								// CRC on + packet fixed length -> CRC on + packet variable length
	{0x37, 0x20},								// CRC on -> CRC off, manchester
	//{0x3D, 0x12},								// packet config packet RX delay = 0 and autorestart ON x02 => delay = x?2 and ON
	//{255, 0},				// I have no idea what it is
	{0x01, 0x10}								// RX mode
}; 

void printReg(){
	int size = sizeof(regChk)/sizeof(regDesc), i;
	char tbuf[2], rbuf[2];
	time_t currentTime;
	char* timeString;
	currentTime = time(NULL);
	timeString = ctime(&currentTime);
	printf("\n\t%s", timeString);
	
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
	
void readFifo(char *rbuf){
	char addr[2], val[2];
	addr[0] = 0x28;											// FIFO flags address
	bcm2835_spi_transfernb(addr, val, 2);
	if (val[1] & 0x40){
		int i = 0;
		while (val[1] & 0x40){									// while FIFO not empty
			addr[0] = 0x00;										// FIFO data addr
			bcm2835_spi_transfernb(addr, val, 2);				// read FIFO
			rbuf[++i] = val[1];
			addr[0] = 0x28;										// FIFO flags address
			bcm2835_spi_transfernb(addr, val, 2);
		}
		rbuf[0] = i;
		for (;i < 64;)
			rbuf[++i] = 0;
	}
}

void printFifo(char *rbuf){
	time_t currentTime;
	currentTime = time(NULL);
	//printf("\n");
	printf("\n\t%s", ctime(&currentTime));
	int i, max = rbuf[0];
	for (i=1; i<=max; ++i){
		printf("FIFO[%d]=%02x%c", i, (int)rbuf[i], i%5==0?'\n':'\t');
	}
}
