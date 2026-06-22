# Font Optimization Guide

Referencia de las optimizaciones de fuentes LCD (Adafruit_mfGFX) y como modificarlas.

## Contexto

El firmware de SAQ-2 superaba el limite de APP_FLASH por 68 bytes. Las fuentes del LCD eran
el mayor consumidor de flash (~11.5 KB con las 3 fuentes activas). Dos optimizaciones
liberaron ~8.9 KB.

## Arquitectura de fuentes

Cada fuente tiene dos arrays: `Bitmaps[]` (cabecera de 2 bytes + datos por caracter) y
`Descriptors[]` (un `{width, height, offset}` por caracter). El rango de caracteres debe ser
**contiguo**; los caracteres no usados dentro del rango apuntan a un bloque vacio compartido.

Ficheros: `src/libs/fonts.h` (defines, IDs, externs), `src/libs/fonts.cpp` (datos) y
`src/libs/Adafruit_mfGFX.cpp` (`setFont()` con switch por ID).

## Optimizacion 1: SANSSERIF_24 -> SANSSERIF_24_NUM (~5.8 KB)

Subset de SANSSERIF_24 con solo los caracteres usados en la pantalla grande de PM2.5:
espacio, `( ) - .` y digitos `0-9` (rango 0x20-0x39). Los huecos del rango apuntan a un
bloque de ceros. La fuente pasa de ~8.4 KB a ~1.3 KB.

Para anadir un caracter: copiar sus bytes de la seccion `#ifdef SANSSERIF24` original al array
del subset, actualizar el offset en el descriptor y, si cae fuera de 0x20-0x39, ampliar el
`endChar` de la cabecera y rellenar los descriptores intermedios.

## Optimizacion 2: GLCDFONT opcional (~3.1 KB)

GLCDFONT (256 chars, 5x8px) se compilaba siempre como fallback, aunque ningun `print()` lo
usaba. Se anadio una guarda `#ifdef ENABLE_GLCDFONT` en `fonts.cpp`, `fonts.h` y
`Adafruit_mfGFX.cpp` (el default de `setFont()` cae a `CENTURY_8` cuando esta deshabilitado).

## Optimizacion descartada: subset de CENTURYGOTHIC8

CENTURYGOTHIC8 solo ocupa ~1.5 KB y se usa para texto variado (labels, unidades, errores),
con un conjunto de caracteres dificil de auditar. El ahorro (~500-700 B) no justifica el
riesgo de olvidar un caracter usado.

## Estado actual

Activas: CENTURYGOTHIC8 (~1.5 KB, texto general) y SANSSERIF24_NUM (~1.3 KB, digitos PM).
El resto de fuentes (SANSSERIF_24, GLCDFONT, TIMESNEWROMAN8, ARIAL8, COMICSANSMS8,
SANSSERIF_6/12, TESTFONT) estan deshabilitadas. Flash total en fuentes: ~2.8 KB.

## Como revertir

- SANSSERIF_24 completa: en `fonts.h` comentar `SANSSERIF24_NUM` y descomentar `SANSSERIF24`;
  en `screen.cpp` cambiar las llamadas a `setFont()`.
- GLCDFONT: en `fonts.h`, descomentar `#define ENABLE_GLCDFONT`.
