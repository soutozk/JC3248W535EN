package com.cyberdeck.spotifybridge.obd.blebridge;

import com.cyberdeck.spotifybridge.obd.models.ObdConnectionState;
import com.cyberdeck.spotifybridge.obd.models.ObdPid;
import com.cyberdeck.spotifybridge.obd.repository.ObdRepository;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;

public final class ObdBlePacketEncoder {
    public static final int TELEMETRY_SIZE = 48;
    public static final int STATUS_SIZE = 36;

    public byte[] telemetry(ObdRepository.Snapshot s, int packetSequence) {
        ByteBuffer b = ByteBuffer.allocate(TELEMETRY_SIZE).order(ByteOrder.LITTLE_ENDIAN);
        b.putShort((short) 0x424f).put((byte) 1).put((byte) 1).putShort((short) TELEMETRY_SIZE);
        b.putInt(packetSequence).putLong(s.timestampMs);
        b.putShort((short) s.validMask).putShort((short) s.supportedMask);
        b.putShort(u16(s.get(ObdPid.RPM)));
        b.putShort(u16(s.get(ObdPid.SPEED)));
        b.putShort(i16(s.get(ObdPid.COOLANT) * 10));
        b.putShort(u16(s.get(ObdPid.THROTTLE) * 100));
        b.putShort(u16(s.get(ObdPid.ENGINE_LOAD) * 100));
        b.putShort(u16(s.get(ObdPid.CONTROL_VOLTAGE) * 1000));
        b.putShort(u16(s.get(ObdPid.FUEL_LEVEL) * 100));
        b.putShort(i16(s.get(ObdPid.INTAKE_AIR) * 10));
        b.putShort(u16(s.get(ObdPid.MAP) * 10));
        b.putShort(u16(s.get(ObdPid.MAF) * 100));
        b.putInt((int) Math.max(0, Math.min(0xffffffffL, Math.round(s.get(ObdPid.RUNTIME)))));
        b.putShort(crc16(b.array(), TELEMETRY_SIZE - 2));
        return b.array();
    }

    public byte[] status(ObdConnectionState state, boolean elm, boolean ecu, boolean esp,
                         String protocol, int latencyMs, int timeouts, int lastError,
                         int sequence, long nowMs) {
        ByteBuffer b = ByteBuffer.allocate(STATUS_SIZE).order(ByteOrder.LITTLE_ENDIAN);
        b.putShort((short) 0x424f).put((byte) 1).put((byte) 2).putShort((short) STATUS_SIZE);
        b.putInt(sequence).putLong(nowMs).put((byte) state.ordinal());
        b.put((byte) ((elm ? 1 : 0) | (ecu ? 2 : 0) | (esp ? 4 : 0)));
        byte[] protocolBytes = protocol == null ? new byte[0] :
                protocol.getBytes(StandardCharsets.US_ASCII);
        for (int i = 0; i < 8; i++) b.put(i < protocolBytes.length ? protocolBytes[i] : 0);
        b.putShort((short) clamp16(latencyMs)).putShort((short) clamp16(timeouts));
        b.putShort((short) clamp16(lastError));
        b.putShort(crc16(b.array(), STATUS_SIZE - 2));
        return b.array();
    }

    public static short crc16(byte[] bytes, int length) {
        int crc = 0xffff;
        for (int i = 0; i < length; i++) {
            crc ^= (bytes[i] & 0xff) << 8;
            for (int bit = 0; bit < 8; bit++)
                crc = (crc & 0x8000) != 0 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff;
        }
        return (short) crc;
    }

    private short u16(double value) { return (short) clamp16((int) Math.round(Math.max(0, value))); }
    private short i16(double value) { return (short) Math.max(Short.MIN_VALUE, Math.min(Short.MAX_VALUE, Math.round(value))); }
    private int clamp16(int value) { return Math.max(0, Math.min(0xffff, value)); }
}
