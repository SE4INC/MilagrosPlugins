# MilagrosAfina

Afinador de voz (tipo autotune) **gratuito y de código abierto** (VST3) para FL Studio y
otros DAWs en PC. Pensado para artistas emergentes: todo se explica en notas (Do, Re, Mi...),
sin términos técnicos.

> **Versión 0.1 (fase 1).** Aún no probada en PC: es la primera versión del código.
> La precisión real se debe medir con voces reales (ver "Cómo probar la precisión").

## Qué hace

- **Detector de nota en tiempo real** (algoritmo YIN con refinamiento a la frecuencia
  original). En una voz limpia, el error esperado es de pocos centésimos de semitono.
  Con ruido o un micrófono muy sucio baja la precisión; el plugin lo avisa.
- **Corrección hacia la tonalidad elegida** (Do..Si + Mayor, Menor, Menor armónica,
  Pentatónicas, Blues o Cromática).
- **Velocidad:** 0 ms = efecto robótico (trap); más alto = más natural.
- **Cantidad:** cuánto se corrige.
- **Estilos rápidos** para principiantes: Natural, Pop moderno, Trap, Retoque suave.
- **Pantalla de notas:** celeste = tu voz, naranja = voz ya afinada, con las notas
  permitidas marcadas.
- **Funciona igual** grabando en vivo o afinando una voz ya grabada.

### Pestaña "Asesor" (hoja de consejos)

Analiza lo que cantas y te dice:

- Tu **rango de voz** y tu nota más cómoda
- La **tonalidad** en la que parece que cantas
- La **tonalidad de la instrumental** (si la envías al sidechain)
- Qué porcentaje de tus notas ya está afinado, y **qué Velocidad y Cantidad usar**
- Avisos: voz poco clara, señal saturada, volumen bajo
- Glosario para principiantes

Con botones para aplicar la tonalidad y los ajustes recomendados.

## Latencia

Unos 12-14 ms (se informa al DAW y FL Studio la compensa). Para cantar en vivo con
monitoreo, sirve; si molesta, afina después de grabar.

## Cómo compilarlo (Windows)

Necesitas (todo gratis): **Visual Studio 2022 Community** (con "Desarrollo para el
escritorio con C++"), **CMake** y **Git**. Dentro de la carpeta del proyecto:

```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

El plugin queda en `build\MilagrosAfina_artefacts\Release\VST3\MilagrosAfina.vst3`.
Copia esa carpeta a `C:\Program Files\Common Files\VST3`. También se crea una versión
`Standalone` para probar sin DAW.

## Usarlo en FL Studio

1. `Options > Manage plugins` y pulsa **Find plugins**.
2. Ponlo en la pista de tu voz, **después** de MilagrosLimpio y **antes** de EQ y compresor.
3. Elige la tonalidad de tu canción (o usa el Asesor) y un estilo rápido.
4. **Tonalidad desde la instrumental:** envía la instrumental al sidechain del plugin
   desde el mezclador, si tu versión de FL Studio lo permite. Si no aparece la opción,
   elige la tonalidad a mano.

## Cómo probar la precisión

1. Pon en un celular un afinador y una nota sostenida (por ejemplo La 440 Hz).
2. Canta la nota sostenida con MilagrosAfina y mira la pantalla: debe mostrar La con
   pocos centésimos de diferencia.
3. Prueba voces graves y agudas, con y sin ruido. Anota dónde falla y repórtalo.

## Ideas para contribuir

- Corrector de formantes (para correcciones grandes)
- Mejor detección con ruido; modo de baja latencia
- Presets, MIDI para elegir las notas objetivo
- Guardar el análisis dentro del proyecto
- Detección de acordes de la instrumental (no solo tonalidad)
- Traducciones

## Licencia

Usa **JUCE** bajo AGPLv3, por lo que MilagrosAfina se publica bajo **AGPL-3.0**.
Al crear el repositorio en GitHub, elige esa licencia y se añade el texto oficial.
