package com.cyberdeck.spotifybridge.obd.repository;

import com.cyberdeck.spotifybridge.obd.models.ObdPid;
import com.cyberdeck.spotifybridge.obd.models.ObdResult;

import java.util.EnumMap;
import java.util.EnumSet;
import java.util.Set;

public final class ObdRepository {
    public static final class Value {
        public final double number;
        public final long timestampMs;
        Value(double number, long timestampMs) { this.number = number; this.timestampMs = timestampMs; }
    }

    public static final class Snapshot {
        public final long sequence;
        public final long timestampMs;
        public final int validMask;
        public final int supportedMask;
        public final EnumMap<ObdPid, Value> values;
        Snapshot(long sequence, long timestampMs, int validMask, int supportedMask,
                 EnumMap<ObdPid, Value> values) {
            this.sequence = sequence; this.timestampMs = timestampMs;
            this.validMask = validMask; this.supportedMask = supportedMask; this.values = values;
        }
        public double get(ObdPid pid) { Value value = values.get(pid); return value == null ? 0 : value.number; }
    }

    private final EnumMap<ObdPid, Value> values = new EnumMap<>(ObdPid.class);
    private EnumSet<ObdPid> supported = EnumSet.noneOf(ObdPid.class);
    private long sequence;

    public synchronized void setSupported(Set<ObdPid> pids) {
        supported = pids.isEmpty() ? EnumSet.noneOf(ObdPid.class) : EnumSet.copyOf(pids);
    }

    public synchronized void update(ObdResult result, long nowMs) {
        if (result.status == ObdResult.Status.VALUE && result.pid != null) {
            values.put(result.pid, new Value(result.value, nowMs));
            sequence++;
        }
    }

    public synchronized Snapshot snapshot(long nowMs) {
        EnumMap<ObdPid, Value> copy = new EnumMap<>(values);
        int valid = 0;
        int supportedMask = 0;
        for (ObdPid pid : supported) {
            supportedMask |= pid.bit();
            Value value = copy.get(pid);
            long stale = staleMs(pid);
            if (value != null && nowMs - value.timestampMs <= stale) valid |= pid.bit();
        }
        return new Snapshot(sequence, nowMs, valid, supportedMask, copy);
    }

    private long staleMs(ObdPid pid) {
        switch (pid) {
            case RPM: return 500;
            case SPEED: return 1000;
            case COOLANT: return 5000;
            case CONTROL_VOLTAGE:
            case FUEL_LEVEL:
            case INTAKE_AIR:
            case RUNTIME: return 6000;
            default: return 1500;
        }
    }
}
