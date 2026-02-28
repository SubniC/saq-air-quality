#include "SaqPms.h"
#include "config/config.h"
#include "utils/debug.h"
#include <math.h>

SaqPMS::SaqPMS(uint8_t enable_pin, uint8_t average_factor, SaqPmsModel_t model) :
_enable_pin(enable_pin),
_average_factor(average_factor),
_pms_model(model), _is_enabled(true),
_is_init(false) {
	// Inicializamos las EMAs con el factor de ventana
	for (uint8_t i = 0; i < MAX_CHANNELS; ++i) {
		_medias[i] = EmaF(_average_factor);
	}
};

bool SaqPMS::begin(void) {

    if(_is_init == true) { return true; }

    //En funcion del modelo que usamos tenemos mas o menos datos
    //Asi que adaptamos la cantidad de medias que usara la clase
    switch(_pms_model)
    {
        case SaqPmsModel_t::PMS5003ST:
            _count_medias = 6; //particulas, temepratura, humedad y formaldehido
            break;
        default:
            _count_medias = 3; //Solo particulas
    }

    //Configuramos el pin de enabled y dejamos habilitado
    pinMode(_enable_pin,OUTPUT);
    digitalWrite(_enable_pin, HIGH);

    //Iniciamos el objeto del sensor
    _pms_sensor.begin();
    _pms_sensor.waitForData(PmsSensor::wakeupTime);
    _pms_sensor.write(PmsSensor::cmdModeActive);

    _is_init = true;
    _duty_ts = millis();
    _duty_state = DutyState::ACTIVE;
	return true;
};

void SaqPMS::end(void) {
    _pms_sensor.end();
    _is_init = false;
};

void SaqPMS::enable(void) {
    digitalWrite(_enable_pin, HIGH);
    _is_enabled = true;
};

void SaqPMS::disable(void) {
    digitalWrite(_enable_pin, LOW);
    _is_enabled = false;
};

bool SaqPMS::is_enabled(void) {
 return _is_enabled;
};

float SaqPMS::getCurrentValue(SaqPmsDataField_t type)
{
    if(!_is_enabled) {return NAN;}

    if((uint8_t)type < _count_medias)
    {
        return _medias[(uint8_t)type].get();
    }
    return NAN;
}

float SaqPMS::getLastValue(SaqPmsDataField_t type)
{
    if(!_is_enabled) {return NAN;}

    if((uint8_t)type < _count_medias)
    {
        return _lastvalues[(uint8_t)type];
    }
    return NAN;
}

void SaqPMS::configureDutyCycle(uint8_t sleep_min, uint8_t wake_sec)
{
    _sleep_min = sleep_min;
    _wake_sec  = (wake_sec < PMS_WARMUP_SEC + 5) ? (PMS_WARMUP_SEC + 10) : wake_sec;

    if (_sleep_min == 0) {
        // Modo continuo: si estaba dormido, despertar
        if (_duty_state == DutyState::SLEEPING) {
            _enterWarmup();
        }
    }
    LOG_INFO("PMS duty-cycle: sleep=%umin wake=%us", (unsigned)_sleep_min, (unsigned)_wake_sec);
}

void SaqPMS::_enterSleep()
{
    _pms_sensor.write(PmsSensor::cmdSleep);
    digitalWrite(_enable_pin, LOW);
    _duty_state = DutyState::SLEEPING;
    _duty_ts = millis();
    LOG_INFO("PMS -> SLEEP (%umin)", (unsigned)_sleep_min);
    if (_state_cb) _state_cb(stateToStr(_duty_state));
}

void SaqPMS::_enterWarmup()
{
    digitalWrite(_enable_pin, HIGH);
    _pms_sensor.write(PmsSensor::cmdWakeup);
    _pms_sensor.write(PmsSensor::cmdModeActive);
    _duty_state = DutyState::WARMUP;
    _duty_ts = millis();
    LOG_INFO("PMS -> WARMUP (%us)", (unsigned)PMS_WARMUP_SEC);
    if (_state_cb) _state_cb(stateToStr(_duty_state));
}

void SaqPMS::_enterActive()
{
    _duty_state = DutyState::ACTIVE;
    _duty_ts = millis();
    LOG_INFO("PMS -> ACTIVE (%us)", (unsigned)_wake_sec);
    if (_state_cb) _state_cb(stateToStr(_duty_state));
}

PmsSensor::PmsStatus SaqPMS::loop()
{
    if(!_is_enabled) {return PmsSensor::noData;}

    const uint32_t now = millis();
    const uint32_t elapsed = now - _duty_ts;

    // --- Máquina de estados duty-cycle ---
    if (_sleep_min > 0) {
        switch (_duty_state) {
            case DutyState::SLEEPING:
                if (elapsed >= (uint32_t)_sleep_min * 60000UL)
                    _enterWarmup();
                return PmsSensor::noData;

            case DutyState::WARMUP:
            {
                if (elapsed >= (uint32_t)PMS_WARMUP_SEC * 1000UL)
                    _enterActive();
                // Durante warmup leemos serial para vaciar buffer pero no alimentamos EMAs
                PmsSensor::pmsData data[PmsSensor::Reserved];
                _pms_sensor.read(data, PmsSensor::Reserved);
                return PmsSensor::noData;
            }

            case DutyState::ACTIVE:
                // Tiempo de lectura activa agotado -> dormir
                if (elapsed >= (uint32_t)_wake_sec * 1000UL)
                {
                    _enterSleep();
                    return PmsSensor::noData;
                }
                break; // continuar con lectura normal
        }
    }

    // --- Lectura normal (ACTIVE o modo continuo) ---
    const auto n = PmsSensor::Reserved;
    PmsSensor::pmsData data[n];
    PmsSensor::PmsStatus status = _pms_sensor.read(data, n);

    switch (status) {
        case PmsSensor::OK:
        {
            // Validación: rechazar PM2.5 absurdo
            if (data[PmsSensor::PM2dot5] > PMS_PM25_MAX_UGM3) {
                LOG_WARN("PMS PM2.5 rejected: %u", (unsigned)data[PmsSensor::PM2dot5]);
                return PmsSensor::readError;
            }

            _lastvalues[SaqPmsDataField_t::PM1dot0] = data[PmsSensor::PM1dot0];
			_lastvalues[SaqPmsDataField_t::PM2dot5] = data[PmsSensor::PM2dot5];
			_lastvalues[SaqPmsDataField_t::PM10dot0] = data[PmsSensor::PM10dot0];

            _medias[SaqPmsDataField_t::PM1dot0].add(data[PmsSensor::PM1dot0]);
			_medias[SaqPmsDataField_t::PM2dot5].add(data[PmsSensor::PM2dot5]);
			_medias[SaqPmsDataField_t::PM10dot0].add(data[PmsSensor::PM10dot0]);

            if(_pms_model == SaqPmsModel_t::PMS5003ST)
            {
                _lastvalues[SaqPmsDataField_t::HCHO] = data[PmsSensor::HCHO];
                _lastvalues[SaqPmsDataField_t::Temperature] = data[PmsSensor::Temperature]/10.0f;
                _lastvalues[SaqPmsDataField_t::Humidity] = data[PmsSensor::Humidity]/10.0f;

                _medias[SaqPmsDataField_t::HCHO].add(data[PmsSensor::HCHO]);
                _medias[SaqPmsDataField_t::Temperature].add(data[PmsSensor::Temperature]/10.0f);
                _medias[SaqPmsDataField_t::Humidity].add(data[PmsSensor::Humidity]/10.0f);
            }
        }
    }
    return status;
}
