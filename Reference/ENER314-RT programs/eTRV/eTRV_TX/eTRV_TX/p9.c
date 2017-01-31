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
void sendMessage(char*);
// receive and display data
int main(int argc, char **argv){
	if (!bcm2835_init())
		return 1;
	
	FILE *fpLog, *fpData;
	remove("/home/pi/Desktop/CProj/log.txt");
	remove("/home/pi/Desktop/CProj/data.txt");
	char *tbuf, *rbuf;
	int8_t i, buffSize = 0x72;
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
		//if (rbuf[1] & 0x40){									// FIFO not empty
			static int32_t payloadCnt = 1;
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
			fprintf(fpData, "%02d:%02d:%02d %d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, payloadCnt);
			printf("%02d:%02d:%02d %d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, payloadCnt++);
			readFifo(rbuf);
			//printFifo(rbuf);

			if (rbuf[1] <= 40 && rbuf[1] >= 20){
				//printf("\nproduct_ID=%02x, msg_len=%d, PIP=%04x\n", rbuf[0], rbuf[1], (uint16_t)(rbuf[2]<<8)|rbuf[3]);
				fprintf(fpLog, " %02x %d %02x %02x", rbuf[0], rbuf[1], rbuf[2], rbuf[3]);
				int8_t dataLen = 0;
				uint8_t type, length, size = rbuf[1], crcFlag = 0;
				uint64_t dataVal = 0;
				char *helpStr;
				
				seed(0xEF, (uint16_t)(rbuf[2]<<8)|rbuf[3]);
				for (i = 4; i < size; ++i)
					rbuf[i] = decrypt(rbuf[i]);

				printf(" SensorID=%#08x", rbuf[4]<<16|rbuf[5]<<8|rbuf[6]);
				
				// LED RELATED
				// uint8_t sensId = rbuf[4]|rbuf[5]|rbuf[6];
				
				if (((int16_t)(rbuf[size-2]<<8) | rbuf[size-1]) == crc((uint8_t*)rbuf + 4, size - 6)){
					fprintf(fpData, " CRC PASS");
					printf(" CRC=PASS");
					crcFlag = 1;
				} else {
					fprintf(fpData, " CRC FAIL: expected:%04x result:%04x", ((int16_t)(rbuf[size-2]<<8) | rbuf[size-1]), (int16_t)crc((uint8_t*)rbuf + 4, size - 6));
					printf(" CRC=FAIL");
				}

				for (i = 4; i < size - 3; ++i){

					char val = rbuf[i];
					fprintf(fpLog, " %02x", val);
					//printf("Msg[%d]=%02x%c", i-3, val, i%5==3?'\n':'\t');
					if (i < 8)
						continue;
					if (dataLen > 0){
						dataVal = dataVal * 256 + val;
						--dataLen;
						if (dataLen == 0){								// print data as a correct data type
							fprintf(fpData, " %#x", (uint32_t)dataVal);
							if (type >= 0 && type <= 6){				// unsigned integer
								printf("%g", (double)dataVal / (1 << (4*type)));
							} else if (type == 7) {						// characters
								int8_t i;
								char *ch = (char*)&dataVal;
								for (i = length - 1; i >= 0; --i)
									printf("%c",ch[i]);
							} else if (type >= 8 && type <= 11){		// signed integer
								if (dataVal & (1uLL << (length*8-1))){		// check neg
									int8_t i;
									for (i = 0; i < length; ++i)
										dataVal ^= 0xFFuLL << 8*i;
									printf("-%g", (double)(dataVal+1) / (1 << (8*(type-8))));
								} else
									printf("%g", (double)dataVal / (1 << (8*(type-8))));
							} else if (type == 15){						// floating point
								if (dataVal & (1uLL << (length*8-1))){		// check neg
									int8_t i;
									for (i = 0; i < length; ++i)
										dataVal ^= 0xFFuLL << 8*i;
									printf("-%g", (double)(dataVal+1) / (1 << (length == 2? 11 : length == 4 ? 24 : 53)));
								} else
									printf("%g", (double)dataVal / (1 << (length == 2? 11 : length == 4 ? 24 : 53)));
							} else {									// reserved
								printf("reserved dType");
							}
							dataVal = 0;
						}
					} else if (dataLen == 0){
						dataLen = -1;
						switch (val){
							case 0x6A:
								helpStr = "Join";
								type = 0;
								dataLen = 1;
								length = dataLen;
								break;
							case 0x70:
								helpStr = "Power";
								break;
							case 0x71:
								helpStr = "Reactive_P";
								break;
							case 0x76:
								helpStr = "Voltage";
								break;
							case 0x69:
								helpStr = "Current";
								break;
							case 0x66:
								helpStr = "Frequency";
								break;
							case 0x73:
								helpStr = "Switch state";
								break;
							case 0x00:
								helpStr = "CRC";
								dataLen = 2;
								break;
							default:
								helpStr = "Unknown";
						}
						fprintf(fpData, " %s", helpStr);
						printf(" %s=", helpStr);
					} else {
						type = val >> 4;
						dataLen = val & 0x0F;
						length = dataLen;
						fprintf(fpData, " %#01x", type);
					}
				}
				fprintf(fpLog, "\n");
				fprintf(fpData, "\n");
				printf("\n");
				if (crcFlag == 1)
					sendMessage(rbuf);
			} else {
				printf(" message size incorrect: %d bytes\n", rbuf[1]);
				fprintf(fpLog, "\n");
				fprintf(fpData, " message_size_incorrect: %d\n", rbuf[1]);
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
	{0x2C, 0x00}, {0x2D, 0x05},					// preamble size MSB, LSB 3 -> 5
	//{0x29, 220},								// RSSI threshold 0xE4 -> 0x??
	{0x2D, 0x04},								// preamble size
	{0x2E, 0x88},								// sych config x80 | x18 => 
	{0x2F, 0x2D},								// synch value 1
	{0x30, 0xD4},								// synch value 2
	//{0x37, 0x90},								// CRC on + packet fixed length -> CRC on + packet variable length
	{0x37, 0x22},								// CRC on -> CRC off, manchester
	{0x38, 28},									// Payload length
	{0x39, 0x01},								// Node address
	{0x3C, 27},									// FIFO threshold
	//{0x3D, 0x12},								// packet config packet RX delay = 0 and autorestart ON x02 => delay = x?2 and ON
	{0x01, 0x10}								// RX mode
}; 

void printReg(){
	uint8_t size = sizeof(regChk)/sizeof(regDesc), i;
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

void printFifo(char *rbuf){
	time_t currentTime;
	currentTime = time(NULL);
	//printf("\n");
	printf("\n\t%s", ctime(&currentTime));
	uint8_t i, max = rbuf[0];
	for (i=1; i<=max; ++i){
		printf("FIFO[%d]=%02x%c", i, (int)rbuf[i], i%5==0?'\n':'\t');
	}
}

void sendMessage(char *buf){
	char tbuf[0x41];
	static uint8_t sendMsgCnt = 0, ledState = 0;
	uint8_t i;
		
	// LED RELATED
	if (ledState == 1){
		ledState = 0;
		bcm2835_gpio_write(PIN1, LOW);
	} else {
		ledState = 1;
		bcm2835_gpio_write(PIN1, HIGH);
	}
	
	tbuf[0] = 0x81;
	tbuf[1] = 0x04;
	bcm2835_spi_writenb(tbuf, 2);								// Switch to standby mode
	
	do {
		tbuf[0] = 0x27;
		bcm2835_spi_transfern(tbuf, 2);
	} while ((tbuf[1] & 0x80) == 0x00);							// wait for ModeReady
	
	tbuf[0] = 0x80;
	for (i = 1; i < buf[1]; i++)
		tbuf[i] = buf[i-1];
	tbuf[6] = 0xC0;
	tbuf[7] = 0xC0;
	if (sendMsgCnt % 5 == 0){									// Add switch state to message
		tbuf[i++] = 0x73;
		tbuf[i++] = 0x00;
		tbuf[i++] = 0x01;
	}
	for (i = 0; i < buf[1]; i++)
		printf("%02x, ", tbuf[i]);
	printf("\n");
	bcm2835_spi_writenb(tbuf, 29);								// Fill FIFO

	tbuf[0] = 0x81;
	tbuf[1] = 0x0C;
	bcm2835_spi_writenb(tbuf, 2);								// Switch to TX mode

	do {
		tbuf[0] = 0x28;
		bcm2835_spi_transfern(tbuf, 2);
	} while ((tbuf[1] & 0x08) == 0x00);							// wait for Packet sent
	
	tbuf[0] = 0x81;
	tbuf[1] = 0x04;
	bcm2835_spi_writenb(tbuf, 2);								// Switch to standby mode
	
	do {
		tbuf[0] = 0x27;
		bcm2835_spi_transfern(tbuf, 2);
	} while ((tbuf[1] & 0x80) == 0x00);							// wait for ModeReady

	tbuf[0] = 0x81;
	tbuf[1] = 0x10;
	bcm2835_spi_writenb(tbuf, 2);								// Switch to RX mode
	
	do {
		tbuf[0] = 0x27;
		bcm2835_spi_transfern(tbuf, 2);
	} while ((tbuf[1] & 0x80) == 0x00);							// wait for ModeReady
	
	++sendMsgCnt;
	return;
}
