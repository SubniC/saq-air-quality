
#include "pmsst.h"

////////////////////////////////////////

// #if defined NOMINMAX
//
// #if defined min
// #undef min
// #endif
// template <class T>
// inline const T& __attribute__((always_inline)) min(const T& a, const T& b) {
// 	return !(b < a) ? a : b;
// }
//
// #endif

////////////////////////////////////////

inline void __attribute__((always_inline)) swapEndianBig16(uint16_t *x) {
	constexpr union {
		// endian.test16 == 0x0001 for low endian
		// endian.test16 == 0x0100 for big endian
		// should be properly optimized by compiler
		uint16_t test16;
		uint8_t test8[2];
	} endian = { .test8 = { 1,0 } };

	if (endian.test16 != 0x0100) {
		uint8_t hi = (*x & 0xff00) >> 8;
		uint8_t lo = (*x & 0xff);
		*x = lo << 8 | hi;
	}
}

////////////////////////////////////////

void sumBuffer(uint16_t *sum, const uint8_t *buffer, uint16_t cnt) {
	for (; cnt > 0; --cnt, ++buffer) {
		*sum += *buffer;
	}
}

inline void sumBuffer(uint16_t *sum, const uint16_t data) {
	*sum += (data & 0xFF) + (data >> 8);
}

////////////////////////////////////////

void PmsSensor::setTimeout(const decltype(timeout) timeout) {
	Serial1.setTimeout(timeout);
	this->timeout = timeout;
};

decltype(PmsSensor::timeout) PmsSensor::getTimeout(void) const {
	return timeout;
};


PmsSensor::PmsSensor() : passive(jb::logic::tribool(jb::logic::unknown)), sleep(jb::logic::tribool(jb::logic::unknown)) {
#if defined PMS_DYNAMIC
	begin();
#endif
};


bool PmsSensor::begin(void) {
	Serial1.setTimeout(PmsSensor::timeoutPassive);
	//En photon esto devuelve VOID no BOOL
	Serial1.begin(9600);
	// if (!Serial1.begin(9600)) {
	// 	return false;
	// }
	return true;
};

void PmsSensor::end(void) {
	Serial1.end();
};

size_t PmsSensor::available(void) {
	while (Serial1.available()) {
		if (Serial1.peek() != sig[0]) {
			Serial1.read();
		} else {
			break;
		}
	}
	return static_cast<size_t>(Serial1.available());
}

PmsSensor::PmsStatus PmsSensor::read(pmsData *data, const size_t nData, const uint8_t dataSize) {

	if (available() < (dataSize + 2) * sizeof(pmsData) + sizeof(sig)) {
		return noData;
	}

	Serial1.read(); // Value is equal to sig[0]. There is no need to check the value, it was checked by prior peek()

	if (Serial1.read() != sig[1]) // The rest of the buffer will be invalidated during the next read attempt
		return readError;

	uint16_t sum{ 0 };
	sumBuffer(&sum, (uint8_t *)&sig, sizeof(sig));

	pmsData thisFrameLen{ 0x1c };
	if (Serial1.readBytes((char*)&thisFrameLen, sizeof(thisFrameLen)) != sizeof(thisFrameLen)) {
		return readError;
	};

	if (thisFrameLen % 2 != 0) {
		return frameLenMismatch;
	}
	sumBuffer(&sum, thisFrameLen);

	const decltype(thisFrameLen) maxFrameLen{ 2 * 0x1c };    // arbitrary

	swapEndianBig16(&thisFrameLen);
	if (thisFrameLen > maxFrameLen) {
		return frameLenMismatch;
	}

	size_t toRead{
	min(
		static_cast<size_t>(thisFrameLen - 2),
		static_cast<size_t>(nData) * sizeof(pmsData)
	)
	};
	if (data == nullptr) {
		toRead = 0;
	}

	if (toRead) {
		if (Serial1.readBytes((char*)data, toRead) != toRead) {
			return readError;
		}
		sumBuffer(&sum, (uint8_t*)data, toRead);

		for (size_t i = 0; i < nData; ++i) {
			swapEndianBig16(&data[i]);
		}
	}

	pmsData crc;
	for (; toRead < static_cast<size_t>(thisFrameLen); toRead += 2) {
		if (Serial1.readBytes((char*)&crc, sizeof(crc)) != sizeof(crc)) {
			return readError;
		};

if (toRead < static_cast<size_t>(thisFrameLen - 2))
			sumBuffer(&sum, crc);
	}

	swapEndianBig16(&crc);

	if (sum != crc) {
		return sumError;
	}

	return OK;
}

void PmsSensor::flushInput(void) {
	//Esto es de la libreria AltSoftSerial
	//Serial1.flushInput();
	//Para photon:
	while(Serial1.available()>0) Serial1.read();
}

bool PmsSensor::waitForData(const unsigned int maxTime, const size_t nData) {
	const auto t0 = millis();
	if (nData == 0) {
		for (; (millis() - t0) < maxTime; delay(1)) {
			if (Serial1.available()) {
				return true;
			}
		}
		return Serial1.available();
	}

	for (; (millis() - t0) < maxTime; delay(1)) {
		if (available() >= nData) {
			return true;
		}
	}
	return available() >= nData;
}

bool PmsSensor::write(const PmsCmd cmd) {
	static_assert(sizeof(cmd) >= 3, "Wrong definition of PmsCmd (too short)");

	if ((cmd != cmdReadData) && (cmd != cmdWakeup)) {
		flushInput();
	}

	if (Serial1.write(sig, sizeof(sig)) != sizeof(sig)) {
		return false;
	}
	const size_t cmdSize = 3;
	if (Serial1.write((uint8_t*)&cmd, cmdSize) != cmdSize) {
		return false;
	}

	uint16_t sum{ 0 };
	sumBuffer(&sum, sig, sizeof(sig));
	sumBuffer(&sum, (uint8_t*)&cmd, cmdSize);
	swapEndianBig16(&sum);
	if (Serial1.write((uint8_t*)&sum, sizeof(sum)) != sizeof(sum)) {
		return false;
	}

	switch (cmd) {
		case cmdModePassive:
			passive = jb::logic::tribool(true);
			break;
		case cmdModeActive:
			passive = jb::logic::tribool(false);
			break;
		case cmdSleep:
			sleep = jb::logic::tribool(true);
			break;
		case cmdWakeup:
			sleep = jb::logic::tribool(false);
			passive = jb::logic::tribool(false);
			// waitForData(wakeupTime);
			break;
		default:
			break;
	}
	if ((cmd != cmdReadData) && (cmd != cmdWakeup)) {
		const auto responseFrameSize = 8;
		if (!waitForData(ackTimeout, responseFrameSize)) {
			while(Serial1.available()>0) Serial1.read();
			return true;
		}
		PmsSensor::pmsData response = 0xCCCC;
		read(&response, 1, 1);
	}

	/*
		if ((cmd != cmdReadData) && (cmd != cmdWakeup)) {
			const auto responseFrameSize = 8;
			if (!waitForData(ackTimeout, responseFrameSize)) {
				Serial1.flushInput();
				return false;
			}
			PmsSensor::pmsData response = 0xCCCC;
			if (read(&response, 1, 1) != OK) {
				return false;
			}
			if ((response >> 8) != (cmd & 0xFF)) {
				return false;
			}
		}
	*/

	return true;
}

const char *PmsSensor::getMetrics(const pmsIdx idx) {
	return idx < nValues_PmsDataNames ? PmsSensor::metrics[idx] : "???";
}

const char *PmsSensor::getDataNames(const pmsIdx idx) {
	return idx < nValues_PmsDataNames ? PmsSensor::dataNames[idx] : "???";
}

const char * PmsSensor::errorMsg[nValues_PmsStatus]{
	"OK",
	"noData",
	"readError",
	"frameLenMismatch",
	"sumError"
};

const char *PmsSensor::metrics[]{
	"mcg/m3",
	"mcg/m3",
	"mcg/m3",

	"mcg/m3",
	"mcg/m3",
	"mcg/m3",

	"/0.1L",
	"/0.1L",
	"/0.1L",
	"/0.1L",
	"/0.1L",
	"/0.1L",

	"mg/m3",
	"ºC",
	"%",

	"???"
};

const char *PmsSensor::dataNames[]{
	"PM1.0, CF=1",
	"PM2.5, CF=1",
	"PM10.  CF=1",
	"PM1.0",
	"PM2.5",
	"PM10.",

	"Particles < 0.3 micron",
	"Particles < 0.5 micron",
	"Particles < 1.0 micron",
	"Particles < 2.5 micron",
	"Particles < 5.0 micron",
	"Particles < 10. micron",

	"Formaldehyde",
	"Temperature",
	"Humidity",

	"Reserved_0"
};
