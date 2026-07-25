# Guia d'ús

## Requisits

- macOS 12 o posterior.
- OBS Studio 32.2 o una versió compatible.
- Una o més fonts de vídeo creades dins d'OBS.
- Balanç de blancs automàtic desactivat a les càmeres, si és possible.

## Instal·lació a macOS

Tanca OBS i copia `obs-ccu.plugin` a:

```text
~/Library/Application Support/obs-studio/plugins/obs-ccu.plugin
```

Torna a obrir OBS. El registre ha d'incloure:

```text
[CCU OBS] Loaded version 0.1.2
```

## Obrir i assignar càmeres

1. Obre **Eines → CCU OBS…**.
2. Utilitza el desplegable de cada canal per assignar-hi una font.
3. El CCU manté activa la font mentre la mostra, encara que no sigui visible a
   l'escena actual. No obre físicament la càmera una segona vegada.
4. Les assignacions es desen al directori de configuració del plugin.

## Càmera activa i controls

Selecciona un canal amb els botons `1`, `2`, `3` i `4`, o clicant directament
la seva previsualització. El marc daurat identifica la càmera activa.

Només aquesta càmera respon als controls compartits:

- **Vermell, verd i blau:** guany independent de cada canal.
- **Brillantor:** desplaçament digital del nivell de la imatge.
- **Contrast:** separació respecte del gris mitjà.
- **Gamma:** redistribució dels tons mitjans.
- **Saturació:** intensitat global del color.
- **Restablir:** recupera els valors neutres de la càmera activa.

## Seleccionar un blanc

1. Col·loca una carta blanca o gris neutra sota la il·luminació real.
2. Selecciona **Comptagotes**.
3. Mou el cursor sobre una previsualització.
4. La lupa mostra una ampliació aproximada de 18×. La retícula és
   semitransparent i el punt blanc central indica la mostra exacta.
5. Clica dins del vídeo. Els marges, bandes i controls no accepten clics.
6. El CCU calcula guanys RGB fixos i desactiva el comptagotes.

El blanc neutre elimina una dominant general, però no iguala necessàriament
tons de pell o colors específics entre sensors diferents.

## Vista ampliada

Selecciona **Lupa** per ocultar temporalment els altres tres canals i ampliar
la càmera activa. Torna a prémer el botó per recuperar els quatre visors.

## Persistència

Cada font rep un filtre `CCU OBS`. El filtre pertany a la font original, no a
una instància concreta d'una escena. La correcció afecta totes les escenes on
aparegui la font i es desa amb la col·lecció d'escenes.

El plugin no modifica altres filtres de color creats per l'usuari.

## Resolució de problemes

### No apareix al menú Eines

Comprova la ruta del bundle i busca `obs-ccu` al registre d'OBS.

### Els controls es mouen però la imatge no canvia

Confirma que el títol mostra 0.1.2 o posterior i que la font conté el filtre
`CCU OBS`. Reinicia OBS després de substituir un bundle.

### No hi ha imatge en un visor

Comprova que la font encara existeix i produeix vídeo. Torna-la a seleccionar
si ha estat eliminada o recreada.

### El comptagotes rebutja un clic

Només són vàlids els píxels dins del rectangle real de la font. Les bandes
afegides pel visor queden excloses deliberadament.
