# Organizacao do SD card

O firmware monta o cartao em `/sd` e procura os arquivos abaixo. Use FAT32 e nomes simples, sem acentos.

```text
/sd
├── intro
│   ├── boot.gif       # primeira opcao da tela de inicializacao
│   ├── boot.png       # fallback do GIF
│   └── logo.png       # fallback da imagem de intro
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

Para o fundo da Home, prefira uma imagem PNG de 480x320 em `backgrounds/home.png`. O nome antigo `home_bg.png` continua funcionando.

GIFs devem ficar em `gifs/`. A lista e atualizada pelo botao `SCAN SD`; tocar um `.gif` abre o player. Arquivos `.mjpeg` e `.mp4` aparecem como formatos ainda sem decoder no firmware atual.

Mantenha GIFs leves, idealmente em 320x240 ou 480x320, e ejete o cartao antes de reiniciar a placa.
