#include "sys.h"
#include "sound.h"

#ifdef ENABLE_LCD
#include "screen.h"
#endif


#include "config/config.h"
#include "utils/debug.h"

bool is_flashing = false;

void SYS::begin()
{
	Time.zone(SYSTEM_TIMEZONE);
	LOG_INIT();
	// TODO: Especifico de photon!
	Wire.setSpeed(400000);  // 400 kHz
}

//Sincronizacion del tiempo
bool SYS::sync_time()
{
	if(Particle.connected())
	{
		Particle.syncTime();
	}
	DBG("sync_time");
	ML_MESSAGE("PARTICLE","sync_time");
	return true;
}

void SYS::on_system_events(system_event_t event, int param)
{
	if (param == firmware_update_begin)
	{
		//Hemos iniciado un update de firmware
		is_flashing = true;
		//Notificamos y detenemos el sampleo de audio
		SOUND::stop_sampling();

		#ifdef ENABLE_LCD
			SCREEN::set_flashing();
		#endif

		DBG("firmware_update_begin event");
		ML_MESSAGE("PARTICLE","firmware_update_begin event");
	}
	else if(param == firmware_update_complete)
	{
		//El update a terminado :)
		#ifdef ENABLE_LCD
			SCREEN::set_flashing_ok();
		#endif
	}
}

void SYS::set_event_handlers()
{
	System.on(firmware_update, on_system_events);
}