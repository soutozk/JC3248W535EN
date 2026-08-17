package com.cyberdeck.spotifybridge.obd.models;

public final class ObdDtc {
    public final String code;
    public final boolean pending;

    public ObdDtc(String code, boolean pending) {
        this.code = code;
        this.pending = pending;
    }
}
