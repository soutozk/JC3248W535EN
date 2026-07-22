# Organizacao do SD card

O firmware monta o cartao em `/sd` e procura os arquivos abaixo. Use FAT32 e nomes simples, sem acentos.

```text
/sd
├── intro
│   ├── boot.gif       # primeira opcao da tela de inicializacao
│   ├── boot.png       # fallback do GIF, idealmente 480x320
│   └── logo.png       # logo da Home e fallback da intro
├── backgrounds
│   └── home.png       # foto de fundo da Home
├── gifs
│   ├── animacao-01.gif
│   ├── visualizer.gif
│   └── clipe.mjpeg
├── logo.png           # compatibilidade com a estrutura antiga
└── home_bg.png        # compatibilidade com a estrutura antiga
```

Prioridade da inicializacao: `intro/boot.gif`, `intro/boot.png`, `intro/logo.png`, `logo.png` e, por ultimo, o logo textual.

Na Home, o visor OEL usa `intro/logo.png` quando existir; se nao existir, tenta `logo.png` e depois volta para texto.

Para o fundo da Home, prefira uma imagem PNG de 480x320 em `backgrounds/home.png`. O nome antigo `home_bg.png` continua funcionando.

GIFs devem ficar em `gifs/`. A lista e atualizada pelo botao `SCAN SD`; tocar um `.gif` abre o player. Arquivos `.mjpeg` e `.mp4` aparecem como formatos ainda sem decoder no firmware atual.

Na tela MIDIA, escolha AUDIO para listar arquivos de `music/`. O player reproduz MP3 e WAV PCM assinado de 16 bits, mono ou estereo, de 8 a 48 kHz, pelo alto-falante I2S da placa.

Mantenha GIFs leves, idealmente em 320x240 ou 480x320, e ejete o cartao antes de reiniciar a placa.

As configuracoes tambem procuram imagens `.png` na raiz do
SD, em `intro/` e em `backgrounds/`. Na tela CONFIGURACOES, toque em
`PROXIMA FOTO` para escolher a imagem do boot ou da logo. A escolha, a cor da
interface e o brilho ficam salvos na memoria NVS do ESP32 e retornam depois de
desligar a placa; os arquivos selecionados precisam continuar no SD.
