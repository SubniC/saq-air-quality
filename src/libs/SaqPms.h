#ifndef _SAQPMS_H_
#define _SAQPMS_H_

#include "application.h"
#include "utils/ema_f.h"
#include "pmsst.h"

typedef enum {
	PMS5003=0,
	PMS5003ST,
	PMS7003,
} SaqPmsModel_t;

typedef enum : uint8_t {
	PM1dot0=0,
	PM2dot5,
	PM10dot0,
	HCHO,
	Temperature,
	Humidity,
} SaqPmsDataField_t;

class SaqPMS {

public:
	enum class DutyState : uint8_t { ACTIVE, WARMUP, SLEEPING };

    SaqPMS(uint8_t pin, uint8_t average_factor, SaqPmsModel_t model);
	bool begin(void);
	void end(void);
    void enable(void);
    void disable(void);
    bool is_enabled(void);

    PmsSensor::PmsStatus loop(void);

	//Esta funcion devuelve LA MEDIA actual
    float getCurrentValue(SaqPmsDataField_t);
	//Esta funcion devuelve el ultimo valor obtenido del sensor
    float getLastValue(SaqPmsDataField_t);

	// Duty-cycle control
	void configureDutyCycle(uint8_t sleep_min, uint8_t wake_sec);
	DutyState getDutyState() const { return _duty_state; }
	uint8_t getSleepMin() const { return _sleep_min; }
	uint8_t getWakeSec() const { return _wake_sec; }

	// Callback on state change: (new_state_string)
	void setStateCallback(void (*cb)(const char*)) { _state_cb = cb; }

	static const char* stateToStr(DutyState s) {
		switch(s) {
			case DutyState::ACTIVE:   return "active";
			case DutyState::WARMUP:   return "warmup";
			case DutyState::SLEEPING: return "sleeping";
		}
		return "unknown";
	}

	uint8_t getSize() const { return _average_factor; }
	SaqPmsModel_t getCurrentModel(void) const { return _pms_model; }

private:
	static constexpr uint8_t MAX_CHANNELS = 6; // PMS5003ST: PM1,PM2.5,PM10,HCHO,Temp,Hum
	const uint8_t _enable_pin;
	const uint8_t _average_factor;
	const SaqPmsModel_t _pms_model;
	bool _is_enabled;
	bool _is_init;
	EmaF _medias[MAX_CHANNELS];
	float _lastvalues[MAX_CHANNELS] = {};
	PmsSensor _pms_sensor;
	uint8_t _count_medias;

	// Duty-cycle state
	DutyState _duty_state = DutyState::ACTIVE;
	uint32_t  _duty_ts    = 0;
	uint8_t   _sleep_min  = 0;       // 0 = sin cycling (continuo)
	uint8_t   _wake_sec   = 40;

	void (*_state_cb)(const char*) = nullptr;
	void _enterSleep();
	void _enterWarmup();
	void _enterActive();
};

#endif
