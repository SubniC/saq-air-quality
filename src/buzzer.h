#ifndef _MODULEBUZZER_H_
#define _MODULEBUZZER_H_

#include "config/config.h"

#ifdef ENABLE_BUZZER

#include "application.h"
#include "libs/NonBlockingRtttl.h"

extern bool _buzzer_enabled;
extern const char *connection_ok_rtttl;
extern const char *connection_error_rtttl;
extern const char *init_rtttl;
extern const char *reboot_rtttl;
extern char playing_song[AUDIO_SONG_BUFFER_SIZE];

namespace BUZZER{
	void begin(void);
	void success(bool=false);
	void error(bool=false);
	void reboot(bool=false);
	void silence(void);

	void loop(void);
	bool play(const char* rttl, bool force = true, bool block = false);

	void enable(void);
	void disable(void);
	bool is_enabled(void);
}

#endif // ENABLE_BUZZER
#endif // _MODULEBUZZER_H_
