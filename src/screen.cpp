#include "screen.h"
#include <cstdio>  // snprintf
#include <cstring> // strlen
#include <math.h>
#include "config/config.h"
#include "libs/Adafruit_mfGFX.h"
#include "libs/Adafruit_SSD1306_mfGFX.h"
#include "sound.h"
#include "sensors.h"
#include "comms.h"
#include "utils/debug.h"


Adafruit_SSD1306 display_obj(LCD_PIN);

// Logo MDPS para el splash
const unsigned char mdps_logo[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8, 0x7C, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
    0x00, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8, 0x7C, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
    0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8, 0x7C, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8, 0x7C, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
    0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8, 0x7C, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
    0x03, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E,
    0x07, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E,
    0x0F, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E,
    0x1F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E,
    0x3F, 0x81, 0xFF, 0xFF, 0xC0, 0x1F, 0xFF, 0xFC, 0x01, 0xFF, 0xFF, 0xC3, 0xFF, 0xFF, 0xFC, 0x3E,
    0x7F, 0x03, 0xFF, 0xFF, 0xC0, 0x3F, 0xFF, 0xFC, 0x03, 0xFF, 0xFF, 0xC3, 0xFF, 0xFF, 0xFC, 0x3E,
    0x7E, 0x07, 0xFF, 0xFF, 0xC0, 0x7F, 0xFF, 0xFC, 0x07, 0xFF, 0xFF, 0xC3, 0xFF, 0xFF, 0xFC, 0x3E,
    0x7C, 0x0F, 0xFF, 0xFF, 0xC0, 0xFF, 0xFF, 0xFC, 0x0F, 0xFF, 0xFF, 0xC3, 0xFF, 0xFF, 0xFC, 0x3E,
    0x7C, 0x1F, 0xFF, 0xFF, 0xC1, 0xFF, 0xFF, 0xFC, 0x1F, 0xFF, 0xFF, 0xC3, 0xFF, 0xFF, 0xFC, 0x3E,
    0x7C, 0x3F, 0x8F, 0x87, 0xC3, 0xF8, 0x00, 0x7C, 0x3F, 0x80, 0x07, 0xC3, 0xE0, 0x00, 0x00, 0x3E,
    0x7C, 0x3F, 0x0F, 0x87, 0xC3, 0xF0, 0x00, 0x7C, 0x3F, 0x00, 0x07, 0xC3, 0xE0, 0x00, 0x00, 0x3E,
    0x7C, 0x3E, 0x0F, 0x87, 0xC3, 0xE0, 0x00, 0x7C, 0x3E, 0x00, 0x07, 0xC3, 0xE0, 0x00, 0x00, 0x3E,
    0x7C, 0x3E, 0x0F, 0x87, 0xC3, 0xE0, 0x00, 0x7C, 0x3E, 0x00, 0x07, 0xC3, 0xFF, 0xFF, 0xFC, 0x3E,
    0x7C, 0x3E, 0x0F, 0x87, 0xC3, 0xE0, 0x00, 0x7C, 0x3E, 0x00, 0x07, 0xC3, 0xFF, 0xFF, 0xFC, 0x3E,
    0x7C, 0x3E, 0x0F, 0x87, 0xC3, 0xE0, 0x00, 0x7C, 0x3E, 0x00, 0x07, 0xC3, 0xFF, 0xFF, 0xFC, 0x3E,
    0x7C, 0x3E, 0x0F, 0x87, 0xC3, 0xE0, 0x00, 0x7C, 0x3E, 0x00, 0x07, 0xC3, 0xFF, 0xFF, 0xFC, 0x3E,
    0x7C, 0x3E, 0x0F, 0x87, 0xC3, 0xE0, 0x00, 0x7C, 0x3E, 0x00, 0x07, 0xC3, 0xFF, 0xFF, 0xFC, 0x3E,
    0x7C, 0x3E, 0x0F, 0x87, 0xC3, 0xE0, 0x00, 0x7C, 0x3E, 0x00, 0x07, 0xC0, 0x00, 0x00, 0x7C, 0x3E,
    0x7C, 0x3E, 0x0F, 0x87, 0xC3, 0xE0, 0x00, 0x7C, 0x3E, 0x00, 0x07, 0xC0, 0x00, 0x00, 0x7C, 0x3E,
    0x7C, 0x3E, 0x0F, 0x87, 0xC3, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xC3, 0xFF, 0xFF, 0xF8, 0x3E,
    0x7C, 0x3E, 0x0F, 0x87, 0xC3, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xC3, 0xFF, 0xFF, 0xF0, 0x3E,
    0x7C, 0x3E, 0x0F, 0x87, 0xC3, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xC3, 0xFF, 0xFF, 0xF0, 0x7E,
    0x7C, 0x3E, 0x0F, 0x87, 0xC3, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xC3, 0xFF, 0xFF, 0xC0, 0xFE,
    0x7C, 0x3E, 0x0F, 0x87, 0xC3, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xC3, 0xFF, 0xFF, 0x81, 0xFC,
    0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xF8,
    0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xF0,
    0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xE0,
    0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0xC0,
    0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0x3E, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x80,
    0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0x3E, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
    0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0x3E, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0x00,
    0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0x3E, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0x00,
    0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0x3E, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// ----------------- helpers (no API pública) -----------------

namespace
{
  struct Dirty
  {
    bool top{true};
    bool mid{true};
    bool bot{true};
  } s_dirty;

  // Devuelve el valor actual del EmaF, o NaN si aún no hay datos
  static inline float lastOrNan(EmaF &ema)
  {
    return ema.get();
  }

  // Formatea un número a texto o "--" si NaN
  static inline void fmtNum(char *out, size_t cap, float v, const char *fmt)
  {
    if (isnan(v))
    {
      strncpy(out, "--", cap);
      out[cap - 1] = '\0';
      return;
    }
    snprintf(out, cap, fmt, v);
  }

  inline int W() { return display_obj.width(); }
  inline int H() { return display_obj.height(); }

  int textWidthFor(const String &s)
  {
    int w = 0;
    for (size_t i = 0, n = s.length(); i < n; ++i)
      w += display_obj.charWidth(s.charAt(i));
    return w;
  }

  int textWidthForC(const char *s)
  {
    int w = 0;
    for (size_t i = 0; s && s[i] != '\0'; ++i)
      w += display_obj.charWidth(s[i]);
    return w;
  }

  String trimToWidth(const String &s, int maxW)
  {
    if (maxW <= 0)
      return String("");
    String out;
    out.reserve(s.length());
    int w = 0;
    const int dotsW = display_obj.charWidth('.') * 3; // "..."
    for (size_t i = 0, n = s.length(); i < n; ++i)
    {
      int cw = display_obj.charWidth(s.charAt(i));
      // deja sitio para "..." si no cabe
      if (w + cw > maxW)
      {
        if (w + dotsW <= maxW)
          out += "...";
        break;
      }
      out += s.charAt(i);
      w += cw;
    }
    return out;
  }

  static inline void clear_top(int x = 0, int w = -1)
  {
    if (w < 0)
      w = W() - x;
    display_obj.fillRect(x, TOP_Y, w, TOP_H, BLACK);
  }
  static inline void clear_mid()
  {
    display_obj.fillRect(0, MID_Y, W(), MID_H, BLACK);
  }
  static inline void clear_bot()
  {
    display_obj.fillRect(0, BOT_Y, W(), BOT_H, BLACK);
  }

  static inline int rightAlignX(int text_w, int padding = 0)
  {
    int x = W() - padding - text_w;
    return (x < 0) ? 0 : x;
  }

  // inline int rightAlignX(int widthPx, int rightPadding = 1)
  // {
  //   int x = W() - rightPadding - widthPx;
  //   return x < 0 ? 0 : x;
  // }

  // inline void clearTopBandFromX(int x, int bandHeight = 10)
  // {
  //   if (x < 0)
  //     x = 0;
  //   display_obj.fillRect(x, 0, W() - x, bandHeight, BLACK);
  // }
}

// Genera la línea inferior intentando que quepa en el ancho
void buildBottomLine(char *out, size_t cap)
{
  float t_last = lastOrNan(temperature_avg);
  float h_last =
#ifdef HAS_HUMIDITY_DATA
      lastOrNan(humidity_avg);
#else
      NAN;
#endif
  float lux_last =
#ifdef ENABLE_LUX_SENSOR
      lastOrNan(lux_avg);
#else
      NAN;
#endif
  float db_last = sound_meter->getNoiseLevel();

  // 1) Formato más legible
  char t_s[12], h_s[12], lux_s[12], db_s[12];
  fmtNum(t_s, sizeof(t_s), t_last, "%.1f"); // 1 decimal
  fmtNum(h_s, sizeof(h_s), h_last, "%.0f");
  fmtNum(lux_s, sizeof(lux_s), lux_last, "%.0f");
  fmtNum(db_s, sizeof(db_s), db_last, "%.0f");

  // Candidatos de mayor a menor ancho
  // A: con separadores y LUX
#ifdef ENABLE_LUX_SENSOR
  snprintf(out, cap, "%sC | %s%% | %slx | %sdB", t_s, h_s, lux_s, db_s);
#else
  snprintf(out, cap, "%sC | %s%% | %sdB", t_s, h_s, db_s);
#endif
  if (textWidthForC(out) <= W())
    return;

  // B: sin separadores (ahorra varios px)
#ifdef ENABLE_LUX_SENSOR
  snprintf(out, cap, "%sC %s%% %slx %sdB", t_s, h_s, lux_s, db_s);
#else
  snprintf(out, cap, "%sC %s%% %sdB", t_s, h_s, db_s);
#endif
  if (textWidthForC(out) <= W())
    return;

  // C: sin decimales en T
  fmtNum(t_s, sizeof(t_s), t_last, "%.0f");
#ifdef ENABLE_LUX_SENSOR
  snprintf(out, cap, "%sC %s%% %slx %sdB", t_s, h_s, lux_s, db_s);
#else
  snprintf(out, cap, "%sC %s%% %sdB", t_s, h_s, db_s);
#endif
  if (textWidthForC(out) <= W())
    return;

#ifdef ENABLE_LUX_SENSOR
  // D: quita LUX como último recurso (mantén T/H/dB)
  snprintf(out, cap, "%sC %s%% %sdB", t_s, h_s, db_s);
  if (textWidthForC(out) <= W())
    return;
#endif

  // E: ultra compacto: T y H sin unidades + dB
  snprintf(out, cap, "%s %s %s", t_s, h_s, db_s);
}
// ----------------- API pública -----------------

void SCREEN::begin()
{
  display_obj.begin(SSD1306_SWITCHCAPVCC, LCD_I2C_ADDRESS);
  display_obj.setTextColor(WHITE);

  display_obj.clearDisplay();
  // Dibuja el logo centrado verticalmente (altura logo = 41, ancho = 128)
  int y = (H() - 41) / 2;
  if (y < 0)
    y = 0;
  display_obj.drawBitmap(0, y, mdps_logo, 128, 41, 1);
  display_obj.display();
}

// void SCREEN::update_time()
// {
//   display_obj.setFont(SANSSERIF_6);
//   String time_out = Time.format(Time.now(), "%d/%m/%Y %H:%M");

//   const int w = textWidthFor(time_out);
//   clearTopBandFromX(W() - (w + 1), 10);
//   display_obj.setCursor(rightAlignX(w, 1), 0);
//   display_obj.print(time_out);
// }

void SCREEN::update_wifi() {
  // Limpia sólo el área del icono
  clear_top(0, 40);

  if (!COMMS::network_ready()) {
    display_obj.setFont(CENTURY_8);
    display_obj.setCursor(0, TOP_Y);
    display_obj.print("Error");
    return;
  }

  // 1) RSSI -> 0..100 (saturado)
  int rssi = WiFi.RSSI();
  // int q = map(rssi, -1, -127, 100, 0);
  int q = map(rssi, -30, -90, 100, 0);
  if (q < 0) q = 0;
  else if (q > 100) q = 100;

  LOG_EVERY_MS(DBG, 2000, "[DEBUG] {SCREEN} WIFI RSSI=%ddB - mapped=%d%%", rssi, q);

  // 2) Barras adaptadas a la altura de la banda
  const int bars = 5;
  const int barW = 4;        // ancho de cada barra
  const int gap  = 2;        // separación entre barras
  const int baseX = 0;       // desplazamiento X del icono
  const int baseY = TOP_Y + TOP_H - 1;
  const int maxH = (TOP_H >= 6) ? (TOP_H - 2) : TOP_H; // deja 1px margen arriba/abajo

  // Para q=0..100, activamos 0..5 barras (umbrales ~20,40,60,80,100)
  for (int i = 0; i < bars; ++i) {
    int thr = (i + 1) * 100 / bars;   // 20,40,60,80,100
    if (q >= thr) {
      int h = (i + 1) * maxH / bars;  // alturas crecientes
      if (h < 1) h = 1;
      int x = baseX + i * (barW + gap);
      display_obj.fillRect(x, baseY - h + 1, barW, h, WHITE);
    }
  }
}

// void SCREEN::update_wifi()
// {
//   // limpia zona de icono/signal
//   display_obj.fillRect(0, 0, 30, 10, BLACK);

//   if (COMMS::network_ready())
//   {
//     int wifi_rssi = WiFi.RSSI();
//     int wifi_quality = map(wifi_rssi, -1, -127, 100, 0);

//     if (wifi_quality > 0)
//     {
//       display_obj.drawFastVLine(0, 6, 2, WHITE);
//       display_obj.drawFastVLine(1, 6, 2, WHITE);
//     }
//     if (wifi_quality >= 20)
//     {
//       display_obj.drawFastVLine(3, 5, 3, WHITE);
//       display_obj.drawFastVLine(4, 5, 3, WHITE);
//     }
//     if (wifi_quality >= 40)
//     {
//       display_obj.drawFastVLine(6, 4, 4, WHITE);
//       display_obj.drawFastVLine(7, 4, 4, WHITE);
//     }
//     if (wifi_quality >= 60)
//     {
//       display_obj.drawFastVLine(9, 3, 5, WHITE);
//       display_obj.drawFastVLine(10, 3, 5, WHITE);
//     }
//     if (wifi_quality >= 80)
//     {
//       display_obj.drawFastVLine(12, 2, 6, WHITE);
//       display_obj.drawFastVLine(13, 2, 6, WHITE);
//     }
//   }
//   else
//   {
//     display_obj.setFont(SANSSERIF_6);
//     display_obj.setCursor(0, 0);
//     display_obj.print("Error");
//   }
// }

void SCREEN::update_co2() {
  display_obj.setFont(CENTURY_8);
  // limpia desde x=40 hasta el final de la banda superior
  clear_top(40);

  float co2 = lastOrNan(co2_avg);
  char co2_s[12], buf[32];
  fmtNum(co2_s, sizeof(co2_s), co2, "%.0f");
  snprintf(buf, sizeof(buf), "CO2 %sppm", co2_s);

  const int gw = textWidthForC(buf);
  display_obj.setCursor(rightAlignX(gw, 1), TOP_Y); // y dentro de TOP
  display_obj.print(buf);
}

// void SCREEN::update_co2()
// {
//   display_obj.setFont(SANSSERIF_6);

//   // Superior derecha: CO2 (promedio rápido si hay, si no último)
//   clearTopBandFromX(40, 10);

//   float co2 = lastOrNan(co2_avg);

//   char co2_s[12], gas_buff[32];
//   fmtNum(co2_s, sizeof(co2_s), co2, "%.0f");
//   snprintf(gas_buff, sizeof(gas_buff), "CO2 %sppm", co2_s);

//   const int gw = textWidthForC(gas_buff);
//   display_obj.setCursor(rightAlignX(gw, 1), 0);
//   display_obj.print(gas_buff);
// }

void SCREEN::update_sensors() {
  clear_bot();

  float t_last = lastOrNan(temperature_avg);
  float h_last =
  #ifdef HAS_HUMIDITY_DATA
    lastOrNan(humidity_avg);
  #else
    NAN;
  #endif

  float lux_last =
  #ifdef ENABLE_LUX_SENSOR
    lastOrNan(lux_avg);
  #else
    NAN;
  #endif

  float db_last = sound_meter->getNoiseLevel();

  char t_s[12], h_s[12], lux_s[12], db_s[12], line[80];
  fmtNum(t_s,   sizeof(t_s),   t_last,   "%.1f");
  fmtNum(h_s,   sizeof(h_s),   h_last,   "%.0f");
  fmtNum(lux_s, sizeof(lux_s), lux_last, "%.0f");
  fmtNum(db_s,  sizeof(db_s),  db_last,  "%.0f");

  display_obj.setFont(CENTURY_8);
  #ifdef ENABLE_LUX_SENSOR
    snprintf(line, sizeof(line), "%sC | %s%% | %slx | %sdB", t_s, h_s, lux_s, db_s);
  #else
    snprintf(line, sizeof(line), "%sC | %s%% | %sdB",       t_s, h_s, db_s);
  #endif

  display_obj.setCursor(0, BOT_Y); // dentro de la banda inferior
  display_obj.print(line);
}

// Devuelve true si repintó (valor cambió), false si no hacía falta
bool SCREEN::update_pm() {
  float pm = pms.getCurrentValue(SaqPmsDataField_t::PM2dot5);
  char pm_str[12];
  fmtNum(pm_str, sizeof(pm_str), pm, "%.0f"); // "--" si NaN

  // Indicador de estado: paréntesis si el PMS no está activo (dato stale)
  const bool stale = (pms.getDutyState() != SaqPMS::DutyState::ACTIVE);
  if (stale)
  {
    char tmp[14];
    snprintf(tmp, sizeof(tmp), "(%s)", pm_str);
    strncpy(pm_str, tmp, sizeof(pm_str));
    pm_str[sizeof(pm_str) - 1] = '\0';
  }

  // Si el texto no cambió, no repintar
  static char s_prev[12] = {};
  if (strcmp(pm_str, s_prev) == 0)
    return false;
  strncpy(s_prev, pm_str, sizeof(s_prev));

  clear_mid();

  const char* unit = "ug/m3";

  display_obj.setFont(SANSSERIF_24_NUM);
  const int w_val  = textWidthForC(pm_str);
  display_obj.setFont(CENTURY_8);
  const int w_unit = textWidthForC(unit);

  const int spacing = 4;
  const int w_total = w_val + spacing + w_unit;
  int x_val = (W() - w_total) / 2; if (x_val < 0) x_val = 0;

  // Y"s relativos al MID_Y para no pisar la banda inferior
  display_obj.setFont(SANSSERIF_24_NUM);
  display_obj.setCursor(x_val, MID_Y + 9); // ajusta 9 si tu baseline difiere
  display_obj.print(pm_str);

  display_obj.setFont(CENTURY_8);
  display_obj.setCursor(x_val + w_val + spacing, MID_Y + 25); // debajo del número
  display_obj.print(unit);
  return true;
}

// void SCREEN::update_pm()
// {
//   display_obj.fillRect(0, 10, W(), 55, BLACK);

//   // 1) Valor PM2.5
//   float pm = pms.getCurrentValue(SaqPmsDataField_t::PM2dot5);
//   char pm_str[12];
//   fmtNum(pm_str, sizeof(pm_str), pm, "%.0f"); // "--" si NaN

//   // 2) Unidades
//   const char *unit = "ug/m3";

//   // 3) Medidas para centrar
//   display_obj.setFont(SANSSERIF_24_NUM);
//   const int w_val = textWidthForC(pm_str);
//   display_obj.setFont(CENTURY_8);
//   const int w_unit = textWidthForC(unit);

//   const int spacing = 4;
//   const int w_total = w_val + spacing + w_unit;
//   int x_val = (W() - w_total) / 2;
//   if (x_val < 0)
//     x_val = 0;

//   // 4) Pintar
//   display_obj.setFont(SANSSERIF_24_NUM);
//   display_obj.setCursor(x_val, 19);
//   display_obj.print(pm_str);

//   display_obj.setFont(CENTURY_8);
//   display_obj.setCursor(x_val + w_val + spacing, 35);
//   display_obj.print(unit);
// }

void SCREEN::set_flashing()
{
  display_obj.clearDisplay();
  display_obj.setFont(CENTURY_8);

  const String line1 = "PLEASE WAIT";
  const String line2 = "Uploading firmware";

  String l1 = trimToWidth(line1, W() - 20);
  String l2 = trimToWidth(line2, W() - 20);

  const int x1 = (W() - textWidthFor(l1)) / 2;
  const int x2 = (W() - textWidthFor(l2)) / 2;

  display_obj.setCursor(x1, 10);
  display_obj.print(l1);
  display_obj.setCursor(x2, 30);
  display_obj.print(l2);
  display_obj.display();
}

void SCREEN::set_flashing_ok()
{
  display_obj.clearDisplay();
  display_obj.setFont(CENTURY_8);

  const String line1 = "Update succeed!";
  const String line2 = "rebooting...";

  String l1 = trimToWidth(line1, W() - 20);
  String l2 = trimToWidth(line2, W() - 20);

  const int x1 = (W() - textWidthFor(l1)) / 2;
  const int x2 = (W() - textWidthFor(l2)) / 2;

  display_obj.setCursor(x1, 10);
  display_obj.print(l1);
  display_obj.setCursor(x2, 30);
  display_obj.print(l2);
  display_obj.display();
}

bool SCREEN::update_display()
{
  if (is_flashing)
    return false;

  // Pinta todo y flush (como antes, pero en 2 fases)
  paint_top();
  paint_mid();
  paint_bottom();
  return flush_if_dirty();
}

void SCREEN::paint_top()
{
  // Banda superior: icono WiFi y, si quieres, hora o CO2
  // Reusa tu update_wifi(), update_time(), update_sensors2(...) para CO2 arriba si prefieres
  update_wifi(); // ya borra su rect de 0..30x10

  // CO2
  update_co2();

  // Si también pintas la hora, descomenta:
  // update_time();
  s_dirty.top = true;
}

void SCREEN::paint_mid()
{
  // PM2.5 grande + unidad — solo marca dirty si el valor cambió
  if (update_pm())
    s_dirty.mid = true;
}

void SCREEN::paint_bottom()
{
  // Línea inferior completa, con ajuste de ancho
  // display_obj.fillRect(0, H() - 6, W(), 6, BLACK);
  // display_obj.setFont(CENTURY_8); // si migraste a CENTURY_8 y te cabe, cámbialo aquí

  // char line[80];
  // buildBottomLine(line, sizeof(line));
  // display_obj.setCursor(0, H() - 6);
  // display_obj.print(line);
  
  update_sensors();

  s_dirty.bot = true;
}

bool SCREEN::flush_if_dirty()
{
  if (s_dirty.top || s_dirty.mid || s_dirty.bot)
  {
    display_obj.display(); // coste grande (I²C)
    s_dirty = {};          // limpia todos
    return true;
  }
  return false;
}