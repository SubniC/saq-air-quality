# Font Optimization Guide

Guia de referencia para las optimizaciones realizadas en las fuentes LCD (Adafruit_mfGFX)
y como modificarlas en el futuro.

## Contexto

El firmware de SAQ-2 superaba el limite de APP_FLASH por 68 bytes. Las fuentes del LCD
eran el mayor consumidor de flash del proyecto (~11.5 KB con las 3 fuentes activas).
Se realizaron dos optimizaciones que liberaron ~8.9 KB solo en fuentes.

## Arquitectura de fuentes (Adafruit_mfGFX)

Cada fuente consta de dos arrays:
- **Bitmaps[]**: cabecera de 2 bytes (startChar, endChar) + datos de bitmap por caracter
- **Descriptors[]**: un `FontDescriptor` por caracter con `{width, height, offset}`

El rango de caracteres debe ser **contiguo** (no se pueden saltar caracteres). Para
caracteres no usados dentro del rango, se apuntan a un bloque vacio compartido.

Ficheros involucrados:
- `src/libs/fonts.h` — defines de compilacion, IDs de fuente, externs
- `src/libs/fonts.cpp` — datos de bitmap y descriptores
- `src/libs/Adafruit_mfGFX.cpp` — setFont() con switch/case por ID

## Optimizacion 1: SANSSERIF_24 -> SANSSERIF_24_NUM (ahorro ~5.8 KB)

### Que se hizo

Se creo un subset de SANSSERIF_24 que solo contiene los caracteres usados en la
pantalla grande de PM2.5:

| Caracter | ASCII | Uso |
|----------|-------|-----|
| espacio  | 0x20  | Espaciado |
| (        | 0x28  | PM stale: "(42)" |
| )        | 0x29  | PM stale: "(42)" |
| -        | 0x2D  | Valor no disponible: "--" |
| .        | 0x2E  | Decimal (si se usa) |
| 0-9      | 0x30-0x39 | Digitos del valor PM |

**Rango**: 0x20-0x39 (26 entradas en el descriptor)
**Caracteres con bitmap real**: 15
**Caracteres vacios** (!, ", #, $, %, &, ', *, +, ,, /): apuntan a bloque de 32 bytes de ceros con width=1

### Tamanos

| Fuente | Bitmap | Descriptores | Total |
|--------|--------|-------------|-------|
| SANSSERIF_24 (original) | 8002 B | 380 B | ~8382 B |
| SANSSERIF_24_NUM (subset) | 1186 B | 104 B | ~1290 B |
| **Ahorro** | | | **~7092 B** |

### Como revertir

En `fonts.h`, comentar `SANSSERIF24_NUM` y descomentar `SANSSERIF24`.
En `screen.cpp`, cambiar `SANSSERIF_24_NUM` por `SANSSERIF_24` en las llamadas a `setFont()`.

### Como anadir un caracter al subset

1. En `fonts.cpp`, buscar la seccion `#ifdef SANSSERIF24` (la fuente original, comentada)
2. Localizar la linea del caracter deseado (linea 420 + (ASCII - 0x20))
3. Copiar sus bytes al array `sansSerif_24ptNumBitmaps[]` en la seccion `#ifdef SANSSERIF24_NUM`
4. Actualizar el offset en `sansSerif_24ptNumDescriptors[]`
5. Si el caracter esta fuera del rango 0x20-0x39, ampliar el byte endChar en la cabecera
   y anadir descriptores vacios para los huecos

**Ejemplo**: anadir ':' (0x3A) requeriria cambiar la cabecera de `0x39` a `0x3A`,
copiar los bytes del ':' del original, y anadir una entrada al descriptor.

## Optimizacion 2: GLCDFONT opcional (ahorro ~3.1 KB)

### Que se hizo

GLCDFONT (256 chars, 0x00-0xFF, 5x8px) estaba siempre compilado como fallback por defecto
en Adafruit_mfGFX, aunque ningun `print()` en screen.cpp lo usaba explicitamente.

Se anadio una guarda `#ifdef ENABLE_GLCDFONT` en:
- `fonts.cpp`: alrededor de `glcdfontBitmaps[]` y `glcdfontDescriptors[]`
- `fonts.h`: alrededor de las declaraciones `extern`
- `Adafruit_mfGFX.cpp`: constructor y default case de `setFont()` caen a `CENTURY_8`
  cuando GLCDFONT esta deshabilitado

### Tamanos

| Componente | Bytes |
|-----------|-------|
| Bitmap (256 chars x 8 bytes + 2 header) | 2050 B |
| Descriptores (256 x 4 bytes) | 1024 B |
| **Total eliminado** | **~3074 B** |

### Como revertir

En `fonts.h`, descomentar `#define ENABLE_GLCDFONT`.

## Optimizacion descartada: subset de CENTURYGOTHIC8

### Por que no se hizo

CENTURYGOTHIC8 solo ocupa ~1486 bytes en total (1102 bitmap + 384 descriptores).
Un subset ahorraria ~500-700 bytes — insuficiente para justificar la complejidad
y el riesgo de olvidar un caracter usado en algun mensaje de pantalla.

Ademas, esta fuente se usa para texto variado (labels, unidades, mensajes de error,
estado de firmware) con un conjunto de caracteres mas dificil de auditar que los
digitos de SANSSERIF_24.

## Inventario de fuentes (estado actual)

| Fuente | Define | ID | Tamano | Estado |
|--------|--------|----|--------|--------|
| CENTURYGOTHIC8 | `CENTURYGOTHIC8` | CENTURY_8 (1) | ~1.5 KB | Activa (texto general) |
| SANSSERIF24_NUM | `SANSSERIF24_NUM` | SANSSERIF_24_NUM (9) | ~1.3 KB | Activa (digitos PM grandes) |
| SANSSERIF_24 | `SANSSERIF24` | SANSSERIF_24 (8) | ~8.4 KB | Deshabilitada (reemplazada por NUM) |
| GLCDFONT | `ENABLE_GLCDFONT` | GLCDFONT (4) | ~3.1 KB | Deshabilitada (no usada) |
| TIMESNEWROMAN8 | `TIMESNEWROMAN8` | TIMESNR_8 (0) | ~9.0 KB | Deshabilitada |
| ARIAL8 | `ARIAL8` | ARIAL_8 (2) | ~7.6 KB | Deshabilitada |
| COMICSANSMS8 | `COMICSANSMS8` | COMICS_8 (3) | ~8.9 KB | Deshabilitada |
| SANSSERIF_6 | `SANSSERIF6` | SANSSERIF_6 (6) | ~6.8 KB | Deshabilitada |
| SANSSERIF_12 | `SANSSERIF12` | SANSSERIF_12 (7) | ~18.0 KB | Deshabilitada |
| TESTFONT | `TESTFONT` | TEST (5) | ~15.7 KB | Deshabilitada |

**Flash actual en fuentes**: ~2.8 KB (CENTURYGOTHIC8 + SANSSERIF24_NUM)
