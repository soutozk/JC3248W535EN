package com.cyberdeck.spotifybridge.obd.elm327;

import com.cyberdeck.spotifybridge.obd.connection.Elm327Connection;
import com.cyberdeck.spotifybridge.obd.diagnostics.ObdLogBuffer;

import java.io.IOException;
import java.util.Locale;

public final class Elm327Initializer {
    public static final class Result {
        public final boolean ready;
        public final String adapterVersion;
        public final String protocol;
        Result(boolean ready, String adapterVersion, String protocol) {
            this.ready = ready; this.adapterVersion = adapterVersion; this.protocol = protocol;
        }
    }

    private final ObdLogBuffer logs;
    public Elm327Initializer(ObdLogBuffer logs) { this.logs = logs; }

    public Result initialize(Elm327Connection connection) throws IOException {
        String version = send(connection, "ATZ", 2500, false);
        send(connection, "ATE0", 1000, false);
        send(connection, "ATL0", 1000, true);
        send(connection, "ATS0", 1000, true);
        send(connection, "ATH0", 1000, true);
        send(connection, "ATSP0", 1200, false);
        send(connection, "ATAT1", 1000, true);
        String ecu = connection.execute("0100", 5000);
        String upper = ecu.toUpperCase(Locale.US);
        boolean ready = upper.contains("4100") || upper.contains("41 00");
        String protocol = send(connection, "ATDP", 1200, true).replace(">", "").replace("\r", "").replace("\n", "").trim();
        return new Result(ready, version.replace(">", "").trim(), protocol);
    }

    private String send(Elm327Connection c, String command, long timeout, boolean optional) throws IOException {
        String response = c.execute(command, timeout);
        if (response.contains("?") && optional) {
            logs.add("Comando opcional nao suportado: " + command);
            return "";
        }
        if (response.contains("?")) throw new IOException("Comando necessario nao suportado: " + command);
        return response;
    }
}
