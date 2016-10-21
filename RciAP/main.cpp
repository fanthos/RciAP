#include <Arduino.h>

#define WRITERET(x, b, l) l = sizeof(x);memcpy(b,x,l);
#define R(x) retdata ## x

typedef unsigned char uchar;
typedef const uchar cuchar;
typedef unsigned int uint;
typedef const uint cuint;
typedef unsigned long ulong;
typedef const ulong culong;

uint playstatus = 0;

cuint STATUS_PUSH = 0x0001;

culong IAP_BAUDRATE = 19200;
culong IAP_BYTEPS = IAP_BAUDRATE / 10;
culong IAP_MAXWAIT = 256L * 1000L / IAP_BYTEPS + 1;
culong IAP_MILLISBUF = 1;

inline void setstatus(cuint flag) {
	playstatus |= flag;
}

inline void clearstatus(cuint flag) {
	playstatus &= ~flag;
}

inline uchar getstatus(cuint flag) {
	return (playstatus & flag);
}

inline void mydelay(culong t) {
	Serial.print("mydelay: ");
	Serial.println(t);
	delay(t);
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
	Serial.print("play: ");
	Serial.println(action, HEX);
}

void procunkcmd(cuint len, cuchar recv[], const char lbl[]) {
	Serial.print(lbl);
	for(uint _i = 0; _i < len; _i++){
		Serial.print(recv[_i], HEX);
		Serial.print(" ");
	}
	Serial.println("");
}

void serialwrite(cuchar buf[], cuint len) {
	static ulong lastsent = 0;
	ulong newsent;
	culong sendinterval = 10;
	ulong t;
	ulong sendtime = len * 1000 / IAP_BYTEPS + IAP_MILLISBUF;
	procunkcmd(len - 2, buf + 2, "send: ");
	newsent = millis();
	if(lastsent > newsent) {
		if(lastsent - newsent > IAP_MAXWAIT) {
			t = IAP_MAXWAIT;
		}
		mydelay(t + sendinterval);
	} else {
		t = (newsent - lastsent);
		if(t < sendinterval){
			mydelay(sendinterval - t);
		}
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
	Serial.print("P");
	sendbuf_data[2] = (uchar)sendbuflen;
	sendbuf_data[sendbuflen + 3] = calcsum((uchar*)sendbuf_data + 2, sendbuflen + 1);
	serialwrite((uchar *)sendbuf_data, sendbuflen + 4);
	sendbufready = 0;
}

uint doreply(cuint len, cuchar recv[], uchar buf[]) {
	cuchar
		//retdata00ack[] = {0,0},
		retdata0007[] = "MYBT1",
		retdata0009[] = {8,1,1},
		retdata000B[] = "MYBT1ABCDEFG",
		retdata000F[] = {0,1,15},
		retdata04ack[] = {0,0,0},
		retdata04ackunk[] = {5,0,0},
		retdata_str[] = "BTAUDIO",
		//retdata04_0[] = {0},
		retdata_04str[] = "\0\0\0\0MY888",
		retdata04_00[] = {0,0,0,0},
		retdata04_01[] = {0,0,0,1},
		retdata04_02[] = {0,0,0,2},
		retdata04_03[] = {0,0,0,3},
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
			case 0x01:
			case 0x02:
				return 0;
			case 0x13: // Get Device Lingoes
				if(sendbufready == 1) {
					processsendbuf();
				}
				memcpy((void*)sendbuf, retdatabuf0013, sizeof(retdatabuf0013));
				sendbuflen = 3;
				sendbufready = 1;
				// fall to ACK
			case 0x05: // switch to remote ui
			case 0x06: // switch to remote ui
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
				WRITERET(R(_04str),rbuf, retlen);
				buf[1] = 3;
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
				procunkcmd(len, recv, "!!!Unk: ");
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
			case 0x38:
				WRITERET(R(04ack),rbuf, retlen);
				rbuf[2] = recv[2];
				buf[5] = 1;
				break;
			case 0x02:
				WRITERET(R(04_0101),rbuf, retlen);
				break;
			case 0x05:
				WRITERET(R(04_02),rbuf, retlen);
				break;
			case 0x07:
			case 0x20:
			case 0x22:
			case 0x24:
				WRITERET(R(_str),rbuf, retlen);
				break;
			case 0x09:
			case 0x2C:
			case 0x2F:
				retlen = 1;
				rbuf[0] = 0;
				break;
			case 0x18:
				WRITERET(R(04_03),rbuf, retlen);
				break;
			case 0x1C:
				WRITERET(R(041C),rbuf, retlen);
				break;
			case 0x1E:
				WRITERET(R(04_01),rbuf, retlen);
				break;
			case 0x35:
				WRITERET(R(04_03),rbuf, retlen);
				break;
			case 0x1A:
				{
					ulong id_s, id_e;
					id_s = (recv[3] << 24) | (recv[4] << 16) | (recv[5] << 8) | recv[6];
					id_e = ((recv[7] << 24) | (recv[8] << 16) | (recv[9] << 8) | recv[10]) + id_s;
					memcpy(rbuf+4, R(_str), sizeof(R(_str)));
					retlen = sizeof(R(_str)) + 4;
					while(id_s < id_e) {
						if(sendbufready == 1) {
							processsendbuf();
						}
						rbuf[0] = id_s >> 24;
						rbuf[1] = id_s >> 16;
						rbuf[2] = id_s >> 8;
						rbuf[3] = id_s;
						memcpy((void*)sendbuf, buf + 3, len + 3);
						sendbuflen = retlen;
						sendbufready = 1;
						id_s ++;
					}
					return 0;
				}
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
			case 0x37:
				WRITERET(R(04ack),rbuf, retlen);
				rbuf[2] = recv[2];
				buf[5] = 1;
				break;
			default:
				WRITERET(R(04ackunk),rbuf, retlen);
				rbuf[2] = recv[2];
				buf[5] = 1;
				procunkcmd(len, recv, "!!!Unk: ");
				break;
		}
		retlen += 3;
	} else {
		procunkcmd(len, recv, "!!!Unk: ");
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
	static uchar _buf[258], buf2[256];
	static uchar *buf = _buf +1;
	static char status = 0;
	static uint pos = 0;
	static uint len = 0;
	static uchar sum = 0;;
	static ulong lasttime = -1;

	ulong newtime;
	newtime = millis();
	if(newtime - lasttime > 100) {
		//status = -1;
	}
	lasttime = newtime;
	if(status == -1) {
		status = 0;
		pos = 0;
		len = 0;
		sum = 0;
	}
	if(input1 == 0xff && status == 0) {
		status = 1;
	} else if(input1 == 0x55 && status == 1) {
		status = 2;
	} else if(status == 2) {
		len = input1;
		sum = input1;
		status = 3;
	} else if(status == 3) {
		if(pos < len){
			buf[pos] = input1;
			sum += input1;
			pos++;
		} else {
			status = 4;
			_buf[0] = len;
			buf[len] = input1;
			procunkcmd(len + 2, _buf, "recv: ");
			sum += input1;
			if(sum == 0) {
				uint retlen = doreply(len, buf, buf2);
				if(retlen > 0) {
					serialwrite(buf2, retlen);
				}
			} else {
				Serial.print("!!!chksum: ");
				Serial.println(sum, HEX);
			}
			status = -1;
		}
	} else {
		status = -1;
	}
}

void loop() {
	static ulong lastrun = 0;
	ulong newrun = millis();
	if(newrun - lastrun > 50) {
		Serial.print("Lagging: ");
		Serial.println(newrun - lastrun);
	}
	lastrun = newrun;
	int ret1;
	ret1 = Serial1.read();
	if(ret1 != -1) {
		processserial((uchar)ret1);
	} else {
		processsendbuf();
	}
	//mydelay(5);
}
