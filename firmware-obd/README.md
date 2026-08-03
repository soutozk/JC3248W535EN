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

## Simulacao sem veiculo

Para exercitar as telas sem Android, ELM327 ou ECU, adicione temporariamente
`-D OBD_SIMULATION=1` aos `build_flags` de `platformio.ini`. O valor normal e `0`.

## Transporte BLE

- Servico: `f38a0001-82eb-4a73-a38c-ce98c9438012`
- Telemetria: `f38a0006-82eb-4a73-a38c-ce98c9438012`
- Estado: `f38a0007-82eb-4a73-a38c-ce98c9438012`

Os quadros usam little-endian, numero de sequencia, mascara de validade e CRC-16/CCITT. O
firmware descarta tamanhos, versoes, CRCs, sequencias e valores fora das faixas aceitas.
