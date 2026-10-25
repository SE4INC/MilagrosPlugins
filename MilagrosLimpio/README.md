# MilagrosLimpio

Plugin **gratuito y de código abierto** (VST3) para FL Studio y otros DAWs en PC.
Combina en un solo plugin una **reducción de ruido** y una **puerta de ruido inteligente**,
pensado para home studios con micrófonos económicos y cuartos sin tratamiento.

> Es parte de la familia MilagrosSound. Ningún plugin hace magia: con ruido muy fuerte
> puede aparecer un sonido "burbujeante". Para eso existe el control **Suavidad**.

## Qué hace

**1. Reducción de ruido (espectral)**
- Quita ruido constante como ventilador, PC, zumbido o siseo del micrófono.
- **Modo Automático:** el plugin sigue el ruido de fondo por sí solo.
- **Modo Perfil aprendido:** pulsa **Aprender ruido** mientras estás en silencio
  (2 o 3 segundos), vuelve a pulsar y elige este modo. Suele limpiar mejor.
- Controles: **Reducción** (cuánto quita) y **Suavidad** (evita el efecto burbujeante).

**2. Puerta de ruido inteligente**
- El umbral se ajusta solo: mide tu ruido de fondo de los últimos segundos.
- Solo escucha la zona de la voz (120 Hz - 4.5 kHz), así ignora graves y siseo.
- **Abre antes de la primera palabra** (lookahead), para no cortar el inicio de las frases.
- Con histéresis y tiempo de **Mantener**, para que no "tartamudee".
- Controles: **Sensibilidad**, **Profundidad** (cuánto baja cuando está cerrada),
  **Mantener**, **Liberación** y **Salida**.

La ventana muestra si la puerta está abierta o cerrada y el ruido de fondo estimado.

## Latencia

El plugin tiene una latencia de 1024 muestras (unos 23 ms). FL Studio la compensa
automáticamente. Para grabar con monitoreo en vivo, mejor úsalo después de grabar.

## Cómo compilarlo (Windows)

Necesitas (todo gratis): **Visual Studio 2022 Community** (con "Desarrollo para el
escritorio con C++"), **CMake** y **Git**. Luego, dentro de la carpeta del proyecto:

```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

El plugin queda en `build\MilagrosLimpio_artefacts\Release\VST3\MilagrosLimpio.vst3`.
Copia esa carpeta a `C:\Program Files\Common Files\VST3`.
También se crea una versión independiente en la carpeta `Standalone`.

## Usarlo en FL Studio

1. `Options > Manage plugins` y pulsa **Find plugins**.
2. Ponlo en la pista de tu voz, **antes** de EQ y compresor (y de MilagrosSound).
3. Ajusta Reducción hasta quitar el ruido sin que la voz suene "bajo el agua".
4. Activa la puerta y ajusta Sensibilidad según qué tan seguido hables o cantes.

## Ideas para contribuir

- Guardar el perfil de ruido dentro del proyecto de FL Studio
- Mejor estimador de ruido y reducción de "ruido musical"
- Modo de baja latencia (ventana de 512)
- Visualizador del espectro y del perfil de ruido
- Quitar eco de cuarto (de-reverb)
- Presets (ventilador, aire acondicionado, zumbido eléctrico)
- Traducciones

## Licencia

Usa **JUCE** bajo AGPLv3, por lo que MilagrosLimpio se publica bajo **AGPL-3.0**.
Al crear el repositorio en GitHub, elige esa licencia y se añade el texto oficial.
