package com.cyberdeck.spotifybridge.obd.models;

public final class ObdResult {
    public enum Status {
        VALUE, NO_DATA, STOPPED, SEARCHING, UNABLE_TO_CONNECT, BUS_ERROR,
        CAN_ERROR, ERROR, UNSUPPORTED, TIMEOUT, ELM_DISCONNECTED,
        ECU_DISCONNECTED, INVALID_RESPONSE
    }

    public final Status status;
    public final ObdPid pid;
    public final double value;
    public final String raw;
    public final String normalized;

    private ObdResult(Status status, ObdPid pid, double value, String raw, String normalized) {
        this.status = status;
        this.pid = pid;
        this.value = value;
        this.raw = raw;
        this.normalized = normalized;
    }

    public static ObdResult value(ObdPid pid, double value, String raw, String normalized) {
        return new ObdResult(Status.VALUE, pid, value, raw, normalized);
    }

    public static ObdResult error(Status status, ObdPid pid, String raw, String normalized) {
        return new ObdResult(status, pid, Double.NaN, raw, normalized);
    }
}
