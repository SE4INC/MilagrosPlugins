# MilagrosMaster

Master fácil, **gratuito y de código abierto** (VST3), pensado para géneros urbanos:
trap, hip hop, drill, reggaeton, pop, R&B, rock y acústico.

> **Versión 0.1, aún sin probar en PC.** El master no arregla una mezcla mal hecha.
> Antes de masterizar, deja tu mezcla con picos de unos -6 dB.

## Qué hace

Cadena: filtro de graves inútiles → EQ (Graves y Brillo) → compresor "pegamento" →
calidez (saturación suave) → ancho estéreo (con graves siempre centrados) → Intensidad →
limitador con anticipación (no recorta).

- **Estilos rápidos:** Trap, Hip hop / Boom bap, Drill, Reggaeton / Dembow, Pop,
  R&B / Pop suave, Rock, Acústico / Voz. Cada uno ajusta todo y el objetivo de volumen.
- **Objetivo de volumen:** Streaming (-14 LUFS), Redes (-12), Fuerte urbano (-9),
  Club (-8), Suave (-16). Son valores orientativos: las plataformas pueden cambiar sus reglas.
- **Medidores:** volumen del tema (LUFS integrado), volumen ahora, pico, reducción del
  limitador y fase estéreo.
- **Comparar justo:** escucha el original con el **mismo volumen** que el master.
- **Pestaña Asesor:** te dice si te falta o te sobra volumen, si la mezcla llega muy fuerte,
  si el limitador trabaja demasiado, si se escucha mal en mono, si hay demasiados o pocos
  graves, y cómo exportar. Botón para **ajustar el volumen automáticamente**.

## Cómo usarlo

1. Ponlo en el canal **master** de FL Studio, al final de la cadena.
2. Elige tu género en "Estilo".
3. Reproduce el tema completo, de principio a fin.
4. Abre el Asesor y sigue los consejos. Si dice que falta volumen, pulsa
   "Ajustar volumen automáticamente" y vuelve a reproducir (a veces hace falta dos veces).
5. Activa "Comparar con el original" para comprobar que realmente mejoró.

## Latencia

Unos 3 ms (el limitador con anticipación). FL Studio la compensa.

## Límites conocidos

- El limitador no mide "picos reales" (true peak): por eso el Techo por defecto es -1 dB.
- La saturación no usa sobremuestreo.
- El Asesor da guías, no verdades absolutas: confía en tus oídos.

## Ideas para contribuir

- Limitador de picos reales (true peak) con sobremuestreo
- Comparar contra una canción de referencia
- Más estilos (afrobeats, dancehall, cumbia urbana...)
- EQ multibanda y control de graves por bandas
- Guardar las mediciones en el proyecto

## Licencia

Usa **JUCE** bajo AGPLv3, por lo que MilagrosMaster se publica bajo **AGPL-3.0**.
