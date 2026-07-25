# Projecte CCU OBS

## Idea

Crear un CCU (Camera Control Unit) dins d’OBS Studio per comparar i igualar
diverses càmeres des d’un únic panell.

El plugin mostraria simultàniament quatre càmeres —ampliable a més fonts— en un
multivisor. Des del mateix panell es podria seleccionar una càmera, mostrejar
una referència blanca o grisa i ajustar-ne el color mentre es compara amb la
resta.

No caldrien quatre programes OBS ni quatre instàncies. Un únic OBS reutilitzaria
les fonts de vídeo que ja estan obertes al projecte.

## Experiència d’ús proposada

1. Obrir el dock **CCU OBS**.
2. Assignar una font d’OBS a cadascun dels quatre visors.
3. Col·locar una carta blanca, grisa o de color sota la il·luminació real.
4. Escollir una càmera com a referència.
5. Utilitzar el comptagotes sobre cada visor per neutralitzar el blanc.
6. Comparar les quatre imatges simultàniament.
7. Afinar manualment cada càmera amb els controls del panell.
8. Desar els ajustaments amb la col·lecció d’escenes d’OBS.

Els canvis s’han de reflectir immediatament a la font real: programa,
previsualització, gravació i emissió.

## Esbós de la interfície

```text
┌───────────────────────┬───────────────────────┐
│ Càmera 1              │ Càmera 2              │
│                       │                       │
│     previsualització  │     previsualització  │
│                       │                       │
├───────────────────────┼───────────────────────┤
│ Càmera 3              │ Càmera 4              │
│                       │                       │
│     previsualització  │     previsualització  │
│                       │                       │
└───────────────────────┴───────────────────────┘

Càmera seleccionada: [ Càmera 2                 ]
Referència:          [ Càmera 1                 ]

[ Comptagotes ]  [ Igualar amb la referència ]  [ Restablir ]

Exposició / brillantor  ───────────────●────────
Contrast                 ──────────●─────────────
Gamma                    ─────────────●──────────
Temperatura              ─────────●──────────────
Tint verd/magenta        ───────────────●────────
Saturació                ───────────●────────────

R 1.000    G 0.963    B 1.087    Desviació: 2,4 %
```

## Controls inicials

- Balanç de blancs amb comptagotes.
- Intensitat de la correcció.
- Brillantor o exposició digital.
- Contrast.
- Gamma.
- Temperatura.
- Tint verd/magenta.
- Saturació.
- Restabliment individual.
- Copiar ajustaments d’una càmera a una altra.
- Activar o desactivar temporalment la correcció.

## Aplicació dels ajustaments

Cada càmera tindria un filtre propi gestionat pel CCU. El dock no hauria de
modificar silenciosament altres filtres de correcció de color creats per
l’usuari.

Associació proposada:

```text
Visor CCU 1 ──> Font de càmera A ──> Filtre CCU White Balance A
Visor CCU 2 ──> Font de càmera B ──> Filtre CCU White Balance B
Visor CCU 3 ──> Font de càmera C ──> Filtre CCU White Balance C
Visor CCU 4 ──> Font de càmera D ──> Filtre CCU White Balance D
```

El filtre hauria de funcionar encara que la font aparegui en diverses escenes.
Cal decidir si els ajustaments pertanyen a la font original o a una instància
concreta dins d’una escena. Per al primer prototip, és preferible aplicar-los a
la font original.

## Primera versió assumible

La primera prova no ha d’intentar fer una igualació completa de càmeres.

### MVP

- Dock amb quatre visors.
- Selector de font per a cada visor.
- Selecció d’una càmera activa.
- Comptagotes individual.
- Controls RGB, brillantor, contrast, gamma i saturació.
- Aplicació immediata sobre un filtre propi.
- Valors persistents.
- Botó de restabliment.
- Compatibilitat inicial amb macOS i Windows.

### Segona etapa

- Càmera de referència.
- Botó **Igualar amb la referència**.
- Histograma RGB.
- Forma d’ona.
- Vectorscopi.
- Indicador numèric de desviació entre càmeres.
- Presets per localització o condició d’il·luminació.
- Vista abans/després.

### Etapa avançada

- Reconeixement d’una carta de color.
- Correcció amb diversos pegats en lloc d’un únic blanc.
- Matriu de correcció de color per càmera.
- Comparació específica de tons de pell.
- Calibratge simultani o semiautomàtic de totes les càmeres.
- Compatibilitat amb superfícies de control MIDI, OSC o Stream Deck.

## Consideracions tècniques

- Reutilitzar les fonts ja capturades per OBS; no obrir físicament cada càmera
  una segona vegada.
- Renderitzar miniatures amb la GPU i limitar-ne la freqüència si cal.
- Evitar que el mateix filtre es corregeixi a si mateix durant el mostreig.
- Separar el dock de control del filtre de vídeo.
- Mantenir el processament de color a la GPU.
- Fer que l’ajust manual sigui reversible i persistent.
- Detectar fonts eliminades, reanomenades o no disponibles.
- Evitar bloquejos quan es tanqui OBS amb un comptagotes actiu.
- Mostrar clarament quan una càmera té activat el balanç de blancs automàtic;
  si no es pot detectar, avisar l’usuari perquè el desactivi.

## Blanc neutre i carta de color

Un blanc o gris neutre permet corregir una dominant general, però no garanteix
que dues càmeres reprodueixin igual tots els colors. Dues imatges poden tenir el
mateix blanc i continuar mostrant diferències en tons de pell, vermells, verds
o saturació.

Per això el projecte hauria d’avançar en dos nivells:

1. **Balanç ràpid:** comptagotes sobre blanc o gris.
2. **Igualació avançada:** carta de color amb diversos pegats.

## Decisions per a la propera sessió

1. El CCU serà una ampliació d’aquest repositori o un plugin independent?
2. Com seleccionarem les quatre fonts al dock?
3. La correcció s’aplicarà a la font original o a una còpia per escena?
4. Quins cinc o sis controls són imprescindibles per al primer prototip?
5. Necessitem quatre visors reals des del primer dia o podem començar amb dos?
6. Quina càmera i quina carta utilitzarem com a referència de les proves?
7. Volem començar només per macOS o mantenir macOS i Windows des de l’inici?

## Primer experiment recomanat

Construir un dock molt simple amb dos visors i dos selectors de font. En clicar
un visor, els controls RGB del filtre actual han d’editar la font corresponent
en temps real.

Si aquest circuit funciona sense duplicar captures ni provocar problemes de
rendiment, ampliar-lo a quatre càmeres serà principalment una feina
d’interfície i coordinació.
