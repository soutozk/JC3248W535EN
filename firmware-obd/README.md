# Firmware OBD-II

Firmware independente para a JC3248W535EN. Ele anuncia `CyberDeck_OBD`, recebe telemetria do
app Android por BLE e oferece telas de RPM, velocidade, arrefecimento, painel geral,
configuracao de shift light e diagnostico.

## Compilar e gravar

```powershell
pio run -d firmware-obd
pio run -d firmware-obd -t upload
```

O ambiente usa ESP-IDF, LVGL 8.3.11, flash de 16 MB e PSRAM OPI de 8 MB. O limite de shift
light e salvo em NVS.

## Modo apresentacao

Quando o app Android nao esta conectado por BLE, o firmware entra automaticamente em modo
apresentacao e alimenta as telas com valores demonstrativos. Ao conectar o celular, os dados
ficticios sao descartados imediatamente e somente a telemetria recebida do app e exibida. Se
a conexao cair, o modo apresentacao volta automaticamente.

Para manter a apresentacao ativa mesmo com um cliente BLE conectado, altere temporariamente
`-D OBD_SIMULATION=1` nos `build_flags` de `platformio.ini`. O valor normal e `0`.

## Transporte BLE

- Servico: `f38a0001-82eb-4a73-a38c-ce98c9438012`
- Telemetria: `f38a0006-82eb-4a73-a38c-ce98c9438012`
- Estado: `f38a0007-82eb-4a73-a38c-ce98c9438012`

Os quadros usam little-endian, numero de sequencia, mascara de validade e CRC-16/CCITT. O
firmware descarta tamanhos, versoes, CRCs, sequencias e valores fora das faixas aceitas.
