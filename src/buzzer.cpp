#include "buzzer.h"

#ifdef ENABLE_BUZZER

bool _buzzer_enabled = ACOUSTIC_NOTIFICATIONS_ENABLED;
const char *connection_ok_rtttl = "ok:d=8,o=5,b=112:d4,4f2";
const char *connection_error_rtttl = "ko:d=32,o=6,b=100:4a,a,2p";
const char *reboot_rtttl = "rst:d=9,o=2,b=200:c6,8f6,c6,8f6";
const char *init_rtttl = "i:d=4,o=5,b=150:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p,8g,8p";
char playing_song[AUDIO_SONG_BUFFER_SIZE] = {0};

void BUZZER::begin() {
	pinMode(BUZZER_PIN, OUTPUT);
	BUZZER::play(init_rtttl, true, true);
}

bool BUZZER::play(const char *rrtl, bool force, bool block)
{
	if(_buzzer_enabled){
		if(rtttl::isPlaying())
		{
			if(force)
			{
				rtttl::stop();
				//TODO: no falta aquí un "return true;"?
			}
			else
			{
				return false;
			}
		}
		strncpy(playing_song, rrtl, AUDIO_SONG_BUFFER_SIZE - 1);
		playing_song[AUDIO_SONG_BUFFER_SIZE - 1] = '\0';
		rtttl::begin(BUZZER_PIN, playing_song);
		if(block)
		{
			do
			{
				rtttl::play();
			}	
			while(rtttl::isPlaying());
		}
		return true;
	}
	return false;
}

void BUZZER::reboot(bool block) {
	BUZZER::play(reboot_rtttl,true,block);
}

void BUZZER::success(bool block) {
	BUZZER::play(connection_ok_rtttl,true,block);
}
void BUZZER::error(bool block) {
	BUZZER::play(connection_error_rtttl,true,block);
}
void BUZZER::silence() {
	rtttl::stop();
}

void BUZZER::enable()
{
	_buzzer_enabled = true;
}
void BUZZER::disable()
{
	_buzzer_enabled = false;
	rtttl::stop();
}
bool BUZZER::is_enabled()
{
	return _buzzer_enabled;
}

void BUZZER::loop()
{
	if ( rtttl::isPlaying() )
	{
		if(_buzzer_enabled)
		{
			rtttl::play();
		}
		else
		{
			rtttl::stop();
		}
	}
}

#endif // ENABLE_BUZZER