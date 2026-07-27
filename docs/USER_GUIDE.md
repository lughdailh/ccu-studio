# Guia d'ús

## Requisits

- macOS 12 o posterior, o Windows 10/11 x64.
- OBS Studio 32.2 o una versió compatible.
- Una o més fonts de vídeo creades dins d'OBS.
- Balanç de blancs automàtic desactivat a les càmeres, si és possible.

## Instal·lació a macOS

Tanca OBS i copia `ccu-studio.plugin` a:

```text
~/Library/Application Support/obs-studio/plugins/ccu-studio.plugin
```

Torna a obrir OBS. El registre ha d'incloure:

```text
[CCU Studio] Loaded version 0.3.2
```

## Instal·lació a Windows

1. Tanca completament OBS.
2. Descomprimeix el paquet `ccu-studio-0.3.2-windows-x64.zip`.
3. Copia la carpeta `ccu-studio` sencera a:

```text
%APPDATA%\obs-studio\plugins\
```

El resultat ha de contenir
`%APPDATA%\obs-studio\plugins\ccu-studio\bin\64bit\ccu-studio.dll` i
`%APPDATA%\obs-studio\plugins\ccu-studio\data\`.
Torna a obrir OBS i busca `CCU Studio` al menú **Eines**.

## Obrir i assignar càmeres

1. Obre **Eines → CCU Studio…**.
2. Fes clic dret sobre un visor i escull la seva font al menú contextual.
3. El CCU manté activa la font mentre la mostra, encara que no sigui visible a
   l'escena actual. No obre físicament la càmera una segona vegada.
4. La font actual apareix marcada al menú. Escull **Sense font** per
   desassignar-la.
5. Les assignacions es desen al directori de configuració del plugin.

Quan un visor està buit, mostra un símbol `+` i la indicació de fer-hi clic
dret per seleccionar una font.

## Càmera activa i controls

Selecciona un canal amb els botons `1`, `2`, `3` i `4`, o clicant directament
la seva previsualització. El marc daurat identifica la càmera activa.

Només aquesta càmera respon als controls compartits:

- **Vermell, verd i blau:** guany independent de cada canal.
- **Brillantor:** desplaçament digital del nivell de la imatge.
- **Contrast:** separació respecte del gris mitjà.
- **Gamma:** redistribució dels tons mitjans.
- **Saturació:** intensitat global del color.
- **Original:** activa una pantalla partida original/correcció exclusivament
  al visor de la càmera activa. No altera el programa ni la gravació.
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
la càmera activa. La vista ampliada ocupa exactament el mateix rectangle que
el mosaic 2×2, de manera que els controls no es desplacen. Torna a prémer el
botó per recuperar els quatre visors.

## Referència congelada dels scopes

1. Selecciona la càmera que utilitzaràs com a referència.
2. Prem **Freeze** a la barra inferior d'eines.
3. El CCU captura simultàniament l'histograma RGB, el waveform i el
   vectorscopi. La captura apareix en ambre amb l'indicador `REF`.
4. Selecciona una altra càmera. Les seves dades continuen actualitzant-se en
   directe damunt de la mateixa referència congelada.
5. Canvia lliurement entre les tres pestanyes: totes conserven la captura del
   mateix fotograma.
6. Torna a prémer **Freeze** per eliminar la referència.

## Comparar original i correcció

Selecciona **Original** a la barra inferior. El visor actiu queda partit amb
l'original a l'esquerra i la imatge corregida a la dreta; una línia central
marca el tall. En canviar de càmera, la comparació segueix només la nova
càmera activa. És una operació de previsualització i no desactiva ni modifica
el filtre aplicat a la sortida d'OBS.

La reconstrucció de l'original és exacta mentre la correcció no hagi retallat
informació. Els píxels que ja hagin arribat a blanc o negre absolut no es
poden recuperar completament.

## Persistència

Cada font rep un filtre `CCU Studio`. El filtre pertany a la font original, no a
una instància concreta d'una escena. La correcció afecta totes les escenes on
aparegui la font i es desa amb la col·lecció d'escenes.

El plugin no modifica altres filtres de color creats per l'usuari.

## Resolució de problemes

### No apareix al menú Eines

Comprova la ruta del bundle i busca `ccu-studio` al registre d'OBS.

### Els controls es mouen però la imatge no canvia

Confirma que el títol mostra 0.3.2 o posterior i que la font conté el filtre
`CCU Studio` (o `CCU OBS` si prové d'una versió anterior). Reinicia OBS després
de substituir un bundle.

### No hi ha imatge en un visor

Comprova que la font encara existeix i produeix vídeo. Torna-la a seleccionar
si ha estat eliminada o recreada.

### El comptagotes rebutja un clic

Només són vàlids els píxels dins del rectangle real de la font. Les bandes
afegides pel visor queden excloses deliberadament.

## Instruccions i suport

El botó discret **Instruccions**, situat a la cantonada inferior esquerra,
obre una guia integrada en l'idioma actiu d'OBS. Al peu d'aquesta finestra hi
ha el botó **Donar suport**, que obre PayPal; la donació és voluntària i ajuda
a desenvolupar més eines.

## Llicència i crèdits

CCU Studio es distribueix sota `GPL-2.0-or-later`. El desenvolupament ha
comptat principalment amb l'assistència d'OpenAI Codex/ChatGPT i Claude, sota
direcció, proves i revisió humanes. Aquesta menció no implica afiliació ni
aval d'OpenAI o Anthropic.

Un projecte de **Lluís Bartra Homedes** per a **Moiz i Bartra Produccions,
SL** i **El Català Emprenyat** ([emprenyat.cat](https://emprenyat.cat)).
Software ideat i creat íntegrament a Catalunya.

Web del projecte: [emprenyat.cat/obs/CCUstudio](https://emprenyat.cat/obs/CCUstudio)
