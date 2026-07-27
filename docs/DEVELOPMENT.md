# Desenvolupament

## Dependències

- CMake 3.28 o posterior.
- Compilador amb C++17.
- Fonts o SDK d'OBS amb `libobs` i `obs-frontend-api`.
- Qt 6 Widgets.
- Xcode per a macOS.

La versió 0.3.1 utilitza dependències universals d'OBS 31.1.1 i es valida en
temps d'execució amb OBS 32.2.

## Compilació macOS universal

```sh
cmake -S . -B build-universal -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -Dlibobs_DIR="/path/to/libobs/cmake" \
  -Dobs-frontend-api_DIR="/path/to/obs-frontend-api/cmake" \
  -DQt6_DIR="/path/to/Qt6/lib/cmake/Qt6" \
  -DCMAKE_PREFIX_PATH="/path/to/Qt6"
cmake --build build-universal
```

## Proves

```sh
ctest --test-dir build-universal --output-on-failure
```

`plugin-smoke` inicialitza libobs/OpenGL, carrega el mòdul i el shader, crea
una font sintètica, renderitza abans i després del filtre i comprova que els
píxels canvien.

Aquesta prova evita repetir el problema de 0.1.0: el mòdul carregava, però
OpenGL rebutjava una expressió del shader.

## Signatura local

```sh
xattr -cr build-universal/obs-ccu.plugin
codesign --force --deep --sign - build-universal/obs-ccu.plugin
codesign --verify --deep --strict build-universal/obs-ccu.plugin
```

És una signatura ad hoc. Una distribució pública requeriria Developer ID,
notarització i un instal·lador.

## Instal·lació

```text
~/Library/Application Support/obs-studio/plugins/obs-ccu.plugin
```

Tanca completament OBS abans de substituir el bundle.

## Compilació Windows

El workflow `.github/workflows/windows-build.yaml` compila Windows x64 amb
OBS 31.1.1 i les dependències oficials d'OBS. Cada push genera un artefacte;
els tags `vX.Y.Z` creen o actualitzen una GitHub Release.

El ZIP de Windows conté:

```text
obs-ccu/
  bin/64bit/obs-ccu.dll
  data/ccu-color.effect
  data/ccu-compare.effect
  data/icons/
  data/locale/
  data/CCUstudio.svg
  LICENSE
```

## Verificació manual

- Quatre fonts visibles sense duplicar captures.
- Canvi de canal actiu inequívoc.
- Controls reflectits al CCU i al programa d'OBS.
- Filtre persistent després de reiniciar.
- Selector inactiu fora del vídeo.
- Lupa, retícula i punt central alineats.
- Tancament segur amb el selector actiu.
