# Arquitectura

## Visió general

CCU OBS és un plugin natiu C++17 format per tres parts:

```text
Finestra Qt
    │
    ├── 4 × ObsDisplayWidget ──> fonts existents de libobs
    │
    └── controls compartits ──> settings del filtre actiu
                                      │
                                      ▼
                              filtre GPU CCU OBS
```

No es creen quatre instàncies d'OBS ni es tornen a obrir dispositius. Cada
visor conserva una referència a una font existent i la renderitza amb
`obs_source_video_render`.

## Integració frontend

`src/ccu-filter.cpp` registra el filtre `ccu_obs_color_filter`, l'acció
**CCU OBS…** al menú d'eines i el tancament segur de la finestra quan OBS surt
o canvia de col·lecció. La finestra és no modal.

## Visors

`ObsDisplayWidget` crea un `obs_display_t` sobre la superfície nativa de Qt.
Cada visor:

- manté una caixa 16:9;
- centra la font sense deformar-la;
- incrementa el comptador `showing` perquè fonts fora de l'escena continuïn
  generant vídeo;
- converteix coordenades de ratolí a coordenades normalitzades;
- rebutja punts fora del rectangle real de vídeo.

La superfície té branques petites per macOS i Windows, però la renderització i
la selecció són comunes.

## Filtre de color

El shader `data/ccu-color.effect` s'executa a la GPU. L'ordre actual és:

1. guanys RGB;
2. contrast i brillantor;
3. gamma;
4. saturació;
5. limitació al rang visible.

Els paràmetres es desen als settings del filtre de libobs.

## Comptagotes

El selector no utilitza `NSColorSampler`, captures de pantalla ni coordenades
globals. Quan l'usuari clica:

1. es valida el rectangle real de vídeo;
2. es desactiva temporalment el filtre CCU per evitar autocorrecció;
3. libobs renderitza la font a una textura SDR;
4. la textura es transfereix a una superfície llegible per CPU;
5. es calcula la mitjana d'una regió de 7 × 7 píxels;
6. es calculen els guanys RGB;
7. es restaura i actualitza el filtre.

La lupa es renderitza al mateix display d'OBS. La imatge ampliada, la retícula
i la coordenada mostrejada comparteixen el mateix sistema de coordenades.

## Persistència

- **Correcció:** settings del filtre dins de la col·lecció d'escenes.
- **Assignacions:** `plugin_config/obs-ccu/assignments.json`.

La correcció pertany a la font original i afecta totes les seves aparicions.

## Fils i GPU

Qt gestiona la interfície. Els callbacks de display s'executen al context
gràfic d'OBS. Les coordenades de la lupa es comparteixen amb valors atòmics.
Les operacions puntuals del selector utilitzen `obs_enter_graphics` i
`obs_leave_graphics`.
