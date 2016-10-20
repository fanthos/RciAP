#include <Arduino.h>

#define WRITERET(x, b, l) l = sizeof(x);memcpy(b,x,l);
#define R(x) retdata ## x

typedef unsigned char uchar;
typedef const uchar cuchar;
typedef unsigned int uint;
typedef const uint cuint;

uint playstatus = 0;

cuint STATUS_PUSH = 0x0001;

cuint IAP_BAUDRATE = 19200;
cuint IAP_BYTEPS = IAP_BAUDRATE / 10;
cuint IAP_MILLISBUF = 1;

inline void setstatus(cuint flag) {
	playstatus |= flag;
}

inline void clearstatus(cuint flag) {
	playstatus &= ~flag;
}

inline uchar getstatus(cuint flag) {
	return (playstatus & flag);
}

uchar calcsum(cuchar *buf, uint len) {
	uchar sum = 0;
	cuchar *buf1 = buf + len;
	while(buf < buf1) {
		sum -= *buf;
		buf++;
	}
	return sum;
}

cuchar
	NONE = 0,
	PLAY = 1,
	PAUSE = 2,
	NEXT = 8,
	PREV = 9,
	STOP = 10;

void playctrl(cuchar action) {
	Serial.print("Play Ctrl: ");
	Serial.println(action, HEX);
}

void procunkcmd(const uint len, cuchar recv[]) {
	Serial.print("Unknown Cmd: ");
	for(uint _i = 0; _i < len; _i++){
		Serial.print(recv[_i]);
		Serial.print(" ");
	}
	Serial.println("");
}

void serialwrite(cuchar buf[], const uint len) {
	static unsigned long lastsent = 0;
	unsigned long newsent = millis();
	const unsigned long sendinterval = 55;
	unsigned long t;
	unsigned long sendtime = len * 1000 / IAP_BYTEPS + IAP_MILLISBUF;
	t = (newsent - lastsent);
	Serial.println(newsent);
	if(t < sendinterval){
		Serial.println(sendinterval - t);
		delay(sendinterval - t);
	}
	lastsent = newsent + sendtime;
	Serial1.write(buf, len);
}

volatile uchar sendbuf_data[140] = {0xff, 0x55};
const volatile uchar *sendbuf = sendbuf_data + 3;
volatile uint sendbuflen;
volatile uchar sendbufready = 0;

void processsendbuf() {
	if(sendbufready == 0)return;
	Serial.println("ProcBuf");
	sendbuf_data[2] = (uchar)sendbuflen;
	sendbuf_data[sendbuflen + 3] = calcsum((uchar*)sendbuf_data + 2, sendbuflen + 1);
	serialwrite((uchar *)sendbuf_data, sendbuflen + 4);
	sendbufready = 0;
}

uint doreply(const uint len, cuchar recv[], uchar buf[]) {
	cuchar
		//retdata00ack[] = {0,0},
		retdata0007[] = "MYBT1",
		retdata0009[] = {3,1,1},
		retdata000B[] = "MYBT1ABCDEFG",
		retdata000D[] = "\0\x03\0\0MYBT1",
		retdata000F[] = {0,1,1},
		retdata04ack[] = {0,0,0},
		retdata04ackunk[] = {5,0,0},
		retdata04str[] = "BTAUDIO",
		//retdata04_0[] = {0},
		retdata04_00[] = {0,0,0,0},
		retdata04_01[] = {0,0,0,1},
		retdata04_0101[] = {0,0,0,1,0,0,0,1},
		retdata041C[] = {0,0,0xea,0x60,0,0,0x27,0x10,1};
	cuchar retdataary0028[] = {1,4,4,4,5,6,7};
	cuchar retdatabuf0013[] = {0x00, 0x27, 0x00};
	uint retlen;
	buf[0] = 0xff;
	buf[1] = 0x55;

	if(recv[0] == 0 && len > 1) {
		uchar *rbuf = buf + 5;
		buf[3] = 0;
		buf[4] = recv[1] + 1;
		switch(recv[1]) {
			case 0x13: // Get Device Lingoes
				if(sendbufready == 0) {
					memcpy((void*)sendbuf, retdatabuf0013, sizeof(retdatabuf0013));
					sendbuflen = 3;
					sendbufready = 1;
				}
				// fall to ACK
			case 0x05: // switch to remote ui
				rbuf[0] = 0;
				rbuf[1] = recv[1];
				buf[4] = 2;
				retlen = 2;
				break;
			case 0x03:
				rbuf[0] = 1;
				retlen = 1;
				break;
			case 0x07: // get ipod name
				WRITERET(R(0007),rbuf, retlen);
				break;
			case 0x09: // get ipod version
				WRITERET(R(0009),rbuf, retlen);
				break;
			case 0x0B: // get serial
				WRITERET(R(000B),rbuf, retlen);
				break;
			case 0x0D: // Get Device Type
				WRITERET(R(000D),rbuf, retlen);
				break;
			case 0x0F: // set lingo protocol version
				WRITERET(R(000F),rbuf, retlen);
				rbuf[0] = recv[2];
				break;
			case 0x28:
				rbuf[0] = retdataary0028[recv[2]];
				if(rbuf[0] > sizeof(retdataary0028)){
					return 0;
				}
				retlen = 1;
				buf[4] = 0x27;
				break;
			default:
				rbuf[0] = 5;
				rbuf[1] = recv[1];
				buf[4] = 2;
				retlen = 2;
				procunkcmd(len, recv);
				break;
		}
		retlen += 2;
	} else if (recv[0] == 4 && recv[1] == 0 && len > 2) {
		uchar *rbuf = buf + 6;
		buf[3] = 4;
		buf[4] = 0;
		buf[5] = recv[2] + 1;
		switch(recv[2]) {
			uchar action;
			case 0x04:
			case 0x0B:
			case 0x16:
			case 0x17:
			case 0x26:
			case 0x28:
			case 0x2E:
			case 0x31:
			case 0x37:
				WRITERET(R(04ack),rbuf, retlen);
				rbuf[2] = recv[2];
				buf[5] = 1;
				break;
			case 0x02:
			case 0x05:
				WRITERET(R(04_0101),rbuf, retlen);
				break;
			case 0x07:
			case 0x20:
			case 0x22:
			case 0x24:
				WRITERET(R(04str),rbuf, retlen);
				break;
			case 0x09:
			case 0x2C:
			case 0x2F:
				retlen = 1;
				rbuf[0] = 0;
				break;
			case 0x18:
				WRITERET(R(04_00),rbuf, retlen);
				break;
			case 0x1C:
				WRITERET(R(041C),rbuf, retlen);
				break;
			case 0x1E:
			case 0x35:
				WRITERET(R(04_01),rbuf, retlen);
				break;
			case 0x29:
				switch(recv[3]) {
					case 0x01:
					case 0x02:
					case 0x0A:
					case 0x0B:
						action = PLAY;
						break;
					case 0x03:
					case 0x08:
					case 0x0C:
						action = NEXT;
						break;
					case 0x04:
					case 0x09:
					case 0x0D:
						action = PREV;
						break;
					default:
						action = NONE;
						break;
				}
				if(action) {
					playctrl(action);
				}
				WRITERET(R(04ack),rbuf, retlen);
				rbuf[2] = recv[2];
				buf[5] = 1;
				break;
			default:
				WRITERET(R(04ackunk),rbuf, retlen);
				rbuf[2] = recv[2];
				buf[5] = 1;
				break;
		}
		retlen += 3;
	} else {
		procunkcmd(len, recv);
		return 0;
	}

	buf[2] = retlen;

	buf[retlen + 3] = calcsum(buf + 2, retlen + 1);
	return retlen + 4;
}

void setup() {
	Serial.begin(19200);
	Serial1.begin(IAP_BAUDRATE);
}

void processserial(cuchar input1) {
	static uchar buf[256], buf2[256];
	static char status = 0;
	static uint pos = 0;
	static uint len = 0;
	static unsigned long lasttime = -1;

	unsigned long newtime;
	newtime = millis();
	if(newtime - lasttime > 50) {
		status = -1;
	}
	lasttime = newtime;
	if(status == -1) {
		status = 0;
		pos = 0;
		len = 0;
	}
	if(input1 == 0xff && status == 0) {
		status = 1;
	} else if(input1 == 0x55 && status == 1) {
		status = 2;
	} else if(status == 2) {
		len = input1;
		status = 3;
	} else if(status == 3) {
		if(pos < len){
			buf[pos] = input1;
			pos++;
		} else {
			Serial.println("Process");
			uint retlen = doreply(len, buf, buf2);
			if(retlen > 0) {
				serialwrite(buf2, retlen);
			}
			status = -1;
		}
	} else {
		status = -1;
	}
}

void loop() {
	uchar ret1;
	processsendbuf();
	while(Serial1.available()) {
		ret1 = Serial1.read();
		processserial(ret1);
	}
	delay(5);
}
