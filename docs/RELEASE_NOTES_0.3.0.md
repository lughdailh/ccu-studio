# CCU OBS 0.3.0

Versió de proves per a macOS universal (Apple Silicon i Intel) i Windows x64.

## Novetats

- Interfície CCU renovada amb logotip, icones pròpies i controls circulars.
- Mosaic 2×2 de visors 16:9 amb separació d’1 píxel.
- Assignació de fonts amb clic dret i ajuda visual als visors buits.
- Histograma RGB, waveform i vectorscopi en temps real.
- Referència congelada simultània dels tres scopes.
- Comparació partida original/correcció només dins la previsualització activa.
- Comptagotes bloquejat a la càmera activa, amb lupa i punt de mostra precís.
- Mode lupa que ocupa exactament el rectangle del mosaic sense moure controls.
- Finestra 5:3 adaptativa, centrada i limitada al 85% de l’àrea útil.
- Interfície en català i anglès.

## Instal·lació

### macOS

Tanca OBS, descomprimeix el ZIP i copia `obs-ccu.plugin` a:

```text
~/Library/Application Support/obs-studio/plugins/
```

La build porta signatura ad hoc i no està notaritzada.

### Windows

Tanca OBS, descomprimeix el ZIP i copia la carpeta `obs-ccu` a:

```text
%APPDATA%\obs-studio\plugins\
```

## Ús bàsic

Obre **Eines → CCU OBS…**, fes clic dret sobre cada visor per assignar-hi una
font i selecciona la càmera activa amb els botons `1`–`4`.

## Compatibilitat

- OBS Studio 32.2 o compatible.
- macOS 12 o posterior.
- Windows 10/11 x64.
