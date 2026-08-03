package com.cyberdeck.spotifybridge.obd.blebridge;

import com.cyberdeck.spotifybridge.obd.models.ObdConnectionState;
import com.cyberdeck.spotifybridge.obd.models.ObdPid;
import com.cyberdeck.spotifybridge.obd.models.ObdResult;
import com.cyberdeck.spotifybridge.obd.repository.ObdRepository;

import org.junit.Test;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.EnumSet;

import static org.junit.Assert.assertEquals;

public class ObdBlePacketEncoderTest {
    private final ObdBlePacketEncoder encoder = new ObdBlePacketEncoder();

    @Test
    public void telemetryMatchesWireLayoutAndCrc() {
        ObdRepository repository = new ObdRepository();
        repository.setSupported(EnumSet.of(ObdPid.RPM, ObdPid.SPEED));
        repository.update(ObdResult.value(ObdPid.RPM, 3456.0, "", ""), 1000);
        repository.update(ObdResult.value(ObdPid.SPEED, 123.0, "", ""), 1000);

        byte[] frame = encoder.telemetry(repository.snapshot(1100), 7);
        ByteBuffer bytes = ByteBuffer.wrap(frame).order(ByteOrder.LITTLE_ENDIAN);

        assertEquals(ObdBlePacketEncoder.TELEMETRY_SIZE, frame.length);
        assertEquals(0x424f, bytes.getShort(0) & 0xffff);
        assertEquals(1, bytes.get(2));
        assertEquals(1, bytes.get(3));
        assertEquals(7, bytes.getInt(6));
        assertEquals(3456, bytes.getShort(22) & 0xffff);
        assertEquals(123, bytes.getShort(24) & 0xffff);
        assertEquals(ObdBlePacketEncoder.crc16(frame, frame.length - 2),
                bytes.getShort(frame.length - 2));
    }

    @Test
    public void statusMatchesWireLayoutAndCrc() {
        byte[] frame = encoder.status(ObdConnectionState.READY, true, true, true,
                "ISO 15765", 42, 3, 0, 9, 1234);
        ByteBuffer bytes = ByteBuffer.wrap(frame).order(ByteOrder.LITTLE_ENDIAN);

        assertEquals(ObdBlePacketEncoder.STATUS_SIZE, frame.length);
        assertEquals(2, bytes.get(3));
        assertEquals(ObdConnectionState.READY.ordinal(), bytes.get(18) & 0xff);
        assertEquals(7, bytes.get(19) & 0xff);
        assertEquals(42, bytes.getShort(28) & 0xffff);
        assertEquals(ObdBlePacketEncoder.crc16(frame, frame.length - 2),
                bytes.getShort(frame.length - 2));
    }
}
