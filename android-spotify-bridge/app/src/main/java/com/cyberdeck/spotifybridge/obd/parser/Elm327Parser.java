package com.cyberdeck.spotifybridge.obd.parser;

import com.cyberdeck.spotifybridge.obd.models.ObdPid;
import com.cyberdeck.spotifybridge.obd.models.ObdResult;
import com.cyberdeck.spotifybridge.obd.models.ObdDtc;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public final class Elm327Parser {
    public String normalize(String command, String raw) {
        if (raw == null) return "";
        String text = raw.toUpperCase(Locale.US).replace("SEARCHING...", "");
        if (command != null) {
            String echo = command.toUpperCase(Locale.US).replaceAll("[^0-9A-F]", "");
            text = text.replace(echo, "");
        }
        return text.replace(">", "").replaceAll("[\r\n\t ]", "");
    }

    public ObdResult parse(ObdPid pid, String raw) {
        String upper = raw == null ? "" : raw.toUpperCase(Locale.US);
        ObdResult.Status explicit = explicitStatus(upper);
        String normalized = normalize(pid.command, raw);
        if (explicit != null && explicit != ObdResult.Status.SEARCHING) {
            return ObdResult.error(explicit, pid, raw, normalized);
        }

        String marker = "41" + pid.pid;
        int index = normalized.indexOf(marker);
        if (index < 0) return ObdResult.error(ObdResult.Status.INVALID_RESPONSE, pid, raw, normalized);
        int dataStart = index + marker.length();
        int required = pid.dataBytes * 2;
        if (normalized.length() < dataStart + required) {
            return ObdResult.error(ObdResult.Status.INVALID_RESPONSE, pid, raw, normalized);
        }
        String bytes = normalized.substring(dataStart, dataStart + required);
        if (!bytes.matches("[0-9A-F]+")) {
            return ObdResult.error(ObdResult.Status.INVALID_RESPONSE, pid, raw, normalized);
        }
        try {
            int a = Integer.parseInt(bytes.substring(0, 2), 16);
            int b = pid.dataBytes > 1 ? Integer.parseInt(bytes.substring(2, 4), 16) : 0;
            return ObdResult.value(pid, pid.convert(a, b), raw, normalized);
        } catch (RuntimeException error) {
            return ObdResult.error(ObdResult.Status.INVALID_RESPONSE, pid, raw, normalized);
        }
    }

    public long parseSupportedMask(String command, String raw) {
        String normalized = normalize(command, raw);
        String marker = "41" + command.substring(2).toUpperCase(Locale.US);
        int index = normalized.indexOf(marker);
        if (index < 0 || normalized.length() < index + marker.length() + 8) return -1;
        try {
            long bitmap = Long.parseLong(normalized.substring(index + marker.length(),
                    index + marker.length() + 8), 16);
            return bitmap;
        } catch (NumberFormatException error) {
            return -1;
        }
    }

    public List<ObdDtc> parseDtcs(String command, String raw, boolean pending) {
        String upper = raw == null ? "" : raw.toUpperCase(Locale.US);
        ObdResult.Status status = explicitStatus(upper);
        if (status == ObdResult.Status.NO_DATA || status == ObdResult.Status.UNSUPPORTED) {
            return new ArrayList<>();
        }
        if (status != null && status != ObdResult.Status.SEARCHING) {
            throw new IllegalArgumentException("Resposta DTC invalida: " + status);
        }

        String normalized = normalize(command, raw);
        String marker = pending ? "47" : "43";
        int index = normalized.indexOf(marker);
        if (index < 0) throw new IllegalArgumentException("Resposta DTC sem " + marker);
        String bytes = normalized.substring(index + marker.length());
        if ((bytes.length() & 1) != 0 || !bytes.matches("[0-9A-F]*"))
            throw new IllegalArgumentException("Bytes DTC invalidos");

        ArrayList<ObdDtc> result = new ArrayList<>();
        for (int offset = 0; offset + 4 <= bytes.length(); offset += 4) {
            int first = Integer.parseInt(bytes.substring(offset, offset + 2), 16);
            int second = Integer.parseInt(bytes.substring(offset + 2, offset + 4), 16);
            if (first == 0 && second == 0) continue;
            result.add(new ObdDtc(decodeDtc(first, second), pending));
            if (result.size() == 8) break;
        }
        return result;
    }

    private String decodeDtc(int first, int second) {
        char[] type = {'P', 'C', 'B', 'U'};
        String hex = "0123456789ABCDEF";
        return "" + type[(first >> 6) & 0x03]
                + hex.charAt((first >> 4) & 0x03)
                + hex.charAt(first & 0x0F)
                + hex.charAt((second >> 4) & 0x0F)
                + hex.charAt(second & 0x0F);
    }

    private ObdResult.Status explicitStatus(String raw) {
        if (raw.contains("NO DATA")) return ObdResult.Status.NO_DATA;
        if (raw.contains("STOPPED")) return ObdResult.Status.STOPPED;
        if (raw.contains("UNABLE TO CONNECT")) return ObdResult.Status.UNABLE_TO_CONNECT;
        if (raw.contains("BUS INIT: ERROR") || raw.contains("BUS ERROR")) return ObdResult.Status.BUS_ERROR;
        if (raw.contains("CAN ERROR")) return ObdResult.Status.CAN_ERROR;
        if (raw.trim().equals("?") || raw.contains("\r?\r")) return ObdResult.Status.UNSUPPORTED;
        if (raw.contains("ERROR")) return ObdResult.Status.ERROR;
        if (raw.contains("SEARCHING...")) return ObdResult.Status.SEARCHING;
        return null;
    }
}
