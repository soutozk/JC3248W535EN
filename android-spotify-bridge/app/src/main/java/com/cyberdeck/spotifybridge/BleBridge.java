package com.cyberdeck.spotifybridge;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothStatusCodes;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.Arrays;
import java.util.UUID;

public class BleBridge {
    public interface Listener {
        void onStatus(String status);
        void onConnectionChanged(boolean connected);
        void onSpotifyTrack(String title, String artist, boolean hasCover);
    }

    private static final String DEVICE_NAME = "CyberDeck_Spotify";
    private static final int COVER_SIZE = 300;
    private static final int COVER_CHUNK_SIZE = 120;

    private static final UUID SERVICE_UUID =
            UUID.fromString("f38a0001-82eb-4a73-a38c-ce98c9438012");
    private static final UUID TRACK_UUID =
            UUID.fromString("f38a0002-82eb-4a73-a38c-ce98c9438012");
    private static final UUID CONTROL_UUID =
            UUID.fromString("f38a0003-82eb-4a73-a38c-ce98c9438012");
    private static final UUID COVER_SIZE_UUID =
            UUID.fromString("f38a0004-82eb-4a73-a38c-ce98c9438012");
    private static final UUID COVER_DATA_UUID =
            UUID.fromString("f38a0005-82eb-4a73-a38c-ce98c9438012");
    private static final UUID CLIENT_CONFIG_UUID =
            UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");

    private static BleBridge instance;

    private final Context context;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final ArrayDeque<WriteOp> writeQueue = new ArrayDeque<>();

    private Listener listener;
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic trackCharacteristic;
    private BluetoothGattCharacteristic controlCharacteristic;
    private BluetoothGattCharacteristic coverSizeCharacteristic;
    private BluetoothGattCharacteristic coverDataCharacteristic;

    private boolean scanning;
    private boolean writeInProgress;
    private String lastAutoPayload = "";
    private int lastAutoCoverHash = 0;
    private int currentCoverBytesTotal = 0;
    private int currentCoverBytesSent = 0;
    private WriteOp currentWriteOp;
    private int activeTransferId = 0;

    public static synchronized BleBridge get(Context context) {
        if (instance == null) {
            instance = new BleBridge(context.getApplicationContext());
        }
        return instance;
    }

    private BleBridge(Context context) {
        this.context = context;
    }

    public void setListener(Listener listener) {
        this.listener = listener;
    }

    public boolean isReady() {
        return gatt != null && trackCharacteristic != null;
    }

    public void startScan() {
        if (!hasScanPermission()) {
            setStatus("Permissao Bluetooth necessaria");
            return;
        }

        BluetoothManager manager = (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter adapter = manager != null ? manager.getAdapter() : null;
        scanner = adapter != null ? adapter.getBluetoothLeScanner() : null;
        if (scanner == null) {
            setStatus("Bluetooth LE indisponivel");
            return;
        }

        close();
        setStatus("Procurando CyberDeck_Spotify...");
        scanning = true;
        scanner.startScan(scanCallback);

        mainHandler.postDelayed(() -> {
            if (scanning) {
                stopScan();
                setStatus("Nao encontrou CyberDeck_Spotify");
            }
        }, 12000);
    }

    public void stopScan() {
        if (scanner != null && scanning && hasScanPermission()) {
            scanner.stopScan(scanCallback);
        }
        scanning = false;
    }

    public void close() {
        stopScan();
        writeQueue.clear();
        writeInProgress = false;
        if (gatt != null && hasConnectPermission()) {
            gatt.close();
        }
        gatt = null;
        trackCharacteristic = null;
        controlCharacteristic = null;
        coverSizeCharacteristic = null;
        coverDataCharacteristic = null;
        notifyConnection(false);
    }

    public void sendManualTrack(String title, String artist) {
        sendTrackAndCover(title, artist, null, true);
    }

    public void sendSpotifyTrack(String title, String artist, Bitmap cover) {
        sendTrackAndCover(title, artist, cover, false);
    }

    private void sendTrackAndCover(String title, String artist, Bitmap cover, boolean force) {
        String cleanTitle = clean(title);
        String cleanArtist = clean(artist);
        if (cleanTitle.isEmpty()) {
            return;
        }
        if (cleanArtist.isEmpty()) {
            cleanArtist = "Spotify";
        }

        if (!isReady()) {
            setStatus("Spotify detectado, mas BLE nao esta conectado");
            return;
        }

        byte[] jpeg = cover != null ? bitmapToJpeg300(cover) : null;
        int coverHash = jpeg != null ? Arrays.hashCode(jpeg) : 0;

        String payload = cleanTitle + ";" + cleanArtist;
        if (!force && payload.equals(lastAutoPayload) && coverHash == lastAutoCoverHash) {
            return;
        }

        if (!force) {
            writeQueue.clear();
        }

        activeTransferId++;
        lastAutoPayload = payload;
        lastAutoCoverHash = coverHash;

        byte[] metadata = payload.getBytes(StandardCharsets.UTF_8);
        enqueueWrite(trackCharacteristic, metadata, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT, true);

        if (jpeg != null && coverSizeCharacteristic != null && coverDataCharacteristic != null) {
            currentCoverBytesTotal = jpeg.length;
            currentCoverBytesSent = 0;

            byte[] size = ByteBuffer.allocate(4)
                    .order(ByteOrder.LITTLE_ENDIAN)
                    .putInt(jpeg.length)
                    .array();
            enqueueWrite(coverSizeCharacteristic, size, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT, true);
            for (int offset = 0; offset < jpeg.length; offset += COVER_CHUNK_SIZE) {
                int len = Math.min(COVER_CHUNK_SIZE, jpeg.length - offset);
                byte[] chunk = new byte[len];
                System.arraycopy(jpeg, offset, chunk, 0, len);
                enqueueWrite(coverDataCharacteristic, chunk,
                        BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT, true);
            }
            setStatus("Enviando Spotify + capa (0/" + (jpeg.length / 1024) + " KB)");
        } else {
            currentCoverBytesTotal = 0;
            currentCoverBytesSent = 0;
            setStatus("Enviando Spotify: " + payload);
        }

        notifySpotify(cleanTitle, cleanArtist, jpeg != null);
        processNextWrite();
    }

    private void enqueueWrite(BluetoothGattCharacteristic characteristic, byte[] data,
                              int writeType, boolean waitForCallback) {
        if (characteristic == null || data == null) {
            return;
        }
        byte[] copy = new byte[data.length];
        System.arraycopy(data, 0, copy, 0, data.length);
        writeQueue.add(new WriteOp(characteristic, copy, writeType, waitForCallback, activeTransferId));
    }

    private void processNextWrite() {
        mainHandler.post(() -> {
            if (writeInProgress || gatt == null || writeQueue.isEmpty() || !hasConnectPermission()) {
                return;
            }

            WriteOp op = writeQueue.poll();
            currentWriteOp = op;
            writeInProgress = true;
            boolean started = writeCharacteristic(op);
            if (!started) {
                currentWriteOp = null;
                writeInProgress = false;
                setStatus("Falha ao iniciar escrita BLE");
                processNextWrite();
                return;
            }

            if (!op.waitForCallback) {
                mainHandler.postDelayed(() -> {
                    currentWriteOp = null;
                    writeInProgress = false;
                    processNextWrite();
                }, 18);
            }
        });
    }

    private boolean writeCharacteristic(WriteOp op) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            int result = gatt.writeCharacteristic(op.characteristic, op.data, op.writeType);
            return result == BluetoothStatusCodes.SUCCESS;
        }

        op.characteristic.setWriteType(op.writeType);
        op.characteristic.setValue(op.data);
        return gatt.writeCharacteristic(op.characteristic);
    }

    private byte[] bitmapToJpeg300(Bitmap source) {
        Bitmap out = Bitmap.createBitmap(COVER_SIZE, COVER_SIZE, Bitmap.Config.RGB_565);
        Canvas canvas = new Canvas(out);
        canvas.drawColor(0xff101318);

        float scale = Math.max(
                COVER_SIZE / (float) source.getWidth(),
                COVER_SIZE / (float) source.getHeight());
        float width = source.getWidth() * scale;
        float height = source.getHeight() * scale;
        float left = (COVER_SIZE - width) / 2f;
        float top = (COVER_SIZE - height) / 2f;

        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        canvas.drawBitmap(source, null,
                new android.graphics.RectF(left, top, left + width, top + height), paint);

        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        out.compress(Bitmap.CompressFormat.JPEG, 68, stream);
        return stream.toByteArray();
    }

    private void enableControlNotifications() {
        if (gatt == null || controlCharacteristic == null || !hasConnectPermission()) {
            return;
        }
        gatt.setCharacteristicNotification(controlCharacteristic, true);
        BluetoothGattDescriptor descriptor = controlCharacteristic.getDescriptor(CLIENT_CONFIG_UUID);
        if (descriptor == null) {
            return;
        }
        enqueueWriteDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
    }

    private void enqueueWriteDescriptor(BluetoothGattDescriptor descriptor, byte[] value) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            gatt.writeDescriptor(descriptor, value);
        } else {
            descriptor.setValue(value);
            gatt.writeDescriptor(descriptor);
        }
    }

    private final ScanCallback scanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            BluetoothDevice device = result.getDevice();
            String name = null;
            if (hasConnectPermission()) {
                name = device.getName();
            }
            if (name == null && result.getScanRecord() != null) {
                name = result.getScanRecord().getDeviceName();
            }

            if (DEVICE_NAME.equals(name)) {
                setStatus("Encontrado. Conectando...");
                stopScan();
                connect(device);
            }
        }

        @Override
        public void onScanFailed(int errorCode) {
            scanning = false;
            setStatus("Falha no scan BLE: " + errorCode);
        }
    };

    private void connect(BluetoothDevice device) {
        if (!hasConnectPermission()) {
            setStatus("Permissao de conexao Bluetooth necessaria");
            return;
        }
        gatt = device.connectGatt(context, false, gattCallback);
    }

    private final BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                notifyConnection(true);
                setStatus("Conectado. Negociando MTU...");
                if (hasConnectPermission()) {
                    gatt.requestMtu(247);
                    mainHandler.postDelayed(() -> {
                        if (BleBridge.this.gatt != null && trackCharacteristic == null && hasConnectPermission()) {
                            BleBridge.this.gatt.discoverServices();
                        }
                    }, 1000);
                }
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                trackCharacteristic = null;
                setStatus("Desconectado");
                notifyConnection(false);
                close();
            }
        }

        @Override
        public void onMtuChanged(BluetoothGatt gatt, int mtu, int status) {
            setStatus("MTU BLE: " + mtu + ". Descobrindo servicos...");
            if (hasConnectPermission()) {
                gatt.discoverServices();
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            BluetoothGattService service = gatt.getService(SERVICE_UUID);
            if (service == null) {
                setStatus("Servico Spotify nao encontrado");
                return;
            }

            trackCharacteristic = service.getCharacteristic(TRACK_UUID);
            controlCharacteristic = service.getCharacteristic(CONTROL_UUID);
            coverSizeCharacteristic = service.getCharacteristic(COVER_SIZE_UUID);
            coverDataCharacteristic = service.getCharacteristic(COVER_DATA_UUID);

            if (trackCharacteristic == null) {
                setStatus("Caracteristica de musica nao encontrada");
                return;
            }

            enableControlNotifications();
            setStatus("Pronto. Abra o Spotify e toque uma musica.");
            notifyConnection(true);
            mainHandler.postDelayed(() -> SpotifyNotificationListener.pushActiveSpotify(context), 500);
        }

        @Override
        public void onCharacteristicWrite(BluetoothGatt gatt,
                                          BluetoothGattCharacteristic characteristic,
                                          int status) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                setStatus("Falha ao enviar BLE: " + status);
            }
            updateCoverProgress(currentWriteOp);
            currentWriteOp = null;
            writeInProgress = false;
            processNextWrite();
            if (writeQueue.isEmpty() && status == BluetoothGatt.GATT_SUCCESS) {
                setStatus("Envio concluido");
            }
        }

        @Override
        public void onCharacteristicChanged(BluetoothGatt gatt,
                                            BluetoothGattCharacteristic characteristic) {
            handleControlNotification(characteristic.getValue());
        }

        @Override
        public void onCharacteristicChanged(BluetoothGatt gatt,
                                            BluetoothGattCharacteristic characteristic,
                                            byte[] value) {
            handleControlNotification(value);
        }
    };

    private void handleControlNotification(byte[] value) {
        if (value == null || value.length == 0) {
            return;
        }

        int command = value[0] & 0xff;
        if (command == 1) {
            setStatus("Comando da placa: proxima");
        } else if (command == 2) {
            setStatus("Comando da placa: anterior");
        } else if (command == 3) {
            setStatus("Comando da placa: play/pause");
        } else {
            setStatus("Comando desconhecido da placa: " + command);
            return;
        }

        SpotifyNotificationListener.handleMediaCommand(context, command);
    }

    private void updateCoverProgress(WriteOp op) {
        if (op == null || currentCoverBytesTotal <= 0) {
            return;
        }
        if (op.transferId != activeTransferId) {
            return;
        }
        if (!COVER_DATA_UUID.equals(op.characteristic.getUuid())) {
            return;
        }

        currentCoverBytesSent += op.data.length;
        int sentKb = currentCoverBytesSent / 1024;
        int totalKb = Math.max(1, currentCoverBytesTotal / 1024);
        setStatus("Enviando capa: " + sentKb + "/" + totalKb + " KB");
    }

    private boolean hasScanPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return context.checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN)
                    == PackageManager.PERMISSION_GRANTED;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return context.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)
                    == PackageManager.PERMISSION_GRANTED;
        }
        return true;
    }

    private boolean hasConnectPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return context.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)
                    == PackageManager.PERMISSION_GRANTED;
        }
        return true;
    }

    private String clean(String value) {
        return value == null ? "" : value.trim().replace(';', '-');
    }

    private void setStatus(String status) {
        if (listener != null) {
            mainHandler.post(() -> listener.onStatus(status));
        }
    }

    private void notifyConnection(boolean connected) {
        if (listener != null) {
            mainHandler.post(() -> listener.onConnectionChanged(connected));
        }
    }

    private void notifySpotify(String title, String artist, boolean hasCover) {
        if (listener != null) {
            mainHandler.post(() -> listener.onSpotifyTrack(title, artist, hasCover));
        }
    }

    private static class WriteOp {
        final BluetoothGattCharacteristic characteristic;
        final byte[] data;
        final int writeType;
        final boolean waitForCallback;
        final int transferId;

        WriteOp(BluetoothGattCharacteristic characteristic, byte[] data,
                int writeType, boolean waitForCallback, int transferId) {
            this.characteristic = characteristic;
            this.data = data;
            this.writeType = writeType;
            this.waitForCallback = waitForCallback;
            this.transferId = transferId;
        }
    }
}
