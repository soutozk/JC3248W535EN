package com.cyberdeck.spotifybridge.obd.polling;

import com.cyberdeck.spotifybridge.obd.models.ObdPid;

public final class ObdPollItem {
    public final ObdPid pid;
    public long intervalMs;
    public final int priority;
    public boolean enabled;
    long nextAtMs;

    public ObdPollItem(ObdPid pid, long intervalMs, int priority, boolean enabled) {
        this.pid = pid; this.intervalMs = intervalMs; this.priority = priority; this.enabled = enabled;
    }
}
