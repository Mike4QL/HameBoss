#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <bcm2835.h>
#include "decoder.h"

#define PIN1 				RPI_GPIO_P1_07
#define PIN2 				RPI_GPIO_P1_08
#define TRUE				1
#define FALSE				0
#define STANDBY 			0x04
#define TRANSMITER 			0x0C
#define RECEIVER 			0x10
#define SIZE_PRODID			1
#define SIZE_MSGLEN			1
#define SIZE_ENCRYPTPIP		2
#define SIZE_SENSORID		3
#define SIZE_PARAMSET		1
#define SIZE_DATA_PARAMID	1
#define SIZE_DATA_TYPEDESC	1
#define SIZE_CRC			2
#define BYTES_TILL_MSG_LEN	2
#define ADDR_FIFO			0x00
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
#define MASK_PAYLOADRDY		0x04
static char buf[0x02];

typedef enum {
	S_PRODID = 1,
	S_MSGLEN,
	S_ENCRYPTPIP,
	S_SENSORID,
	S_PARAMSET,
	S_DATA_PARAMID,
	S_DATA_TYPEDESC,
	S_DATA_VAL,
	S_CRC,
	S_FINISH
} state_t;
typedef struct regSet_t {
	char addr;
	char val;
} regSet_t;
typedef struct msg_t {
	state_t state;
	uint8_t msgSize, bytesToRead, dataLen;
	uint8_t type;
	uint16_t pip;
	uint32_t value;
	uint8_t bufCnt;
	uint8_t buf[255];
} msg_t;
 void setupRegisters();
 uint8_t readFifoAll(uint8_t*);
 void regReadN(char* , uint8_t, uint8_t);
 void regWriteN(char*, uint8_t, uint8_t);
 uint8_t regRead(uint8_t);
 void regWrite(uint8_t, uint8_t);
 void changeMode(uint8_t);
 void assertRegVal(uint8_t, uint8_t, uint8_t, char*);
 void waitFor (uint8_t, uint8_t, uint8_t);
 void decryptMsg(char*, uint8_t);
 void encryptMsg(char*, uint8_t);
 void setupCrc(char*);
 void msgNextState(msg_t*);
 char* getIdName(uint8_t);
 char* getValString(uint64_t, uint8_t, uint8_t);

// receive in unlimited length mode and display data
int main(int argc, char **argv){
	if (!bcm2835_init())
		return 1;
	
	time_t currentTime;
	currentTime = time(NULL);
	struct tm *tmp;
	uint16_t payloadCnt = 0;
	uint8_t *rbuf;
	msg_t msg = {S_PRODID, BYTES_TILL_MSG_LEN, SIZE_PRODID, 0, 0, 0, 0, 0};
	uint8_t buffSize = 0xFF;
	rbuf = (uint8_t*) malloc (buffSize);
	
	bcm2835_spi_begin();	
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32); 	// 32 = 512ns = 7.80625MHz
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); 				// CPOL = 0, CPHA = 0
	bcm2835_spi_chipSelect(BCM2835_SPI_CS1);					// chip select 1

	setupRegisters();
	waitFor(ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);				// wait until ready after mode switching
	
	// LED RELATED
	bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(PIN1, HIGH);
	bcm2835_gpio_write(PIN2, LOW);
	
	readFifoAll(rbuf);
	regWrite(0x38, 0x00); // payload length (unlimited mode)
	while(1){
		
	
		//waitFor(ADDR_IRQFLAGS2, MASK_PAYLOADRDY, TRUE);
		waitFor(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY, TRUE);
		tmp = gmtime(&currentTime);
		printf("%02d:%02d:%02d %d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, payloadCnt++);
		while (msg.state != S_FINISH)
		{
			//printf("\nbytes to read=%d, msgSize=%d, state=%d, ", msg.bytesToRead, msg.msgSize, msg.state);
			if (msg.bytesToRead == 0 || msg.msgSize == 0){
				printf("\nERROR: bytes to read=%d, msgSize=%d", msg.bytesToRead, msg.msgSize);
				msg.state = S_FINISH;
				break;
			}
			waitFor(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY, TRUE);
			if (msg.state > S_ENCRYPTPIP)						// in states after S_ENCYPTPIP bytes need to be decrypted
			{
				msg.buf[msg.bufCnt++] = decrypt(regRead(ADDR_FIFO));
			}
			else
			{
				msg.buf[msg.bufCnt++] = regRead(ADDR_FIFO);
			}
			msg.value = (msg.value << 8) | msg.buf[msg.bufCnt - 1];
			--msg.bytesToRead;
			--msg.msgSize;

			if (msg.bytesToRead == 0)
			{
				msgNextState(&msg);
				msg.value = 0;
			}
		}
		msgNextState(&msg);
	}
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
	{0x37, 0x20},								// CRC on -> CRC off, manchester
	{0x38, 30},									// payload length
	//{0x39, 0x12},								// Node address
	//{0x3D, 0x12},								// packet config packet RX delay = 0 and autorestart ON x02 => delay = x?2 and ON
	{0x01, 0x10}								// RX mode
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
		if (cnt % 1000000 == 0){
			if (cnt > 20000000)
			{
				printf("timeout inside a while for addr %02x\n", addr);
				break;
			}
			else
			{
				//printf("wait long inside a while for addr %02x\n", addr);
			}
		}
		ret = regRead(addr);
	} while ((ret & mask) != (val ? mask : 0));
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
 void msgNextState(msg_t *msg){				// switch and initialize next state
	char *temp;
	switch (msg->state)
	{
		case S_PRODID:
			msg->state = S_MSGLEN;
			msg->bytesToRead = SIZE_MSGLEN;
			break;
		case S_MSGLEN:
			msg->state = S_ENCRYPTPIP;
			msg->bytesToRead = SIZE_ENCRYPTPIP;
			msg->msgSize = msg->value - 2;
			break;
		case S_ENCRYPTPIP:
			msg->state = S_SENSORID;
			msg->bytesToRead = SIZE_SENSORID;
			msg->pip = msg->value;
			seed(0xEF, msg->pip);
			break;
		case S_SENSORID:
			msg->state = S_PARAMSET;
			msg->bytesToRead = SIZE_PARAMSET;
			printf(" SensorID=%#08x", msg->value);
			break;
		case S_PARAMSET:
			msg->state = S_DATA_PARAMID;
			msg->bytesToRead = SIZE_DATA_PARAMID;
			break;
		case S_DATA_PARAMID:
			temp = getIdName(msg->value);
			printf(" %s=", temp);
			if (msg->value == 0)			// Parameter identifier for CRC
			{
				msg->state = S_CRC;
				msg->bytesToRead = SIZE_CRC;
			}
			else
			{
				msg->state = S_DATA_TYPEDESC;
				msg->bytesToRead = SIZE_DATA_TYPEDESC;
			}
			if (msg->value == 0x73)						// Switch state
			{
				msg->state = S_DATA_VAL;
				msg->bytesToRead = 2;
				msg->type = 0x00;
				static uint8_t ledState = 1;
				if (ledState == 1){
					ledState = 0;
					bcm2835_gpio_write(PIN1, LOW);
				} else {
					ledState = 1;
					bcm2835_gpio_write(PIN1, HIGH);
				}
			}
			if (msg->value == 0x6A)						// Join
			{
				msg->state = S_DATA_VAL;
				msg->bytesToRead = 1;
				msg->type = 0x00;
			}
			if (strcmp(temp, "Unknown") == 0)
				msg->state = S_FINISH;
			break;
		case S_DATA_TYPEDESC:
			msg->state = S_DATA_VAL;
			msg->bytesToRead = msg->value & 0x0F;
			msg->type = msg->value;
			break;
		case S_DATA_VAL:
			temp = getValString(msg->value, msg->type >> 4, msg->dataLen);
			printf("%s", temp);
			msg->state = S_DATA_PARAMID;
			msg->bytesToRead = SIZE_DATA_PARAMID;
			if (strcmp(temp, "Reserved") == 0)
				msg->state = S_FINISH;
			break;
		case S_CRC:
			msg->state = S_FINISH;
			if ((int16_t)msg->value == crc(msg->buf + 4, msg->bufCnt - 6))
			{
				printf("OK\n");
			}
			else
			{
				printf("FAIL expVal=%04x, pip=%04x, val=%04x\n", (int16_t)msg->value, msg->pip, crc(msg->buf + 4, msg->bufCnt - 6));
			}
			break;
		case S_FINISH:
			msg->state = S_PRODID;
			msg->bytesToRead = SIZE_PRODID;
			if (msg->msgSize > 0)
				printf("\nShouldn't be there more data?!\n");
			msg->msgSize = BYTES_TILL_MSG_LEN;
			int i;
			for (i = 0; i < msg->bufCnt; ++i){
				printf("[%d]=%02x%c", i, msg->buf[i], i%8==7?'\n':'\t');
			}
			printf("\n\n");
			msg->bufCnt = 0;
			msg->value = 0;
			readFifoAll(msg->buf);
			changeMode(STANDBY);
			waitFor(ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);
			changeMode(RECEIVER);
			waitFor(ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);
			//assertRegVal(ADDR_IRQFLAGS1, 0x01, FALSE, "end of the message");
			break;
		default:
			printf("\nYou are in an non existing state!!");
	}
	msg->dataLen = msg->bytesToRead;
}
char* getIdName(uint8_t val){
	switch (val){
		case 0x6A:
			return "Join";
		case 0x70:
			return "Power";
		case 0x71:
			return "Reactive_P";
		case 0x76:
			return "Voltage";
		case 0x69:
			return "Current";
		case 0x66:
			return "Frequency";
		case 0x73:
			return "Switch_state";
		case 0x00:
			return "CRC";
		default:
			return "Unknown";
	}
}
char* getValString(uint64_t dataVal, uint8_t type, uint8_t length){
	static char str[20];
	if (type >= 0 && type <= 6){				// unsigned integer
		sprintf(str, "%g", (double)dataVal / (1 << (4*type)));
	} else if (type == 7) {						// characters
		int8_t i;
		char *ch = (char*)&dataVal;
		for (i = length - 1; i >= 0; --i)
			sprintf(str, "%c",ch[i]);
	} else if (type >= 8 && type <= 11){		// signed integer
		if (dataVal & (1uLL << (length*8-1))){		// check neg
			int8_t i;
			for (i = 0; i < length; ++i)
				dataVal ^= 0xFFuLL << 8*i;
			sprintf(str, "-%g", (double)(dataVal+1) / (1 << (8*(type-8))));
		} else
			sprintf(str, "%g", (double)dataVal / (1 << (8*(type-8))));
	} else if (type == 15){						// floating point
		if (dataVal & (1uLL << (length*8-1))){		// check neg
			int8_t i;
			for (i = 0; i < length; ++i)
				dataVal ^= 0xFFuLL << 8*i;
			sprintf(str, "-%g", (double)(dataVal+1) / (1 << (length == 2? 11 : length == 4 ? 24 : 53)));
		} else
			sprintf(str, "%g", (double)dataVal / (1 << (length == 2? 11 : length == 4 ? 24 : 53)));
	} else {									// reserved
		sprintf(str, "Reserved");
	}
	
	return str;
}

