package com.cyberdeck.spotifybridge.obd.diagnostics;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.List;

public final class ObdLogBuffer {
    private final int capacity;
    private final ArrayDeque<String> entries = new ArrayDeque<>();

    public ObdLogBuffer(int capacity) {
        this.capacity = Math.max(10, capacity);
    }

    public synchronized void add(String entry) {
        while (entries.size() >= capacity) entries.removeFirst();
        entries.addLast(System.currentTimeMillis() + " " + entry);
    }

    public synchronized List<String> snapshot() {
        return new ArrayList<>(entries);
    }
}
