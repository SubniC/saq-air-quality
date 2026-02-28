#include "persistence.h"

// ------- Internos -------
namespace
{

	constexpr uint32_t MAGIC = 0x41514250u; // "AQBP" o el que prefieras
	constexpr uint16_t VERSION = 5;			// v5: +ccs811_baseline
	struct Header
	{
		uint32_t magic;
		uint16_t version;
		uint16_t length;   // sizeof(Data)
		uint32_t checksum; // FNV-1a de Data
	} __attribute__((packed));

	constexpr int EEPROM_SIZE = 2048; // Photon (emulada en flash)
	static_assert(sizeof(Header) + (int)sizeof(PERSISTENCE::Data) <= EEPROM_SIZE,
				  "Persistencia no cabe en EEPROM");

	// Dirección base (0). Si prefieres dejar hueco, súbela.
	constexpr int BASE_ADDR = 0;

	inline uint32_t fnv1a32(const void *buf, size_t len)
	{
		const uint8_t *p = static_cast<const uint8_t *>(buf);
		uint32_t h = 2166136261u;
		for (size_t i = 0; i < len; ++i)
		{
			h ^= p[i];
			h *= 16777619u;
		}
		return h;
	}

	// Estado en RAM
	PERSISTENCE::Data g_data{};
	bool g_dirty = false;
	bool g_valid = false;
	uint32_t g_last_save_ms = 0;

	// Helpers EEPROM (escritura en bloque)
	// Lee una struct completa
	template <typename T>
	bool eeprom_read_struct(int addr, T &out)
	{
		if (addr < 0 || addr + (int)sizeof(T) > (int)EEPROM.length())
			return false;
		EEPROM.get(addr, out);
		return true;
	}

	// Escribe una struct completa
	template <typename T>
	bool eeprom_write_struct(int addr, const T &in)
	{
		if (addr < 0 || addr + (int)sizeof(T) > (int)EEPROM.length())
			return false;
		EEPROM.put(addr, in);
		return true;
	}

	void to_defaults(PERSISTENCE::Data &d)
	{
		d.offsets.temp  = 0.0f;
		d.offsets.hum   = 0.0f;
		d.offsets.lux   = 0.0f;
		d.offsets.press = 0.0f;
		d.audio.mic_noise_floor = 0.0f;
		
		#ifdef ENABLE_NEOPIXEL
    		d.neopixel_enabled = 1;
		#else
			d.neopixel_enabled = 0;
		#endif
		
		#ifdef NEOPIXEL_LED_BRIGHTNESS
    		d.neopixel_enabled = NEOPIXEL_LED_BRIGHTNESS;
		#else
			d.led_brightness   = 50;
		#endif

		#ifdef ENABLE_BUZZER
    		d.buzzer_enabled   = 1;
		#else
			d.buzzer_enabled   = 0;
		#endif
		
		d.pms_sleep_min    = PMS_DEFAULT_SLEEP_MIN;
		d.pms_wake_sec     = PMS_DEFAULT_WAKE_SEC;
		d.ccs811_baseline  = 0;  // 0 = sin calibrar
		memset(d.reserved, 0, sizeof(d.reserved));
	}

	bool load_internal()
	{
		Header hdr{};
		if (!eeprom_read_struct(BASE_ADDR, hdr))
			return false;

		if (hdr.magic != MAGIC || hdr.version != VERSION || hdr.length != sizeof(PERSISTENCE::Data))
		{
			return false;
		}

		PERSISTENCE::Data tmp{};
		if (!eeprom_read_struct(BASE_ADDR + (int)sizeof(Header), tmp))
			return false;

		if (fnv1a32(&tmp, sizeof(tmp)) != hdr.checksum)
			return false;

		g_data = tmp;
		return true;
	}

	void save_internal()
	{
		Header hdr{};
		hdr.magic = MAGIC;
		hdr.version = VERSION;
		hdr.length = sizeof(PERSISTENCE::Data);
		hdr.checksum = fnv1a32(&g_data, sizeof(g_data));

		// Opcional: valida capacidad real antes de escribir
		const int needed = (int)sizeof(Header) + (int)sizeof(PERSISTENCE::Data);
		if (needed > (int)EEPROM.length())
		{
			// aquí puedes loguear y abortar, pero no intentes escribir fuera
			return;
		}

		eeprom_write_struct(BASE_ADDR, hdr);
		eeprom_write_struct(BASE_ADDR + (int)sizeof(Header), g_data);
	}
} // namespace

// ------- API -------
namespace PERSISTENCE
{

	bool begin()
	{
		const int needed = (int)sizeof(Header) + (int)sizeof(PERSISTENCE::Data);
		if (needed > (int)EEPROM.length())
		{
			// EEPROM demasiado pequeña para nuestro layout → fallback a defaults en RAM.
			to_defaults(g_data);
			g_valid = false; // no persistimos
			g_dirty = false;
			return false;
		}

		if (load_internal())
		{
			g_valid = true;
			g_dirty = false;
		}
		else
		{
			to_defaults(g_data);
			save_internal();
			g_valid = true;
			g_dirty = false;
		}
		return true;
	}

	void load()
	{
		g_valid = load_internal();
		if (!g_valid)
		{
			to_defaults(g_data);
		}
		g_dirty = false;
	}

	void save()
	{
		save_internal();
		g_dirty = false;
		g_last_save_ms = millis();
	}

	void save_if_dirty(uint32_t min_interval_ms)
	{
		if (!g_dirty)
			return;
		const uint32_t now = millis();
		if ((now - g_last_save_ms) >= min_interval_ms)
		{
			save();
		}
	}

	void mark_dirty() { g_dirty = true; }

	bool is_valid() { return g_valid; }

	Data &data() { return g_data; }

} // namespace PERSISTENCE
