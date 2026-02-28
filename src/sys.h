#ifndef _MODULESYS_H_
#define _MODULESYS_H_

#include "application.h"
//Flag que indica si se esta actualizando el firmware
extern bool is_flashing;

namespace SYS{
	void begin();
	void on_system_events(system_event_t, int);
	void set_event_handlers();
	bool sync_time();
}

#endif
