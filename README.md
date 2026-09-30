# TransientLock Comp

Compresor con protección de transientes (VST3 / AU / AAX / Standalone) hecho con JUCE 8 y C++17.

## Compilar

Requisitos: CMake ≥ 3.22, compilador C++17 y conexión a internet (CMake descarga JUCE 8.0.6 automáticamente).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

- **Windows:** Visual Studio 2022. Los `.vst3` quedan en `build/TransientLockComp_artefacts/Release/VST3`
  (y se copian a `C:\Program Files\Common Files\VST3`; ejecuta la terminal como administrador o desactiva
  `COPY_PLUGIN_AFTER_BUILD`).
- **macOS:** Xcode. Se generan VST3 + AU (universal arm64/x86_64).
- **Linux:** instala antes las dependencias de JUCE:
  `sudo apt install libasound2-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev`
- **AAX:** descarga el AAX SDK de Avid y añade `-DAAX_SDK_PATH=/ruta/AAX_SDK`.
  Para usarlo en Pro Tools release necesitas firmarlo con PACE/iLok.

Antes de distribuir cambia `COMPANY_NAME`, `PLUGIN_MANUFACTURER_CODE` y `BUNDLE_ID` en `CMakeLists.txt`.

## Cómo funciona

La separación transient/sustain se hace en el **dominio de ganancia**, no con filtros:

1. `TransientDetector`: envolvente rápida (0.5 / 10 ms) vs. lenta (30 / 200 ms). La diferencia en dB,
   pasada por un smoothstep (2.5 → 9 dB), da `t` en 0..1 (nivel-independiente, con gate a -80 dBFS).
2. Compresor feed-forward en dB con soft-knee y ballistics attack/release; detección stereo-linked.
3. Reducción aplicada = `GR × BodyCompression × (1 − Protection × t)`.
   Además el detector del compresor se baja hasta 6 dB durante el transiente para evitar
   que el compresor "cargue" con el golpe y provoque un tirón posterior.
4. Con Protection > 75 % se añade un realce de hasta +2 dB sobre el transiente.
5. Mix paralelo, Makeup, Output y Bypass con rampas de 10–20 ms (sin clicks).

Latencia: **0 muestras** (no hay crossover ni lookahead). Sin fase ni smearing porque nunca se suman bandas.

## Medidores

- **BODY:** reducción que sufre el cuerpo (sin protección).
- **APPLIED:** reducción realmente aplicada (la diferencia con BODY es lo que protege el transiente).
- **TRANS:** actividad del detector.

## Ideas para siguiente versión

Sidechain HPF, oversampling opcional para attacks < 1 ms, lookahead opcional, sensibilidad del detector,
presets y GUI escalable.
