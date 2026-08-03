package com.cyberdeck.spotifybridge.obd.connection;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
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

    public Elm327Connection(Context context, ObdLogBuffer logs) {
        this.context = context.getApplicationContext();
        this.logs = logs;
    }

    @SuppressLint("MissingPermission") // guarded by hasPermission()
    public synchronized void connect(BluetoothDevice device) throws IOException {
        close();
        if (!hasPermission()) throw new SecurityException("BLUETOOTH_CONNECT necessario");
        socket = device.createRfcommSocketToServiceRecord(SPP_UUID);
        socket.connect();
        input = socket.getInputStream();
        output = socket.getOutputStream();
        logs.add("ELM RFCOMM conectado: " + device.getAddress());
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
                int value = input.read();
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
