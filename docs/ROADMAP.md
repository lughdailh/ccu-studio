# Estat i full de ruta

## Validat a 0.1.2

- Càrrega a OBS Studio 32.2 en Apple Silicon.
- Quatre visors de fonts existents.
- Finestra al 85% de la pantalla i visors 16:9.
- Selecció d'una càmera activa.
- Aplicació i persistència del filtre a la font.
- RGB, brillantor, contrast, gamma i saturació.
- Selector restringit al vídeo.
- Lupa i punt central.
- Prova automàtica de canvi real de píxels.

## Limitacions

- Interfície encara basada en components visuals estàndard.
- Sense histograma, waveform ni vectorscopi.
- Un sol blanc no iguala tota la resposta de color dels sensors.
- Sense detecció automàtica del balanç de blancs automàtic.
- Sense presets ni còpia explícita d'ajustos entre canals.
- Windows pendent de compilació i validació funcional.
- Paquet macOS de proves amb signatura ad hoc.

## Properes fases

### Interfície

- Reordenar controls i jerarquia visual.
- Mostrar millor valors i estat de cada canal.
- Adaptar-se a pantalles petites.
- Afegir activació temporal i còpia d'ajustos.

### Estètica CCU

- Panell inspirat en equips antics.
- Selectors rotatius, botons i indicadors propis.
- Textures subtils sense comprometre llegibilitat.
- Separació estricta entre estil i lògica.

### Mesura i igualació

- Histograma RGB, forma d'ona i vectorscopi.
- Càmera de referència i indicador de desviació.
- Comparació abans/després.
- Carta de color, diversos pegats i matriu de correcció.
- Comparació específica de tons de pell.

### Plataformes i control

- Empaquetatge i proves Windows x64.
- MIDI, OSC o Stream Deck.
