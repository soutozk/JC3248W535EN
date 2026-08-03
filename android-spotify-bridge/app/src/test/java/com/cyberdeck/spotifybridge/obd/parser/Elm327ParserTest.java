package com.cyberdeck.spotifybridge.obd.parser;

import com.cyberdeck.spotifybridge.obd.models.ObdPid;
import com.cyberdeck.spotifybridge.obd.models.ObdResult;

import org.junit.Test;

import static org.junit.Assert.assertEquals;

public class Elm327ParserTest {
    private final Elm327Parser parser = new Elm327Parser();

    @Test
    public void parsesSpacedResponseWithEcho() {
        ObdResult result = parser.parse(ObdPid.RPM, "01 0C\r41 0C 1A F8\r>");

        assertEquals(ObdResult.Status.VALUE, result.status);
        assertEquals(1726.0, result.value, 0.001);
    }

    @Test
    public void parsesSingleBytePid() {
        ObdResult result = parser.parse(ObdPid.COOLANT, "41057B>");

        assertEquals(ObdResult.Status.VALUE, result.status);
        assertEquals(83.0, result.value, 0.001);
    }

    @Test
    public void preservesExplicitElmError() {
        ObdResult result = parser.parse(ObdPid.SPEED, "NO DATA\r>");

        assertEquals(ObdResult.Status.NO_DATA, result.status);
    }

    @Test
    public void parsesSupportedPidBitmap() {
        long mask = parser.parseSupportedMask("0100", "41 00 BE 3E B8 13\r>");

        assertEquals(0xBE3EB813L, mask);
    }
}
