# MilagrosSound

Plugin vocal **gratuito y de código abierto** (VST3) para FL Studio y otros DAWs en PC.
Pensado para quienes graban con micrófonos económicos (como el BM800) y quieren una voz
más clara y profesional sin gastar en plugins caros.

> Ningún plugin convierte un micrófono barato en uno de estudio, pero una buena cadena
> vocal acerca mucho el resultado y ahorra horas de prueba y error.

## Qué hace

Cadena todo en uno, con pocos controles:

1. **Corte de graves** (según tipo de voz)
2. **Anti-barro** (reduce la zona encajonada de 200-400 Hz)
3. **Claridad** (presencia en 3-5 kHz)
4. **Calidez** (graves suaves + saturación ligera)
5. **Aire** (brillo en agudos)
6. **De-esser** (controla las "s" duras)
7. **Compresión suave**
8. **Mezcla** y **Salida**

Controles principales: **Mejorar voz** (intensidad general), **Tipo de voz**
(masculina, femenina, rap/urbano, locución) y **Micrófono** (BM800/económico, gama
media, gama alta).

### Pestaña "Asesor"

Escucha tu voz unos segundos y te da consejos (nivel muy bajo, voz encajonada, falta de
claridad, "eses" duras, falta de aire...). El botón **Aplicar sugerencias** ajusta las
perillas por ti. Los umbrales son un punto de partida: ¡ayúdanos a mejorarlos!

## Cómo compilarlo (Windows)

Necesitas instalar (todo gratis):

- **Visual Studio 2022 Community** (marca "Desarrollo para el escritorio con C++")
- **CMake** (https://cmake.org)
- **Git** (https://git-scm.com)

Luego, en una terminal dentro de la carpeta del proyecto:

```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

La primera vez descarga JUCE automáticamente (tarda unos minutos).

El plugin queda en:

```
build\MilagrosSound_artefacts\Release\VST3\MilagrosSound.vst3
```

Cópialo a `C:\Program Files\Common Files\VST3` (la carpeta .vst3 completa).

También se crea una versión independiente para probar sin DAW:
`build\MilagrosSound_artefacts\Release\Standalone\MilagrosSound.exe`

## Usarlo en FL Studio

1. `Options > Manage plugins`, pulsa **Find plugins** (Start scan).
2. Pon MilagrosSound en un slot del mezclador, en la pista de tu voz.
3. Elige tipo de voz y micrófono, y sube **Mejorar voz** hasta que suene bien.
4. Abre la pestaña **Asesor** mientras cantas para ver consejos.

## Ideas para contribuir

- Suavizar los cambios de parámetros (evitar clics al mover perillas)
- Corte de graves de 24 dB/octava
- De-esser más preciso (banda ajustable)
- Sobremuestreo en la saturación
- Analizador de espectro visual en el Asesor
- Detección de ruido de fondo y sugerencia de puerta de ruido
- Mejorar los umbrales del Asesor con grabaciones reales de distintos micrófonos
- Presets y más tipos de voz y micrófono
- Traducciones (inglés, portugués...)

## Licencia

Este proyecto usa **JUCE** bajo AGPLv3, por lo que MilagrosSound se publica bajo
**AGPL-3.0**. Al crear el repositorio en GitHub, elige la licencia "GNU Affero General
Public License v3.0" y se añadirá el texto oficial automáticamente.
