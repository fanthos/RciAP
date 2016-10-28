#include <Arduino.h>

#define WRITERET(x, b, l) l += sizeof(x);memcpy(b,x,l);
#define R(x) retdata ## x
#define RL(x) sizeof(retdata ## x)

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

inline ulong uc2ul(uchar a, uchar b, uchar c, uchar d) {
	ulong t = a;
	t = t << 8 | b;
	t = t << 8 | c;
	t = t << 8 | d;
	return t;
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
		t = lastsent - newsent;
		if(t > IAP_MAXWAIT) {
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

uchar sendbuf_data[140] = {0xff, 0x55};
uchar *sendbuf = sendbuf_data + 3;
uint sendbuflen;
uchar sendbufready = 0;

void processsendbuf() {
	if(sendbufready == 0)return;
	sendbuf_data[2] = (uchar)sendbuflen;
	sendbuf_data[sendbuflen + 3] = calcsum((uchar*)sendbuf_data + 2, sendbuflen + 1);
	serialwrite((uchar *)sendbuf_data, sendbuflen + 4);
	sendbufready = 0;
}

uint doreply(cuint len, cuchar recv[]) {
	cuchar
		//retdata00ack[] = {0,0},
		retdata_0007[] = "MYBT1",
		retdata_0009[] = {8,1,1},
		retdata_000B[] = "MYBT1ABCDEFG",
		retdata_000F[] = {0,1,15},
		retdata_04ack[] = {0,0,0},
		retdata_04ackunk[] = {5,0,0},
		retdata_str[] = "BTAUDIO",
		//retdata04_0[] = {0},
		retdata_04str[] = "\0\0\0\0MY888",
		retdata_00[] = {0,0,0,0},
		retdata_01[] = {0,0,0,1},
		retdata_02[] = {0,0,0,2},
		retdata_03[] = {0,0,0,3},
		retdata_0101[] = {0,0,0,1,0,0,0,1},
		retdata_041C[] = {0,0,0xea,0x60,0,0,0x27,0x10,1};
	cuchar retdata_ary0028[] = {1,4,4,4,5,6,7};
	cuchar retdata_buf0013[] = {0x00, 0x27, 0x00};
	cuchar retdata_d0027[] = {1, 0, 0, 0, 1};
	uint retlen;

	if(recv[0] == 0 && len > 1) {
		uchar *rbuf = sendbuf + 2;
		sendbuf[0] = 0;
		sendbuf[1] = recv[1] + 1;
		sendbuflen = 0;
		const uint headersize = 2;
		switch(recv[1]) {
			case 0x01: // Identify
			case 0x02: // !ACK
				return 0;
			case 0x03: // RequesetRemoteUIMode
				rbuf[0] = 1;
				sendbuflen = headersize + 1;
				break;
			case 0x05: // switch remote ui
			case 0x06: // Exit remote ui
				rbuf[0] = 0;
				rbuf[1] = recv[1];
				sendbuf[1] = 2;
				sendbuflen = headersize + 2;
				break;
			case 0x07: // get ipod name
				sendbuflen = headersize;
				WRITERET(R(_0007),rbuf, sendbuflen);
				break;
			case 0x09: // get ipod version
				sendbuflen = headersize;
				WRITERET(R(_0009),rbuf, sendbuflen);
				break;
			case 0x0B: // get serial
				sendbuflen = headersize;
				WRITERET(R(_000B),rbuf, sendbuflen);
				break;
			case 0x0D: // Get Device Type
				sendbuflen = headersize;
				WRITERET(R(_04str),rbuf, sendbuflen);
				rbuf[1] = 3;
				break;
			case 0x0F: // req lingo protocol version
				sendbuflen = headersize;
				WRITERET(R(_000F),rbuf, sendbuflen);
				rbuf[0] = recv[2];
				break;
			case 0x13: // Identify Device Lingoes
				sendbuf[0] = 0;
				sendbuf[1] = 2;
				sendbuf[2] = 0;
				sendbuf[3] = 0x13;
				sendbuflen = headersize + 2;
				sendbufready = 1;
				processsendbuf();
				memcpy((void*)sendbuf, R(_buf0013), RL(_buf0013));
				sendbuflen = headersize + 1;
				sendbufready = 1;
				break;
			case 0x28: // Device RetAccessoryInfo
				rbuf[0] = R(_ary0028)[recv[2]];
				if(rbuf[0] > RL(_ary0028)){
					return 0;
				}
				sendbuflen = headersize + 1;
				sendbuf[1] = 0x27;
				break;
			default:
				rbuf[0] = 5;
				rbuf[1] = recv[1];
				sendbuf[1] = 2;
				sendbuflen = headersize + 2;
				procunkcmd(len, recv, "!!!Unk: ");
				break;
		}
	} else if (recv[0] == 4 && recv[1] == 0 && len > 2) {
		uchar *rbuf = sendbuf + 3;
		sendbuf[0] = 4;
		sendbuf[1] = 0;
		sendbuf[2] = recv[2] + 1;
		const uint headersize = 3;
		switch(recv[2]) {
			uchar action;
			case 0x04: // Set current chapter
			case 0x0B: // Set audiobook speed
			case 0x16: // Rest db selection
			case 0x17: // Select db record
			case 0x26: // Set PlayStatusChange notification
			case 0x28: // Play Current Selection
			case 0x2E: // Set Shuffle
			case 0x31: // Set Repeat
			case 0x38: // SelectSortDbRecord
				sendbuflen = headersize;
				WRITERET(R(_04ack),rbuf, sendbuflen);
				rbuf[2] = recv[2];
				sendbuf[2] = 1;
				break;
			case 0x02:  // Get current playing info
				sendbuflen = headersize;
				WRITERET(R(_0101),rbuf, sendbuflen);
				break;
			case 0x05: // Get current play status
				sendbuflen = headersize;
				WRITERET(R(_02),rbuf, sendbuflen);
				break;
			case 0x07: // Get chapter name
			case 0x20: // Get Track Title
			case 0x22: // Get Artist Name
			case 0x24: // Get Albun name
				sendbuflen = headersize;
				WRITERET(R(_str),rbuf, sendbuflen);
				break;
			case 0x09: // Get audiobook speed
			case 0x2C: // Get Shuffle
			case 0x2F: // Get Repeat
				sendbuflen = headersize + 1;
				rbuf[0] = 0;
				break;
			case 0x18: // Get num of categorized db records
				sendbuflen = headersize;
				WRITERET(R(_03),rbuf, sendbuflen);
				break;
			case 0x1C: // Get PlayStatus
				sendbuflen = headersize;
				WRITERET(R(_041C),rbuf, sendbuflen);
				break;
			case 0x1E: // Get current playing track index
				sendbuflen = headersize;
				WRITERET(R(_01),rbuf, sendbuflen);
				break;
			case 0x35: // Get num playing tracks
				sendbuflen = headersize;
				WRITERET(R(_03),rbuf, sendbuflen);
				break;
			case 0x1A: // Retrieve categorized db records
				{
					ulong id_s, id_e;
					id_e = uc2ul(recv[7], recv[8], recv[9], recv[10]);
					if(id_e == 0xffffffff) {
						id_s = 0;
						id_e = 4;
					} else {
						id_s = uc2ul(recv[3], recv[4], recv[5], recv[6]);
						id_e = id_s + id_e;
						if(id_s > 3)id_s = 3;
						if(id_e > 4)id_e = 4;
					}
					memcpy(rbuf+4, R(_str), RL(_str));
					sendbuflen = headersize;
					sendbuflen += RL(_str) + 4;
					while(id_s < id_e) {
						if(sendbufready == 1) {
							processsendbuf();
						}
						rbuf[0] = id_s >> 24;
						rbuf[1] = id_s >> 16;
						rbuf[2] = id_s >> 8;
						rbuf[3] = id_s;
						sendbufready = 1;
						id_s ++;
					}
					return 0;
				}
			case 0x29: // Play control
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
				sendbuflen = headersize;
				WRITERET(R(_04ack),rbuf, sendbuflen);
				rbuf[2] = recv[2];
				sendbuf[2] = 1;
				sendbufready = 1;

				processsendbuf();
				sendbuf[2] = 0x27;
				sendbuflen = headersize;
				WRITERET(R(_d0027),rbuf, sendbuflen);
				sendbufready = 1;

				break;
			case 0x37: // Set play track
				sendbuflen = headersize;
				WRITERET(R(_04ack),rbuf, sendbuflen);
				rbuf[2] = recv[2];
				sendbuf[2] = 1;
				sendbufready = 1;
				processsendbuf();

				sendbuf[2] = 0x27;
				sendbuflen = headersize;
				WRITERET(R(_d0027),rbuf, sendbuflen);
				sendbufready = 1;
				break;
			default:
				sendbuflen = headersize;
				WRITERET(R(_04ackunk),rbuf, sendbuflen);
				rbuf[2] = recv[2];
				sendbuf[2] = 1;
				procunkcmd(len, recv, "!!!Unk: ");
				break;
		}
	} else {
		procunkcmd(len, recv, "!!!Unk: ");
		return 0;
	}

	if(sendbuflen > 0) {
		sendbufready = 1;
	}
	return retlen;
}

void setup() {
	Serial.begin(19200);
	Serial1.begin(IAP_BAUDRATE);
}

void processserial(cuchar input1) {
	static uchar _buf[258];
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
				doreply(len, buf);
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
	if(sendbufready) {
		processsendbuf();
	} else {
		int ret1;
		ret1 = Serial1.read();
		if(ret1 != -1) {
			processserial((uchar)ret1);
		}
	}
	//mydelay(5);
}
