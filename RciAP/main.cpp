#include <Arduino.h>

#define WRITERET(x, b, l) l = sizeof(x);memcpy(b,x,l);
#define R(x) retdata ## x

void doreply(int len, char recv[], char buf[]) {
	const unsigned char
		retdata0005[] = {0,2,0,5},
		retdata0007[] = "\0\x08MYBT1",
		retdata0009[] = {0,0x0a,3,1,1},
		retdata000B[] = "\0\x0cMYBT1ABCDEFG",
		retdata000D[] = "\0\x0e\0\x03\0\0MYBT1",
		retdata000F[] = {0,0x10,0,1,1},
		retdata0013[] = {0,2,0,0x13};
	int retlen, i, j;
	buf[0] = 0xff;
	buf[1] = 0x55;
	if(recv[0] == 0) {
		switch(recv[1]) {
			case 0x05: // switch to remote ui
				WRITERET(R(0005),buf+3, retlen);
				break;
			case 0x07: // get ipod name
				WRITERET(R(0007),buf+3, retlen);
				break;
			case 0x09: // get ipod version
				WRITERET(R(0009),buf+3, retlen);
				break;
			case 0x0B: // get serial
				WRITERET(R(000B),buf+3, retlen);
				break;
			case 0x0D: // Get Device Type
				WRITERET(R(000D),buf+3, retlen);
				break;
			case 0x0F: // set lingo protocol version
				WRITERET(R(000F),buf+3, retlen);
				break;
			case 0x13: // Get Device Lingoes
				WRITERET(R(0013),buf+3, retlen);
				break;
		}
	} else if (recv[0] == 4) {

	}

	buf[2] = retlen;
	for(i = 2, j = 0; i < retlen + 2; i++) {
		j += buf[i];
	}
	buf[retlen + 3] = 0 - j;
}

void setup() {
	Serial.begin(19200);
}

void processserial(char input1) {
	static char buf[256];
	static char status = 0;

	if(input1 == 0);
}

void loop() {
	char ret1;
	while(Serial.available()) {
		ret1 = Serial.read();
		processserial(ret1);
	}
}
