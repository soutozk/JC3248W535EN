package com.cyberdeck.spotifybridge.obd.connection;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothSocket;
import android.bluetooth.BluetoothManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;

import com.cyberdeck.spotifybridge.obd.diagnostics.ObdLogBuffer;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.UUID;

public final class Elm327Connection {
    public static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805f9b34fb");
    private final Context context;
    private final ObdLogBuffer logs;
    private BluetoothSocket socket;
    private InputStream input;
    private OutputStream output;
    private boolean usingInsecureSocket;
    private boolean preferInsecureSocket;

    public Elm327Connection(Context context, ObdLogBuffer logs) {
        this.context = context.getApplicationContext();
        this.logs = logs;
    }

    @SuppressLint("MissingPermission") // guarded by hasPermission()
    public synchronized void connect(BluetoothDevice device) throws IOException {
        close();
        if (!hasPermission()) throw new SecurityException("BLUETOOTH_CONNECT necessario");

        BluetoothManager manager = (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter adapter = manager == null ? null : manager.getAdapter();
        if (adapter != null) {
            // Discovery competes with RFCOMM and can make connect/read fail on
            // phones that are also scanning for the CyberDeck BLE device.
            adapter.cancelDiscovery();
        }

        IOException first;
        IOException second;
        if (preferInsecureSocket) {
            first = tryConnectInsecure(device);
            if (first == null) return;
            second = tryConnectSecure(device);
        } else {
            first = tryConnectSecure(device);
            if (first == null) return;
            second = tryConnectInsecure(device);
        }
        if (second == null) return;
        IOException combined = new IOException("RFCOMM 1: " + first.getMessage()
                + "; RFCOMM 2: " + second.getMessage());
        combined.addSuppressed(first);
        combined.addSuppressed(second);
        throw combined;
    }

    private IOException tryConnectSecure(BluetoothDevice device) {
        try {
            connectSocket(device.createRfcommSocketToServiceRecord(SPP_UUID), false);
            preferInsecureSocket = false;
            return null;
        } catch (IOException error) {
            logs.add("RFCOMM seguro falhou: " + error.getMessage());
            closeSocketOnly();
            return error;
        }
    }

    private IOException tryConnectInsecure(BluetoothDevice device) {
        try {
            connectSocket(device.createInsecureRfcommSocketToServiceRecord(SPP_UUID), true);
            preferInsecureSocket = true;
            return null;
        } catch (IOException error) {
            logs.add("RFCOMM inseguro falhou: " + error.getMessage());
            closeSocketOnly();
            return error;
        }
    }

    @SuppressLint("MissingPermission")
    private void connectSocket(BluetoothSocket candidate, boolean insecure) throws IOException {
        try {
            candidate.connect();
            socket = candidate;
            input = candidate.getInputStream();
            output = candidate.getOutputStream();
            usingInsecureSocket = insecure;
            logs.add("ELM RFCOMM conectado (" + (insecure ? "inseguro" : "seguro") + ")");
        } catch (IOException error) {
            try { candidate.close(); } catch (IOException ignored) {}
            throw error;
        }
    }

    private void closeSocketOnly() {
        try { if (socket != null) socket.close(); } catch (IOException ignored) {}
        socket = null;
        input = null;
        output = null;
    }

    public synchronized String execute(String command, long timeoutMs) throws IOException {
        if (socket == null || !socket.isConnected() || input == null || output == null)
            throw new IOException("ELM desconectado");
        while (input.available() > 0) input.read();
        logs.add("TX " + command);
        output.write((command.trim() + "\r").getBytes(StandardCharsets.US_ASCII));
        output.flush();
        long deadline = System.currentTimeMillis() + timeoutMs;
        ByteArrayOutputStream response = new ByteArrayOutputStream();
        while (System.currentTimeMillis() < deadline) {
            while (input.available() > 0) {
                int value;
                try {
                    value = input.read();
                } catch (IOException error) {
                    preferInsecureSocket = !usingInsecureSocket;
                    throw new IOException("Falha lendo resposta de " + command + ": "
                            + error.getMessage(), error);
                }
                if (value < 0) throw new IOException("Fim do stream ELM");
                response.write(value);
                if (value == '>') {
                    String raw = response.toString(StandardCharsets.US_ASCII.name());
                    logs.add("RX " + raw.replace('\r', ' ').replace('\n', ' ').trim());
                    return raw;
                }
            }
            try { Thread.sleep(8); }
            catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); throw new IOException("Interrompido", interrupted); }
        }
        logs.add("TIMEOUT " + command);
        throw new java.net.SocketTimeoutException(command);
    }

    public synchronized boolean isConnected() {
        return socket != null && socket.isConnected();
    }

    public synchronized void close() {
        try { if (socket != null) socket.close(); } catch (IOException ignored) {}
        socket = null; input = null; output = null;
    }

    private boolean hasPermission() {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.S ||
                context.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
    }
}
