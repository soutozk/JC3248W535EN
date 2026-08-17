package com.cyberdeck.spotifybridge.obd;

import android.bluetooth.BluetoothDevice;
import android.content.Context;

import com.cyberdeck.spotifybridge.BleBridge;
import com.cyberdeck.spotifybridge.obd.blebridge.ObdBlePacketEncoder;
import com.cyberdeck.spotifybridge.obd.connection.Elm327Connection;
import com.cyberdeck.spotifybridge.obd.diagnostics.ObdLogBuffer;
import com.cyberdeck.spotifybridge.obd.elm327.Elm327Initializer;
import com.cyberdeck.spotifybridge.obd.models.ObdConnectionState;
import com.cyberdeck.spotifybridge.obd.models.ObdDtc;
import com.cyberdeck.spotifybridge.obd.models.ObdPid;
import com.cyberdeck.spotifybridge.obd.models.ObdResult;
import com.cyberdeck.spotifybridge.obd.parser.Elm327Parser;
import com.cyberdeck.spotifybridge.obd.polling.ObdPollingScheduler;
import com.cyberdeck.spotifybridge.obd.repository.ObdRepository;

import java.io.IOException;
import java.util.EnumSet;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

public final class ObdManager implements ObdPollingScheduler.Listener {
    public interface Listener {
        void onObdStatus(String status);
        default void onObdData(ObdRepository.Snapshot snapshot) {}
        default void onDtcData(List<ObdDtc> codes) {}
    }

    private final BleBridge ble;
    private final ObdLogBuffer logs = new ObdLogBuffer(250);
    private final Elm327Connection elm;
    private final Elm327Parser parser = new Elm327Parser();
    private final Elm327Initializer initializer;
    private final ObdRepository repository = new ObdRepository();
    private final ObdBlePacketEncoder encoder = new ObdBlePacketEncoder();
    private final ScheduledExecutorService control = Executors.newSingleThreadScheduledExecutor();
    private final ScheduledExecutorService transmitter = Executors.newSingleThreadScheduledExecutor();
    private ObdPollingScheduler scheduler;
    private volatile ObdConnectionState state = ObdConnectionState.DISCONNECTED;
    private volatile String protocol = "";
    private volatile int latencyMs;
    private volatile int timeouts;
    private volatile int lastError;
    private volatile int packetSequence;
    private volatile long lastSentRepositorySequence = -1;
    private volatile long lastTelemetrySentAtMs;
    private volatile long lastStatusSentAtMs;
    private volatile BluetoothDevice selectedDevice;
    private volatile List<ObdDtc> dtcs = Collections.emptyList();
    private volatile boolean stopped;
    private volatile int reconnectAttempt;
    private Listener listener;

    public ObdManager(Context context, BleBridge ble) {
        this.ble = ble;
        this.elm = new Elm327Connection(context, logs);
        this.initializer = new Elm327Initializer(logs);
        transmitter.scheduleWithFixedDelay(this::sendFrames, 100, 50, TimeUnit.MILLISECONDS);
    }

    public void setListener(Listener listener) { this.listener = listener; }
    public ObdConnectionState getState() { return state; }
    public ObdLogBuffer logs() { return logs; }
    public ObdRepository.Snapshot snapshot() { return repository.snapshot(System.currentTimeMillis()); }
    public List<ObdDtc> dtcs() { return dtcs; }

    public void readDtc() {
        control.execute(this::readDtcInternal);
    }

    public void connect(BluetoothDevice device) {
        selectedDevice = device;
        stopped = false;
        reconnectAttempt = 0;
        control.execute(this::connectInternal);
    }

    public void disconnect() {
        stopped = true;
        if (scheduler != null) scheduler.stop();
        elm.close();
        dtcs = Collections.emptyList();
        Listener current = listener;
        if (current != null) current.onDtcData(dtcs);
        setState(ObdConnectionState.DISCONNECTED, "OBD desconectado");
    }

    public void shutdown() {
        disconnect();
        transmitter.shutdownNow();
        control.shutdownNow();
    }

    private void connectInternal() {
        if (stopped || selectedDevice == null) return;
        try {
            dtcs = Collections.emptyList();
            Listener resetListener = listener;
            if (resetListener != null) resetListener.onDtcData(dtcs);
            setState(reconnectAttempt == 0 ? ObdConnectionState.CONNECTING_ELM : ObdConnectionState.RECONNECTING,
                    "Conectando ao ELM327...");
            elm.connect(selectedDevice);
            setState(ObdConnectionState.INITIALIZING_ELM, "Inicializando ELM327...");
            Elm327Initializer.Result init = initializer.initialize(elm);
            if (!init.ready) throw new IOException("ECU nao respondeu a 0100");
            protocol = init.protocol;
            setState(ObdConnectionState.CHECKING_SUPPORTED_PIDS, "Descobrindo PIDs suportados...");
            Set<ObdPid> supported = discoverSupported();
            if (supported.isEmpty()) throw new IOException("Nenhum PID solicitado e suportado");
            repository.setSupported(supported);
            scheduler = new ObdPollingScheduler(elm, parser, logs, this);
            scheduler.configure(supported);
            scheduler.start();
            reconnectAttempt = 0;
            setState(ObdConnectionState.READY, "OBD pronto: " + supported.size() + " PIDs");
        } catch (SecurityException | IOException error) {
            logs.add("Falha de conexao " + error.getMessage());
            lastError = 1;
            scheduleReconnect(error.getMessage());
        }
    }

    private Set<ObdPid> discoverSupported() throws IOException {
        EnumSet<ObdPid> supported = EnumSet.noneOf(ObdPid.class);
        for (int base = 0; base <= 0xC0; base += 0x20) {
            String command = String.format("01%02X", base);
            String raw = elm.execute(command, 5000);
            long bitmap = parser.parseSupportedMask(command, raw);
            if (bitmap < 0) break;
            for (ObdPid pid : ObdPid.values()) {
                int id = Integer.parseInt(pid.pid, 16);
                int offset = id - base;
                if (offset >= 1 && offset <= 32 && (bitmap & (1L << (32 - offset))) != 0)
                    supported.add(pid);
            }
            if ((bitmap & 1L) == 0) break;
        }
        return supported;
    }

    private void readDtcInternal() {
        if (stopped || !elm.isConnected() ||
                (state != ObdConnectionState.READY && state != ObdConnectionState.DEGRADED)) {
            setState(state, "Conecte a ECU antes de ler os codigos");
            return;
        }
        try {
            ArrayList<ObdDtc> result = new ArrayList<>();
            try {
                result.addAll(parser.parseDtcs("03", elm.execute("03", 5000), false));
            } catch (IllegalArgumentException error) {
                logs.add("DTC atuais invalidos: " + error.getMessage());
            }
            try {
                result.addAll(parser.parseDtcs("07", elm.execute("07", 5000), true));
            } catch (IllegalArgumentException error) {
                logs.add("DTC pendentes indisponiveis: " + error.getMessage());
            }
            dtcs = Collections.unmodifiableList(result);
            logs.add("DTC lidos: " + result.size());
            Listener current = listener;
            if (current != null) current.onDtcData(dtcs);
            if (ble.isObdDtcReady()) {
                ble.sendObdDtc(encoder.dtc(dtcs, ++packetSequence, System.currentTimeMillis()));
                logs.add("DTC transferidos para ESP32: " + result.size());
            } else {
                logs.add("DTC nao transferidos: caracteristica BLE ausente");
            }
            setState(state, result.isEmpty() ? "Nenhum codigo de falha encontrado" :
                    result.size() + " codigo(s) de falha encontrado(s)");
        } catch (IOException error) {
            lastError = 3;
            scheduleReconnect("Leitura DTC: " + error.getMessage());
        }
    }

    @Override
    public void onResult(ObdResult result, long latency) {
        latencyMs = (int) Math.min(65535, latency);
        if (result.status == ObdResult.Status.VALUE) {
            repository.update(result, System.currentTimeMillis());
            if (state == ObdConnectionState.DEGRADED) setState(ObdConnectionState.READY, "OBD recuperado");
        } else if (result.status == ObdResult.Status.TIMEOUT) {
            timeouts++;
            if (timeouts % 3 == 0) setState(ObdConnectionState.DEGRADED, "OBD degradado: timeouts");
        }
        Listener current = listener;
        if (current != null) current.onObdData(repository.snapshot(System.currentTimeMillis()));
    }

    @Override
    public void onTransportError(IOException error) {
        lastError = 2;
        scheduleReconnect(error.getMessage());
    }

    private void scheduleReconnect(String reason) {
        if (stopped) return;
        if (scheduler != null) scheduler.stop();
        elm.close();
        setState(ObdConnectionState.RECONNECTING, "Reconectando: " + reason);
        long delay = Math.min(30, 2L << Math.min(reconnectAttempt++, 4));
        control.schedule(this::connectInternal, delay, TimeUnit.SECONDS);
    }

    private void sendFrames() {
        if (!ble.isObdReady()) return;
        long now = System.currentTimeMillis();
        ObdRepository.Snapshot snapshot = repository.snapshot(now);
        if (snapshot.sequence != lastSentRepositorySequence || now - lastTelemetrySentAtMs >= 500) {
            lastSentRepositorySequence = snapshot.sequence;
            lastTelemetrySentAtMs = now;
            ble.sendObdTelemetry(encoder.telemetry(snapshot, ++packetSequence));
        }
        if (now - lastStatusSentAtMs >= 1000) {
            lastStatusSentAtMs = now;
            ble.sendObdStatus(encoder.status(state, elm.isConnected(),
                    state == ObdConnectionState.READY || state == ObdConnectionState.DEGRADED,
                    ble.isObdReady(), protocol, latencyMs, timeouts, lastError,
                    ++packetSequence, now));
        }
    }

    private void setState(ObdConnectionState value, String message) {
        state = value;
        lastStatusSentAtMs = 0;
        logs.add("STATE " + value + " " + message);
        Listener current = listener;
        if (current != null) current.onObdStatus(message);
    }
}
