#ifndef _MODULESENSORS_H_
#define _MODULESENSORS_H_

#include "application.h"
#include "config/config.h"
#include <Adafruit_Sensor.h>

#if defined(TEMP_SENSOR_BMP280)
  #include <Adafruit_BMP280.h>
#elif defined(TEMP_SENSOR_BME280)
  #include <Adafruit_BME280.h>
#endif

#include <Adafruit_CCS811.h>
#include "utils/ema_f.h"
#ifdef ENABLE_LUX_SENSOR
  #include "BH1750Lib.h"
#endif
#ifdef HAS_PARTICLE_DATA
  #include "libs/SaqPms.h"
#endif
#ifdef ENABLE_AQI
  #include "libs/aqi/AqiEngine.h"
  using namespace AQI;
#endif

#if defined(TEMP_SENSOR_BMP280)
  extern Adafruit_BMP280 bme280; // I2C 0x76
#elif defined(TEMP_SENSOR_BME280)
  extern Adafruit_BME280 bme280; // I2C 0x76
#endif

#ifdef HAS_PARTICLE_DATA
extern SaqPMS pms;
#endif
#ifdef ENABLE_AQI
extern AqiEngine* g_aqi;
#endif

#ifdef ENABLE_LUX_SENSOR
extern BH1750Lib bh1750; // Sensor de lux
#endif

extern Adafruit_CCS811 ccs811;

// Medias de sensores (EMA float — reemplaza RunningAverage)
extern EmaF temperature_avg;
extern EmaF humidity_avg;
extern EmaF pressure_avg;
extern EmaF lux_avg;
extern EmaF co2_avg;
extern EmaF tvoc_avg;

extern volatile bool ccs811_measure_ready;



namespace SENSORS {
  void  begin();
  bool  read_temperature_sensor();
  #ifdef ENABLE_LUX_SENSOR
  bool  read_lux_sensor();
  #endif
  bool  read_gas_sensor();
  #ifdef HAS_PARTICLE_DATA
  bool  read_pms_sensor();
  #endif

  float get_current_humidity();
  float get_current_temperature();
  void  on_gas_update_active_isr();

  // ===== Getters AQI “último consolidado” por bucket =====
  #ifdef ENABLE_AQI
  bool get_aqi(Pollutant p, Scope s, float& conc, int16_t& aqi);

  inline bool get_pm25_minute(float& conc, int16_t& aqi)   { return get_aqi(Pollutant::PM25, Scope::Minute,  conc, aqi); }
  inline bool get_pm25_hour  (float& conc, int16_t& aqi)   { return get_aqi(Pollutant::PM25, Scope::Hour,    conc, aqi); }
  inline bool get_pm25_day   (float& conc, int16_t& aqi)   { return get_aqi(Pollutant::PM25, Scope::Day,     conc, aqi); }
  inline bool get_pm25_now   (float& conc, int16_t& aqi)   { return get_aqi(Pollutant::PM25, Scope::NowCast, conc, aqi); }

  inline bool get_pm10_minute(float& conc, int16_t& aqi)   { return get_aqi(Pollutant::PM10, Scope::Minute,  conc, aqi); }
  inline bool get_pm10_hour  (float& conc, int16_t& aqi)   { return get_aqi(Pollutant::PM10, Scope::Hour,    conc, aqi); }
  inline bool get_pm10_day   (float& conc, int16_t& aqi)   { return get_aqi(Pollutant::PM10, Scope::Day,     conc, aqi); }
  inline bool get_pm10_now   (float& conc, int16_t& aqi)   { return get_aqi(Pollutant::PM10, Scope::NowCast, conc, aqi); }
  #endif
}

#endif
