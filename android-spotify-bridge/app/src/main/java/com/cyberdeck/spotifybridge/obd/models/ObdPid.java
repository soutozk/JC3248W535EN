package com.cyberdeck.spotifybridge.obd.models;

public enum ObdPid {
    ENGINE_LOAD("04", 1, 400, 3),
    COOLANT("05", 1, 1000, 2),
    RPM("0C", 2, 120, 0),
    SPEED("0D", 1, 250, 1),
    INTAKE_AIR("0F", 1, 1500, 4),
    MAF("10", 2, 500, 3),
    THROTTLE("11", 1, 250, 2),
    RUNTIME("1F", 2, 3000, 5),
    FUEL_LEVEL("2F", 1, 4000, 6),
    CONTROL_VOLTAGE("42", 2, 1500, 5),
    MAP("0B", 1, 500, 3);

    public final String pid;
    public final String command;
    public final int dataBytes;
    public final long baseIntervalMs;
    public final int priority;

    ObdPid(String pid, int dataBytes, long baseIntervalMs, int priority) {
        this.pid = pid;
        this.command = "01" + pid;
        this.dataBytes = dataBytes;
        this.baseIntervalMs = baseIntervalMs;
        this.priority = priority;
    }

    public double convert(int a, int b) {
        switch (this) {
            case RPM: return ((a * 256) + b) / 4.0;
            case SPEED: return a;
            case COOLANT:
            case INTAKE_AIR: return a - 40.0;
            case ENGINE_LOAD:
            case THROTTLE:
            case FUEL_LEVEL: return a * 100.0 / 255.0;
            case CONTROL_VOLTAGE: return ((a * 256) + b) / 1000.0;
            case MAF: return ((a * 256) + b) / 100.0;
            case RUNTIME: return (a * 256) + b;
            case MAP: return a;
            default: throw new IllegalStateException("PID sem conversao");
        }
    }

    public int bit() {
        switch (this) {
            case RPM: return 1 << 0;
            case SPEED: return 1 << 1;
            case COOLANT: return 1 << 2;
            case THROTTLE: return 1 << 3;
            case ENGINE_LOAD: return 1 << 4;
            case CONTROL_VOLTAGE: return 1 << 5;
            case FUEL_LEVEL: return 1 << 6;
            case INTAKE_AIR: return 1 << 7;
            case MAP: return 1 << 8;
            case MAF: return 1 << 9;
            case RUNTIME: return 1 << 10;
            default: return 0;
        }
    }
}
