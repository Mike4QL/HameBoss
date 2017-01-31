#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <bcm2835.h>
#include "decoder.h"

#define LEDG 				RPI_V2_GPIO_P1_15
#define LEDR 				RPI_V2_GPIO_P1_13
#define PIN_RESET			RPI_V2_GPIO_P1_22
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
#define SEED_PID			0x01
static uint8_t buf[0x02];

typedef enum {
	S_MSGLEN = 1,
	S_PRODID,
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
	uint8_t addr;
	uint8_t val;
} regSet_t;
typedef struct msg_t {
	state_t state;
	uint8_t msgSize, bytesToRead, dataLen;
	uint8_t paramId;
	uint8_t type;
	uint16_t pip;
	uint32_t value;
	uint8_t bufCnt;
	uint8_t buf[255];
} msg_t;
 void setupRegisters();
 uint8_t readFifoAll(uint8_t*);
 void regReadN(uint8_t* , uint8_t, uint8_t);
 void regWriteN(uint8_t*, uint8_t, uint8_t);
 uint8_t regRead(uint8_t);
 void regWrite(uint8_t, uint8_t);
 void changeMode(uint8_t);
 void assertRegVal(uint8_t, uint8_t, uint8_t, char*);
 void waitFor (uint8_t, uint8_t, uint8_t);
 void decryptMsg(uint8_t*, uint8_t);
 void encryptMsg(uint8_t*, uint8_t);
 void setupCrc(uint8_t*);
 void msgNextState(msg_t*);
 char* getIdName(uint8_t);
 char* getValString(uint64_t, uint8_t, uint8_t);
 void sendMessage(msg_t*);

// receive in variable length packet mode, display and resend. Data with swapped first 2 bytes
int main(int argc, char **argv){
	if (!bcm2835_init())
		return 1;
		
	time_t currentTime;
	
	struct tm *tmp;
	uint16_t payloadCnt = 0;
	uint8_t *rbuf;
	msg_t msg = {S_MSGLEN, 1, SIZE_MSGLEN, 0, 0, 0, 0, 0, 0};
	rbuf = (uint8_t*) malloc (256);
	
	// LED RELATED
	bcm2835_gpio_fsel(LEDG, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(LEDR, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(PIN_RESET, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(LEDG, LOW);
	bcm2835_gpio_write(LEDR, LOW);
	bcm2835_gpio_write(PIN_RESET, LOW);
	delay(10);

	bcm2835_spi_begin();	
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32); 	// 32 = 512ns = 7.8MHz
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); 				// CPOL = 0, CPHA = 0
	bcm2835_spi_chipSelect(BCM2835_SPI_CS1);					// chip select 1

	setupRegisters();
	waitFor(ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);				// wait until ready after mode switching
	
	readFifoAll(rbuf);
	while(1){
		waitFor(ADDR_IRQFLAGS2, MASK_PAYLOADRDY, TRUE);
		currentTime = time(NULL);
		tmp = gmtime(&currentTime);
		printf("%02d:%02d:%02d %d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, payloadCnt++);
		while (msg.state != S_FINISH)
		{
			//printf("\nbytes to read=%d, msgSize=%d, state=%d, ", msg.bytesToRead, msg.msgSize, msg.state);
			if (msg.bytesToRead == 0 || msg.msgSize == 0){
				printf("\nERROR: bytes to read=%d, msgSize=%d\n", msg.bytesToRead, msg.msgSize);
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
	{0x37, 0xA2},								// CRC on -> CRC off, manchester + node addr chk
	{0x38, 0xFF},								// payload length
	{0x39, 0x01},								// Node address
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
			if (rbuf)
			{
				rbuf[i++] = regRead(0x00);
			}
			else
			{
				regRead(0x00);
			}
		} while (regRead(0x28) & 0x40);
	}
	return i;
}
 void regReadN(uint8_t *retBuf, uint8_t addr, uint8_t size){
	retBuf[0] = addr;
	bcm2835_spi_transfern((char*)retBuf, size);
}
 void regWriteN(uint8_t *retBuf, uint8_t addr, uint8_t size){
	uint8_t temp = retBuf[0];
	retBuf[0] = addr | 0x80;
	bcm2835_spi_writenb((char*)retBuf, size);
	retBuf[0] = temp;
}
 uint8_t regRead(uint8_t addr){
	buf[0] = addr;
	bcm2835_spi_transfern((char*)buf, 2);
	return buf[1];
}
 void regWrite(uint8_t addr, uint8_t val){
	buf[0] = addr | 0x80;
	buf[1] = val;
	bcm2835_spi_writenb((char*)buf, 2);
	return;
}
 void changeMode(uint8_t mode){
	buf[0] = 0x81;
	buf[1] = mode;
	bcm2835_spi_writenb((char*)buf, 2);
}
 void assertRegVal(uint8_t addr, uint8_t mask, uint8_t val, char *desc){
	buf[0] = addr;
	bcm2835_spi_transfern((char*)buf, 2);
	if (val){
		if ((buf[1] & mask) != mask)
			printf("ASSERTION FAILED: addr:%02x, expVal:%02x(mask:%02x) != val:%02x, desc: %s\n", addr, val, mask, buf[1], desc);
	} else {
		if ((buf[1] & mask) != 0)
			printf("ASSERTION FAILED: addr:%02x, expVal:%02x(mask:%02x) != val:%02x, desc: %s\n", addr, val, mask, buf[1], desc);
	}
}
 void waitFor (uint8_t addr, uint8_t mask, uint8_t val){
	uint32_t cnt = 0; 
	uint8_t ret;
	do {
		//++cnt;
		if (cnt % 800000 == 0){
			if (cnt > 4000000)
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
 void decryptMsg(uint8_t *buf, uint8_t size){
	uint8_t i;
	seed(SEED_PID, (uint16_t)(buf[2]<<8)|buf[3]);
	for (i = 4; i <= size; ++i)
		buf[i] = decrypt(buf[i]);	
}
 void encryptMsg(uint8_t *buf, uint8_t size){
	uint8_t i;
	seed(SEED_PID, (uint16_t)(buf[2]<<8)|buf[3]);
	for (i = 4; i <= size; ++i)
		buf[i] = decrypt(buf[i]);	
}
 void setupCrc(uint8_t *buf){
	uint16_t val, size = buf[0];
	val = crc((uint8_t*)buf + 4, size - 5);
	buf[size - 1] = val >> 8;
	buf[size] = val & 0x00FF;
}
 void msgNextState(msg_t *msg){				// switch and initialize next state
	char *temp;
	switch (msg->state)
	{
		case S_MSGLEN:
			msg->state = S_PRODID;
			msg->bytesToRead = SIZE_MSGLEN;
			msg->msgSize = msg->value;
			break;
		case S_PRODID:
			msg->state = S_ENCRYPTPIP;
			msg->bytesToRead = SIZE_ENCRYPTPIP;
			break;
		case S_ENCRYPTPIP:
			msg->state = S_SENSORID;
			msg->bytesToRead = SIZE_SENSORID;
			msg->pip = msg->value;
			seed(SEED_PID, msg->pip);
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
			msg->paramId = msg->value;
			temp = getIdName(msg->paramId);
			printf(" %s=", temp);
			if (msg->paramId == 0)			// Parameter identifier for CRC
			{
				msg->state = S_CRC;
				msg->bytesToRead = SIZE_CRC;
			}
			else
			{
				msg->state = S_DATA_TYPEDESC;
				msg->bytesToRead = SIZE_DATA_TYPEDESC;
			}
			if (msg->paramId == 0x73)						// Switch state
			{
				msg->state = S_DATA_VAL;
				msg->bytesToRead = 2;
				msg->type = 0x00;
			}
			if (msg->paramId == 0x6A)						// Join
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
			msg->state = S_MSGLEN;
			msg->bytesToRead = SIZE_MSGLEN;
			if (msg->msgSize > 0)
				printf("\nShouldn't be there more data?!\n");
			msg->msgSize = 1;
			int i;
			for (i = 0; i < msg->bufCnt; ++i){
				printf("[%d]=%02x%c", i, msg->buf[i], i%8==7?'\n':'\t');
			}
			sendMessage(msg);
			printf("\n\n");
			msg->bufCnt = 0;
			msg->value = 0;
			readFifoAll(msg->buf);
			//regWrite(ADDR_IRQFLAGS1, 0x00);
			assertRegVal(ADDR_IRQFLAGS1, 0x01, FALSE, "end of the message");
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
		case 0x61:
			return "Actuate_switch";
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
void sendMessage(msg_t *msg){
	uint8_t tbuf[0x72];
	static uint8_t ledState = 0;
	uint8_t i, size;

	if (msg->buf[3] % 2){
		bcm2835_gpio_write(LEDR, HIGH);
	} else {
		bcm2835_gpio_write(LEDR, LOW);
	}

	msg->buf[0] += 3;
	size = msg->buf[0];
	msg->buf[size -5] = 0x73;			// Switch state
	msg->buf[size -4] = 0;
	msg->buf[size -3] = msg->buf[3] % 2;
	msg->buf[size -2] = 0;				// CRC type
	msg->buf[4] = 0xDE;					// Sensor ID
	msg->buf[5] = 0xCA;					// Sensor ID
	msg->buf[6] = 0xDE;					// Sensor ID
	msg->buf[1] = 0x01;					// Product ID
	setupCrc(msg->buf);
	printf("\nSend msg data: ");
	for (i=0; i < 29 ; ++i)
		printf("%02x, ",msg->buf[i]);
	printf("\n");
	encryptMsg(msg->buf, size);
	
	//for (i = 0; i < msg->bufCnt + 3; ++i){
	//	printf("[%d]=%02x%c", i, msg->buf[i], i%8==7?'\n':'\t');
	//}
	
	readFifoAll(NULL);
	assertRegVal(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY, FALSE, "FIFO flushed");
	assertRegVal(ADDR_MODE, RECEIVER, TRUE, "FIFO flushed");
	
	changeMode(STANDBY);									// Switch to standby mode
	waitFor (ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);			// wait for ModeReady
	assertRegVal(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY, FALSE, "FIFO flushed, than mode changed to standby");
	assertRegVal(ADDR_MODE, STANDBY, TRUE, "FIFO flushed, than mode changed to standby");
	
	tbuf[0] = 0x80;
	if (size > 0x40)
	{
		size = 0x40;
	}
	for (i = 1; i <= size + 1; i++)								// Fill FIFO
		tbuf[i] = msg->buf[i-1];
	regWriteN(tbuf, 0, size + 2);

	assertRegVal(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY, TRUE, "data pushed into FIFO");
	assertRegVal(ADDR_IRQFLAGS2, MASK_FIFOOVERRUN, FALSE, "data pushed into FIFO");
	
	changeMode(TRANSMITER);									// Switch to TX mode

	assertRegVal(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY, TRUE, "data pushed into FIFO and mode changed to TX");
	assertRegVal(ADDR_IRQFLAGS1, MASK_TXREADY, FALSE, "data pushed into FIFO and mode changed to TX");
	assertRegVal(ADDR_DATAMODUL, (MASK_PACKETMODE | MASK_MODULATION), FALSE, "data pushed into FIFO and mode changed to TX");

	waitFor (ADDR_IRQFLAGS1, MASK_MODEREADY | MASK_TXREADY, TRUE);		// wait for ModeReady + TX ready

	assertRegVal(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY, TRUE, "TX mode just ready");
	assertRegVal(ADDR_IRQFLAGS1, MASK_TXREADY, TRUE, "TX mode just ready");
	assertRegVal(ADDR_MODE, TRANSMITER, TRUE, "TX mode just ready");

	waitFor (ADDR_IRQFLAGS2, MASK_PACKETSENT, TRUE);					// wait for Packet sent
	assertRegVal(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY | MASK_FIFOOVERRUN, FALSE, "are all bytes sent?");

	changeMode(STANDBY);												// Switch to standby mode
	waitFor (ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);						// wait for ModeReady
	assertRegVal(ADDR_MODE, STANDBY, TRUE, "near the end");

	changeMode(RECEIVER);												// Switch to RX mode
	waitFor (ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);						// wait for ModeReady
	assertRegVal(ADDR_MODE, RECEIVER, TRUE, "near he end");

	// LED RELATED
	if (ledState == 1){
		ledState = 0;
		bcm2835_gpio_write(LEDG, LOW);
	} else {
		ledState = 1;
		bcm2835_gpio_write(LEDG, HIGH);
	}
}
