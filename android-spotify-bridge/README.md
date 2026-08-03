# CyberDeck Bridge

App Android para enviar a musica atual do Spotify e telemetria OBD-II para a ESP32 via BLE.

## OBD-II com ELM327

O celular funciona como ponte entre dois enlaces: conecta ao ELM327 por Bluetooth classico
(RFCOMM/SPP), consulta a ECU e transmite quadros binarios para o `CyberDeck_OBD` por BLE.

1. Grave o projeto `firmware-obd` na placa.
2. Nas configuracoes Bluetooth do Android, pareie previamente o ELM327.
3. Abra o app, conceda as permissoes Bluetooth e toque em `SCAN / CONECTAR` para conectar ao
   `CyberDeck_OBD`.
4. Selecione o adaptador na lista de dispositivos pareados e toque em `CONECTAR ELM327`.
5. Use `ATUALIZAR LOG OBD` para consultar os ultimos eventos de conexao e polling.

O app inicializa o ELM327, detecta o protocolo automaticamente, descobre os PIDs suportados,
faz polling adaptativo e reconecta com backoff quando o transporte cai. RPM, velocidade,
temperatura, carga, acelerador, tensao, combustivel, ar de admissao, MAP, MAF e tempo de motor
sao enviados quando suportados pela ECU.

## Como testar

1. Grave o firmware atualizado na ESP32.
2. Abra esta pasta (`android-spotify-bridge`) no Android Studio.
3. Compile e instale no celular.
4. Abra o app e permita Bluetooth.
5. Toque em `SCAN / CONECTAR`.
6. Toque em `ATIVAR ACESSO AO SPOTIFY` e habilite `CyberDeck Spotify Bridge`.
7. Volte para o app.
8. Abra o Spotify e toque uma musica.

O app le a notificacao do Spotify e envia automaticamente:

- Titulo/artista em `F38A0002-82EB-4A73-A38C-CE98C9438012`.
- Tamanho da capa em `F38A0004-82EB-4A73-A38C-CE98C9438012`.
- JPEG da capa em blocos em `F38A0005-82EB-4A73-A38C-CE98C9438012`.
- Comandos dos botoes da placa por notificacao em `F38A0003-82EB-4A73-A38C-CE98C9438012`.

A capa e enviada em blocos pequenos com confirmacao BLE. Isso e mais lento, mas evita JPEG incompleto na ESP32.

Os botoes usam a MediaSession do Spotify:

- `1`: proxima musica.
- `2`: musica anterior.
- `3`: play/pause.

Para a capa, o app tenta primeiro a notificacao e depois a MediaSession (`ALBUM_ART` / `ART`).

## Teste manual

O botao `ENVIAR MANUAL` continua disponivel para diagnostico. Ele envia apenas `Titulo;Artista` para confirmar que a conexao BLE esta viva.

## Testes locais

Execute `gradlew.bat test` no Windows. Os testes cobrem respostas ELM327, conversao de PIDs,
layout dos quadros BLE e CRC-16/CCITT.
