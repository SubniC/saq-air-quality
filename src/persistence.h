#ifndef _MODULEPERSISTENCE_H_
#define _MODULEPERSISTENCE_H_

#include "application.h"
#include "config/config.h"

// Offsets aditivos para corrección de sensores analógicos
struct CalibOffsets {
    float temp;     // °C
    float hum;      // %RH
    float lux;      // lux
    float press;    // mBar
};

// Calibración de audio (valores absolutos, no offsets)
struct AudioCalib {
    float mic_noise_floor;  // dBFS de silencio medido (0 = sin calibrar)
};

namespace PERSISTENCE
{

	// Layout fijo — sin #ifdef para garantizar orden binario estable.
	// Los campos de features deshabilitados simplemente se ignoran en runtime.
	struct Data
	{
		CalibOffsets offsets;     // Correcciones de sensores
		AudioCalib   audio;      // Calibración de micrófono
		uint8_t neopixel_enabled;
		uint8_t led_brightness;
		uint8_t buzzer_enabled;
		uint8_t pms_sleep_min;       // 0 = continuo (sin duty-cycling)
		uint8_t pms_wake_sec;        // segundos de wake (incluye warmup)
		uint16_t ccs811_baseline;    // baseline register del CCS811 (0 = sin calibrar)
		uint8_t reserved[9] = {0};  // Reserva para futuro
	};

	// API
	bool begin();	   // Carga si válido, si no defaults + save
	void load();	   // Fuerza lectura (valida)
	void save();	   // Escribe si cabe en EEPROM
	void mark_dirty(); // Marca que hay cambios pendientes
	bool is_valid();   // Última carga válida
	Data &data();	   // Acceso R/W a la copia en RAM

	// Opcional, ahorro de desgaste
	void save_if_dirty(uint32_t min_interval_ms = 5000);

	inline Data& cfg() { return data(); }
} // namespace PERSISTENCE

#endif
