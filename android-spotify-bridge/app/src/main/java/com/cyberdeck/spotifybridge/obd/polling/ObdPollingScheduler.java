package com.cyberdeck.spotifybridge.obd.polling;

import com.cyberdeck.spotifybridge.obd.connection.Elm327Connection;
import com.cyberdeck.spotifybridge.obd.diagnostics.ObdLogBuffer;
import com.cyberdeck.spotifybridge.obd.models.ObdPid;
import com.cyberdeck.spotifybridge.obd.models.ObdResult;
import com.cyberdeck.spotifybridge.obd.parser.Elm327Parser;

import java.io.IOException;
import java.net.SocketTimeoutException;
import java.util.ArrayList;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

public final class ObdPollingScheduler {
    public interface Listener {
        void onResult(ObdResult result, long latencyMs);
        void onTransportError(IOException error);
    }

    private final Elm327Connection connection;
    private final Elm327Parser parser;
    private final ObdLogBuffer logs;
    private final Listener listener;
    private final ArrayList<ObdPollItem> items = new ArrayList<>();
    private final AtomicBoolean running = new AtomicBoolean();
    private ExecutorService worker;
    private volatile ObdPid focus;
    private double adaptiveFactor = 1.0;

    public ObdPollingScheduler(Elm327Connection connection, Elm327Parser parser,
                               ObdLogBuffer logs, Listener listener) {
        this.connection = connection; this.parser = parser; this.logs = logs; this.listener = listener;
    }

    public synchronized void configure(Set<ObdPid> supported) {
        items.clear();
        for (ObdPid pid : supported) items.add(new ObdPollItem(pid, pid.baseIntervalMs, pid.priority, true));
    }

    public void setFocus(ObdPid pid) { focus = pid; }

    public synchronized void start() {
        if (running.getAndSet(true)) return;
        worker = Executors.newSingleThreadExecutor();
        worker.execute(this::loop);
    }

    public synchronized void stop() {
        running.set(false);
        if (worker != null) worker.shutdownNow();
        worker = null;
    }

    private void loop() {
        while (running.get()) {
            ObdPollItem item = nextItem(System.currentTimeMillis());
            if (item == null) {
                sleep(10);
                continue;
            }
            long started = System.currentTimeMillis();
            ObdResult result;
            try {
                String raw = connection.execute(item.pid.command, timeoutFor(item));
                result = parser.parse(item.pid, raw);
                long latency = Math.max(1, System.currentTimeMillis() - started);
                adaptiveFactor = Math.max(1.0, adaptiveFactor * 0.92);
                item.nextAtMs = System.currentTimeMillis() + effectiveInterval(item);
                logs.add("PID " + item.pid.pid + " " + result.status + " " + result.value + " " + latency + "ms");
                listener.onResult(result, latency);
            } catch (SocketTimeoutException timeout) {
                adaptiveFactor = Math.min(4.0, adaptiveFactor * 1.35);
                item.nextAtMs = System.currentTimeMillis() + effectiveInterval(item);
                listener.onResult(ObdResult.error(ObdResult.Status.TIMEOUT, item.pid, "", ""), System.currentTimeMillis() - started);
            } catch (IOException error) {
                listener.onTransportError(error);
                running.set(false);
            }
        }
    }

    private synchronized ObdPollItem nextItem(long now) {
        ObdPollItem best = null;
        double bestScore = Double.NEGATIVE_INFINITY;
        for (ObdPollItem item : items) {
            if (!item.enabled || item.nextAtMs > now) continue;
            double score = now - item.nextAtMs - item.priority * 20.0;
            if (item.pid == focus) score += 250;
            if (score > bestScore) { best = item; bestScore = score; }
        }
        return best;
    }

    private long effectiveInterval(ObdPollItem item) {
        double focusFactor = item.pid == focus ? 0.75 : 1.0;
        return Math.max(100, (long) (item.intervalMs * adaptiveFactor * focusFactor));
    }

    private long timeoutFor(ObdPollItem item) {
        return Math.max(800, Math.min(4000, effectiveInterval(item) * 4));
    }

    private void sleep(long millis) {
        try { Thread.sleep(millis); } catch (InterruptedException ignored) { Thread.currentThread().interrupt(); }
    }
}
