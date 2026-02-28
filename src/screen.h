#ifndef _MODULESCREEN_H_
#define _MODULESCREEN_H_

#include "application.h"

// Forward declaration
class Adafruit_SSD1306;

extern bool is_flashing;
extern Adafruit_SSD1306 display_obj;
extern const unsigned char mdps_logo[] PROGMEM;

// ===== Layout de pantalla (64 px alto; ajusta BOT_H si tu font es más alta) =====
#define TOP_H    10    // barra superior: icono wifi + CO2
#define BOT_H    10    // banda inferior: línea de T|H|LUX|dB
#define TOP_Y    0
#define BOT_Y   (H() - BOT_H)
#define MID_Y   (TOP_Y + TOP_H)
#define MID_H   (H() - TOP_H - BOT_H)

namespace SCREEN {
  void begin();
  void update_wifi();
  // void update_time();
  void update_co2();
  void update_sensors();
  bool update_pm();
  void set_flashing();
  void set_flashing_ok();
  // Pinta secciones (no llaman display())
  void paint_top();       // wifi + (opcional hora / CO2 si lo dejas arriba)
  void paint_mid();       // PM2.5 grande + unidad
  void paint_bottom();    // T | H | [LUX] | dB con ajuste de ancho

  // Flush condicional: hace display() solo si hay algo sucio
  bool flush_if_dirty();

  // update_display mantiene compatibilidad: pinta todo y flush
  bool update_display();
}

#endif
