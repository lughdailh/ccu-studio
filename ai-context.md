# AI Context — CCU OBS

## Estat actual

- Projecte privat: `lughdailh/obs-ccu`.
- Versió: **0.3.0**.
- Plugin natiu d’OBS, Qt 6 + libobs, compatible amb macOS i Windows x64.
- OBS de referència: 32.2; SDK/dependències de compilació: OBS 31.1.1.
- Finestra no modal oberta des de **Eines → CCU OBS…**.

## Funcionalitat

- Mosaic 2×2 de fins a quatre fonts d’OBS, caixes 16:9 i separació d’1 píxel.
- Assignació de font amb clic dret; els visors buits mostren una ajuda grisa.
- Una sola càmera activa, marcada amb una vora daurada interior d’1 píxel.
- Controls compartits: RGB, brillantor, contrast, gamma i saturació.
- Filtre GPU `CCU OBS` persistent aplicat directament a la font.
- Comptagotes restringit exclusivament a la càmera activa i al rectangle real
  del vídeo; lupa de mostra amb retícula i punt central.
- Botó **Lupa**: la càmera activa ocupa exactament el rectangle del mosaic 2×2,
  sense moure els controls.
- Histograma RGB, waveform i vectorscopi de la càmera activa.
- **Congelar** captura simultàniament els tres scopes com a referència ambre.
- **Original** mostra una comparació partida només a la previsualització
  activa; no altera programa, gravació ni emissió.

## Interfície

- Composició 5:3 amb logotip `material/CCUOBS_TTL.png`.
- Mida inicial: 85% de l’àrea útil, centrada; màxim 1650×990 píxels lògics.
- Botons circulars blaus de càmera i eines; icones blanques a
  `material/icons/`.
- Pestanyes dels scopes sota el visor.
- Captura oficial: `docs/images/ccu-obs-0.3.0.png`.
- Idiomes: català (`ca-ES`, reserva) i anglès (`en-US`).

## Fitxers clau

- `src/ccu-window.cpp`: interfície, geometria, assignacions i controls.
- `src/obs-display-widget.cpp`: superfícies OBS, comptagotes, lupa i comparació.
- `src/ccu-filter.cpp`: registre del plugin i filtre GPU.
- `src/scope-data.*`, `src/scope-widget.*`: càlcul i dibuix dels scopes.
- `data/ccu-color.effect`: correcció de color.
- `data/ccu-compare.effect`: reconstrucció visual de l’original.
- `.github/workflows/windows-build.yaml`: build, tests, paquet i Release Windows.

## Compilació i validació

```sh
cmake -S . -B build-release
cmake --build build-release -j 8
ctest --test-dir build-release --output-on-failure
```

- `scope-data-tests`: proves dels instruments.
- `plugin-smoke` (macOS): càrrega real del mòdul i shader.
- macOS distribuït com a bundle universal arm64+x86_64 amb signatura ad hoc.
- Windows empaquetat com
  `obs-ccu/bin/64bit/obs-ccu.dll` + `obs-ccu/data/`.

## Decisions i límits importants

- No deformar mai el vídeo; preservar la proporció declarada per la font.
- No reparentar les superfícies natives d’OBS durant zoom o resize.
- No permetre que el comptagotes actuï sobre una càmera no seleccionada.
- La reconstrucció d’**Original** no pot recuperar píxels ja retallats a blanc
  o negre pel filtre.
- A macOS la finestra es porta al davant mitjançant AppKit.
- La build macOS encara no està notaritzada; és per a proves.

## Publicació 0.3.0 completada

- Commit principal: `411d5ce` (`Release CCU OBS 0.3.0`).
- Tag i Release: `v0.3.0`.
- CI Windows del tag: run `30305046685`, completament verd.
- Assets verificats descarregant-los de la Release:
  - `obs-ccu-0.3.0-macos-universal.zip`
    - SHA-256: `5b937eb8dfe7686f5018646c8bf186526aed6c78b73e14cb7551c3f9c8e59d91`
  - `obs-ccu-0.3.0-windows-x64.zip`
    - SHA-256: `b6d8e4d34bcd9cbe402d042b62535f687ee13fd0c72b7d0a1d90591472a907d3`
- Documentació d’ús i notes de versió disponibles en català i anglès.
- Captura oficial publicada al README.
