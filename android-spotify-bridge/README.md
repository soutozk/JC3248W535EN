# CyberDeck Spotify Bridge

App Android simples para testar a ponte BLE com a ESP32.

## Primeiro teste

1. Grave o firmware atualizado na ESP32.
2. Abra esta pasta (`android-spotify-bridge`) no Android Studio.
3. Compile e instale no celular.
4. Ative Bluetooth e permissões do app.
5. Toque em `SCAN / CONECTAR`.
6. Depois de conectado, preencha titulo e artista.
7. Toque em `ENVIAR PARA ESP32`.

O app procura o dispositivo BLE `CyberDeck_Spotify` e escreve `Titulo;Artista` na característica:

`F38A0002-82EB-4A73-A38C-CE98C9438012`

## Observação

Esta versão ainda não lê automaticamente o Spotify. Ela serve para confirmar que um app Android próprio consegue enviar dados para a tela.
