# CyberDeck Spotify Bridge

App Android para enviar a musica atual do Spotify para a ESP32 via BLE.

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

## Teste manual

O botao `ENVIAR MANUAL` continua disponivel para diagnostico. Ele envia apenas `Titulo;Artista` para confirmar que a conexao BLE esta viva.
