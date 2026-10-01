# TransientLock Comp v1.1

Compresor con proteccion de transientes (VST3 / AU / AAX / Standalone) hecho con JUCE 8 y C++17.

## Compilar

Requisitos: CMake >= 3.22, compilador C++17 e internet (CMake descarga JUCE 8.0.6).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

- **Windows:** Visual Studio 2022. Salida en `build/TransientLockComp_artefacts/Release/VST3`.
  Por defecto copia el plugin a `C:\Program Files\Common Files\VST3` (usa `-DTLC_COPY_AFTER_BUILD=OFF` para evitarlo).
- **macOS:** Xcode. VST3 + AU (universal arm64/x86_64).
- **Linux:** `sudo apt install libasound2-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev`
- **AAX:** `-DAAX_SDK_PATH=/ruta/AAX_SDK` (SDK de Avid). Para Pro Tools release hace falta firma PACE/iLok.

Antes de distribuir cambia `COMPANY_NAME`, `PLUGIN_MANUFACTURER_CODE` y `BUNDLE_ID` en `CMakeLists.txt`.

## GitHub Actions

`.github/workflows/build.yml` compila en Windows, macOS y Linux, firma ad-hoc en macOS,
valida el VST3 con **pluginval** (strictness 5) y sube los artefactos. Si haces push de un tag
`vX.Y.Z` (ej. `git tag v1.1.0 && git push --tags`) crea ademas un **GitHub Release** con los binarios.

### Firma de codigo (opcional, para distribuir)
- **Windows:** firma los `.vst3` con `signtool` y un certificado de code signing (guardalo en Secrets).
- **macOS:** firma con tu "Developer ID Application" y notariza con `xcrun notarytool`
  (Apple Developer Program, 99 USD/ano). Sin esto, el usuario debe ejecutar `xattr -cr`.

## Controles

| Control | Funcion |
|---|---|
| Threshold / Ratio / Knee | Curva del compresor |
| Attack / Release | Ballistics |
| **Transient Protection** | Cuanto se protege el ataque (>75 % anade hasta +2 dB de realce) |
| Body Compression | Cantidad de compresion sobre el cuerpo |
| SC HPF | Filtro pasa-altos 2o orden en el detector (20 Hz = off) |
| Sensitivity | Sensibilidad del detector de transientes |
| Input / Makeup / Mix / Output | Ganancias y compresion paralela |
| Oversampling | Off / 2x / 4x (FIR linear-phase; reporta latencia al DAW) |
| Presets | 10 presets de fabrica (voz, bateria, bajo, guitarra, bus...) |

La ventana es redimensionable (se recuerda el tamano en el estado del plugin).

## Como funciona

La separacion transient/sustain se hace en el **dominio de ganancia**:

1. Detector diferencial: envolvente rapida (0.5/10 ms) vs lenta (30/200 ms) -> `t` en 0..1 (smoothstep).
2. Compresor feed-forward en dB con soft-knee (detector stereo-linked, con HPF opcional).
3. Reduccion aplicada = `GR x BodyCompression x (1 - Protection x t)`.
4. El detector del compresor baja hasta 6 dB durante el transiente (evita tirones posteriores).

Latencia: 0 muestras con Oversampling Off; con 2x/4x se reporta la del oversampler.

## Medidores
**BODY** (reduccion del cuerpo), **APPLIED** (reduccion real aplicada), **TRANS** (actividad del detector).
