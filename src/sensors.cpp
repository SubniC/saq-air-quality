#include "sensors.h"
#include "comms.h"
#include "utils/debug.h"
#include "utils/sensor_helpers.h"
#include "persistence.h"
#include "libs/json.h"
#include "utils/sensor_fusion.h"

#if defined(TEMP_SENSOR_BMP280)
Adafruit_BMP280 bme280;
#elif defined(TEMP_SENSOR_BME280)
Adafruit_BME280 bme280;
#endif

#ifdef ENABLE_LUX_SENSOR
BH1750Lib bh1750;
#endif

Adafruit_CCS811 ccs811;

EmaF temperature_avg(TEMPERATURE_AVG_FACTOR);
EmaF humidity_avg(HUMIDITY_AVG_FACTOR);
EmaF pressure_avg(PRESSURE_AVG_FACTOR);
EmaF lux_avg(LUX_AVG_FACTOR);
EmaF co2_avg(GAS_AVG_FACTOR);
EmaF tvoc_avg(GAS_AVG_FACTOR);

volatile bool ccs811_measure_ready = false;

#ifdef HAS_PARTICLE_DATA
constexpr SaqPmsModel_t kParticleModel =
#if defined(PARTICLE_SENSOR_MODEL_PMS5003ST)
	SaqPmsModel_t::PMS5003ST;
#elif defined(PARTICLE_SENSOR_MODEL_PMS7003)
	SaqPmsModel_t::PMS7003;
#else
	SaqPmsModel_t::PMS5003;
#endif

SaqPMS pms(PMS_ENABLE_PIN, PARTICLE_AVG_FACTOR, kParticleModel);
#endif

#ifdef ENABLE_AQI
AqiEngine *g_aqi = nullptr;
#endif

namespace
{
#if defined(HAS_HUMIDITY_DATA)
	// Humedad fusionada -> devuelve true si 'out' es válido
	bool _retrieve_fused_humidity(float &out)
	{
		FUSE::SensorMeasurement src[2];
		size_t n = 0;

#if defined(TEMP_SENSOR_BME280)
		// BME280
		src[n++] = FUSE::SensorMeasurement{bme280.readHumidity(), 0.5f};
#endif

#if defined(ENABLE_PARTICLE_SENSOR) && defined(PARTICLE_SENSOR_MODEL_PMS5003ST)
		src[n++] = FUSE::SensorMeasurement{pms.getCurrentValue(SaqPmsDataField_t::Humidity), 0.5f};
#endif

		float fused = NAN;
		if (!FUSE::weighted_mean(src, n, &fused))
			return false;

		// No aplicamos offsets aquí! eso lo haremos aguas arriba
		out = fused;
		return true;
	}
#endif

	// Temperatura fusionada -> devuelve true si 'out' es válido
	bool _retrieve_fused_temperature(float &out)
	{
		FUSE::SensorMeasurement src[2];
		size_t n = 0;

		// BME280
		src[n++] = FUSE::SensorMeasurement{bme280.readTemperature(), 0.5f};

		// PMS5003ST (si está presente)
#if defined(ENABLE_PARTICLE_SENSOR) && defined(PARTICLE_SENSOR_MODEL_PMS5003ST)
		src[n++] = FUSE::SensorMeasurement{pms.getCurrentValue(SaqPmsDataField_t::Temperature), 0.5f};
#endif

		float fused = NAN;
		if (!FUSE::weighted_mean(src, n, &fused))
			return false;

		// No aplicamos offsets aquí! eso lo haremos aguas arriba
		out = fused;
		return true;
	}

#ifdef ENABLE_AQI
	// Callback del motor AQI -> delega en COMMS
	void aqi_event_cb(void *ctx, const AQI::Event &ev)
	{
		(void)ctx;
		COMMS::publish_aqi_event(ev); // JSON + MQTT
	}
#endif
}

namespace SENSORS
{

	void begin()
	{
		pinMode(CJMCU811_INT_PIN, INPUT);

#ifdef CJMCU811_WAKE_PIN
		// Configuramos y activamos el componente si procede
		pinMode(CJMCU811_WAKE_PIN, OUTPUT);
		digitalWrite(CJMCU811_WAKE_PIN, LOW);
#endif

		if (ccs811.begin())
		{
			// Configuramos el sensor
			ccs811.enableInterrupt();
			ccs811.setDriveMode(CCS811_DRIVE_MODE_10SEC);
			// Las medias de este las dejamos a 0
			attachInterrupt(CJMCU811_INT_PIN, SENSORS::on_gas_update_active_isr, FALLING);
		}
		else
		{
			LOG_ERROR("cant init CCS811");
		}

		if (bme280.begin(BME280_I2C_ADDRESS))
		{
#ifdef TEMP_SENSOR_BME280
			bme280.setSampling(Adafruit_BME280::MODE_FORCED,
							   Adafruit_BME280::SAMPLING_X1, // temperature
							   Adafruit_BME280::SAMPLING_X1, // pressure
							   Adafruit_BME280::SAMPLING_X1, // humidity
							   Adafruit_BME280::FILTER_OFF);
#endif
		}
		else
		{
			LOG_ERROR("cant init BME280");
			delay(100);
		}

#ifdef HAS_PARTICLE_DATA
		pms.begin();
#endif

#ifdef ENABLE_AQI
		// Config del motor AQI alineada a tu reloj local
		AQI::EngineConfig aqi_cfg;
		aqi_cfg.tzOffsetSec = (int32_t)lroundf(SYSTEM_TIMEZONE * 3600.0f);

		static AQI::AqiEngine engine(aqi_cfg);
		g_aqi = &engine;

		engine.setCallback(&aqi_event_cb, nullptr);
#endif

#ifdef ENABLE_LUX_SENSOR
		// Configuramos el sensor de luz
		bh1750.begin(BH1750LIB_MODE_CONTINUOUSHIGHRES);
		float lux = bh1750.lightLevel();
		if (FUSE::sensor_value_ok(lux))
		{
			lux = apply_offset(lux, PERSISTENCE::cfg().offsets.lux, 0.0f, NAN, true);
			// Iniciamos la media con la primera medida del sensor de luz
			lux_avg.add(lux);
		} else {
			LOG_WARN("{BEGIN} Invalid initial Illuminance value");
		}

#endif

#ifdef TEMP_SENSOR_BME280
		// Forxzamos la primera medida e inicializamos las medias
		bme280.takeForcedMeasurement(); // Tenemos que forzar la lectura, estamos en un modo personalizado
#endif

		float wtemp;
		if (_retrieve_fused_temperature(wtemp)) {
			wtemp = apply_offset(wtemp, PERSISTENCE::cfg().offsets.temp, 0.0f, NAN, true);
			temperature_avg.add(wtemp);
		} else {
			LOG_WARN("{BEGIN} Invalid initial temperature value");
		}


#ifdef HAS_HUMIDITY_DATA
		float whum;
		if (_retrieve_fused_humidity(whum)) {
			whum = apply_offset(whum, PERSISTENCE::cfg().offsets.hum, 0.0f, NAN, true);
			humidity_avg.add(whum);
		} else {
			LOG_WARN("{BEGIN} Invalid initial humidity value");
		}
#endif

		float press = bme280.readPressure();
		if (FUSE::sensor_value_ok(press))
		{
			press = apply_offset(press, PERSISTENCE::cfg().offsets.press, 0.0f, NAN, true);
			pressure_avg.add(press);
		} else {
			LOG_WARN("{BEGIN} Invalid initial pressure value");
		}

#ifdef CJMCU811_EXTERNAL_TEMPERATURE
		//  TODO: No comprobamos que tengamos valores validos?
		ccs811.setEnvironmentalData((uint8_t)round(humidity_avg.get()), temperature_avg.get());
#else
		ccs811.setTempOffset(ccs.calculateTemperature() - 25.0);
#endif
	}

	bool read_temperature_sensor()
	{
		// Leemos el sensor de temperatura/presion/humedad
#ifdef TEMP_SENSOR_BME280
		bme280.takeForcedMeasurement(); // Tenemos que forzar la lectura, estamos en un modo personalizado
#endif

		float wtemp;
		if (_retrieve_fused_temperature(wtemp)) {
			wtemp = apply_offset(wtemp, PERSISTENCE::cfg().offsets.temp, 0.0f, NAN, true);
			temperature_avg.add(wtemp);
			ML_TIMEPLOT_TF("TMP", "%4.2f", temperature_avg.get());
			LOG_DBG("{SENSORS} Temperature calculated: avg: %.2f last: %.2f", temperature_avg.get(), wtemp);
		} else {
			LOG_WARN("{SENSORS} Temperature (fused) value is NaN");
		}

#ifdef HAS_HUMIDITY_DATA
		float whum;
		if (_retrieve_fused_humidity(whum)) {
			whum = apply_offset(whum, PERSISTENCE::cfg().offsets.hum, 0.0f, NAN, true);
			humidity_avg.add(whum);
			ML_TIMEPLOT_TF("HUM", "%4.2f", humidity_avg.get());
			LOG_DBG("{SENSORS} Humidity calculated: avg: %.2f last: %.2f", humidity_avg.get(), whum);
		} else {
			LOG_WARN("{SENSORS} Humidity (fused) value is NaN");
		}
#endif

		float press = bme280.readPressure();
		if (FUSE::sensor_value_ok(press))
		{
			press = apply_offset(press, PERSISTENCE::cfg().offsets.press, 0.0f, NAN, true);
			pressure_avg.add(press);
			ML_TIMEPLOT_TF("PRE", "%4.2f", pressure_avg.get());
			LOG_DBG("{SENSORS} Pressure calculated: avg: %.2f last: %.2f", pressure_avg.get(), press);
		}
		else
		{
			LOG_WARN("{SENSORS} Pressure value is NaN");
		}
		return true;
	}

#ifdef ENABLE_LUX_SENSOR
	bool read_lux_sensor()
	{
		float lux = bh1750.lightLevel();
		if (FUSE::sensor_value_ok(lux))
		{
			lux = apply_offset(lux, PERSISTENCE::cfg().offsets.lux, 0.0f, NAN, true);
			lux_avg.add(lux);
			ML_TIMEPLOT_TF("LUX", "%.0f", lux_avg.get());
			LOG_DBG("{SENSORS} Illuminance calculated: avg: %.0f last: %.0f", lux_avg.get(), lux);
			return true;
		}
		else
		{
			LOG_WARN("{SENSORS} Illuminance value is NaN");
		}
		return false;
	}
#endif

	bool read_gas_sensor()
	{
		if (!ccs811_measure_ready)
		{
			return false; // Nada que hacer
		}

		// Limpia el flag sí o sí al salir de la función (RAII)
		struct Finally
		{
			volatile bool *f;
			~Finally() { *f = false; }
		} _finally{&ccs811_measure_ready};

#ifdef CJMCU811_EXTERNAL_TEMPERATURE
		// --- Actualización de T/RH con early-exit por tiempo ---
		static unsigned long s_last_env_ms = 0;
		static float s_last_t = NAN;
		static float s_last_h = NAN;

		const unsigned long now = millis();
		const bool time_ok = (now - s_last_env_ms) >= CJMCU811_MIN_TIME_BETWEEN_ENVIRONMENTAL_DATA_UPDATE;

		// TODO: Que pasa en los sistemas sin información de humedd? 

		if (time_ok)
		{
			// Sólo si toca por tiempo calculamos promedios y consideramos umbrales
			const float tavg = temperature_avg.get();
			const float havg = humidity_avg.get();

			if (!isnan(tavg) && !isnan(havg))
			{
				const bool t_jump = isnan(s_last_t) || fabsf(tavg - s_last_t) >= CJMCU811_TEMPERATURE_UPDATE_THRESHOLD;
				const bool h_jump = isnan(s_last_h) || fabsf(havg - s_last_h) >= CJMCU811_HUMIDITY_UPDATE_THRESHOLD;

				if (t_jump || h_jump)
				{
					ccs811.setEnvironmentalData((uint8_t)lround(havg), (float)tavg);
					s_last_t = (float)tavg;
					s_last_h = (float)havg;
					LOG_DBG("CSS811 Enviromental dta set - temp: %.2f hum: %.2f", s_last_t, s_last_h);
				}
				s_last_env_ms = now;
			}
			else
			{
				LOG_ERROR("CSS811 T/H NaN: no se actualiza setEnvironmentalData()");
			}
		}
#else
		// Sin sensor externo de T/H: no aplicamos compensación ambiental.
		// (La placa china CCS811 no tiene NTC para calculateTemperature)
		ccs811.setTempOffset(ccs.calculateTemperature() - 25.0f);
#endif

		// --- Lectura de datos del CCS811 con validación ---
		static uint8_t s_consec_errors = 0;

		if (ccs811.readData() != 0 || ccs811.checkError())
		{
			if (++s_consec_errors == CCS811_MAX_CONSEC_ERRORS)
				LOG_WARN("CCS811: %u consecutive errors", (unsigned)s_consec_errors);
			LOG_ERROR("CCS811 error reading data");
			return false;
		}

		const float co2  = (float)ccs811.geteCO2();
		const float tvoc = (float)ccs811.getTVOC();

		// --- CO2: rango + spike detection ---
		const bool co2_range_ok = (co2 >= CCS811_CO2_MIN_PPM && co2 <= CCS811_CO2_MAX_PPM);
		const float co2_cur = co2_avg.get();
		const bool co2_spike = (!isnan(co2_cur) && co2_cur > 0.0f
		                        && co2 > co2_cur * CCS811_SPIKE_FACTOR);

		if (co2_range_ok && !co2_spike)
		{
			co2_avg.add(co2);
			s_consec_errors = 0;
			ML_TIMEPLOT_TF("CO2", "%.0f", co2);
			LOG_DBG("{SENSORS} CO2: %.0f avg: %.0f", co2, co2_avg.get());
		}
		else
		{
			LOG_WARN("CCS811 CO2 rejected: %.0f (avg=%.0f spike=%d)", co2, co2_cur, (int)co2_spike);
		}

		// --- TVOC: rango simple ---
		if (tvoc >= 0.0f && tvoc <= CCS811_TVOC_MAX_PPB)
		{
			tvoc_avg.add(tvoc);
			ML_TIMEPLOT_TF("TVOC", "%.0f", tvoc);
			LOG_DBG("{SENSORS} TVOC: %.0f avg: %.0f", tvoc, tvoc_avg.get());
		}
		else
		{
			LOG_WARN("CCS811 TVOC rejected: %.0f", tvoc);
		}

		// --- Baseline: restaurar tras warmup, guardar periódicamente ---
		{
			static bool s_baseline_restored = false;
			static uint32_t s_last_baseline_save_ms = 0;
			const uint32_t now_bl = millis();

			// Restaurar baseline una sola vez tras 20 min de warmup
			if (!s_baseline_restored && now_bl >= CCS811_BASELINE_WARMUP_MS)
			{
				s_baseline_restored = true;
				const uint16_t saved = PERSISTENCE::cfg().ccs811_baseline;
				if (saved != 0)
				{
					ccs811.setBaseline(saved);
					LOG_INFO("CCS811 baseline restored: 0x%04X", saved);
				}
				else
				{
					LOG_INFO("CCS811 no saved baseline — starting fresh");
				}
				s_last_baseline_save_ms = now_bl;
			}

			// Guardar baseline periódicamente (solo tras warmup)
			if (s_baseline_restored && (now_bl - s_last_baseline_save_ms) >= CCS811_BASELINE_SAVE_MS)
			{
				const uint16_t cur_bl = ccs811.getBaseline();
				if (cur_bl != 0 && cur_bl != PERSISTENCE::cfg().ccs811_baseline)
				{
					PERSISTENCE::cfg().ccs811_baseline = cur_bl;
					PERSISTENCE::mark_dirty();
					PERSISTENCE::save_if_dirty(0);
					LOG_INFO("CCS811 baseline saved: 0x%04X", cur_bl);
				}
				s_last_baseline_save_ms = now_bl;
			}
		}

		return true;
	}

#ifdef HAS_PARTICLE_DATA
	bool read_pms_sensor()
	{
		PmsSensor::PmsStatus pmsres = pms.loop();
		if (pmsres != PmsSensor::PmsStatus::OK)
		{
			return false;
		}



#ifdef ENABLE_AQI
		if (g_aqi)
		{
			// Obtén concentraciones (µg/m³) actuales
			const float pm25 = pms.getCurrentValue(SaqPmsDataField_t::PM2dot5);
			const float pm10 = pms.getCurrentValue(SaqPmsDataField_t::PM10dot0);
			const uint32_t ts = (uint32_t)Time.now();
			g_aqi->feedSample(AQI::Pollutant::PM25, pm25, ts);
			g_aqi->feedSample(AQI::Pollutant::PM10, pm10, ts);
		}
#endif
		return true;
	}
#endif

	float get_current_humidity() { return humidity_avg.get(); }
	float get_current_temperature() { return temperature_avg.get(); }
	void on_gas_update_active_isr() { ccs811_measure_ready = true; }

// ==== Getters AQI por bucket ====
#ifdef ENABLE_AQI
	bool get_aqi(Pollutant p, Scope s, float &conc, int16_t &aqi)
	{
		if (!g_aqi)
			return false;
		switch (s)
		{
		case Scope::Minute:
			return g_aqi->getLastMinute(p, conc, aqi);
		case Scope::Hour:
			return g_aqi->getLastHour(p, conc, aqi);
		case Scope::Day:
			return g_aqi->getLastDay(p, conc, aqi);
		case Scope::NowCast:
			return g_aqi->getLastNowcast(p, conc, aqi);
		}
		return false;
	}
#endif

} // namespace SENSORS