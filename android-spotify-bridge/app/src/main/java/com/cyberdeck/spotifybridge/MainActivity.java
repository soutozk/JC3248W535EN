package com.cyberdeck.spotifybridge;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.nio.charset.StandardCharsets;
import java.util.UUID;

public class MainActivity extends Activity {
    private static final String DEVICE_NAME = "CyberDeck_Spotify";

    private static final UUID SERVICE_UUID =
            UUID.fromString("f38a0001-82eb-4a73-a38c-ce98c9438012");
    private static final UUID TRACK_UUID =
            UUID.fromString("f38a0002-82eb-4a73-a38c-ce98c9438012");

    private final Handler handler = new Handler(Looper.getMainLooper());

    private TextView statusView;
    private EditText titleEdit;
    private EditText artistEdit;
    private Button scanButton;
    private Button sendButton;

    private BluetoothAdapter bluetoothAdapter;
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic trackCharacteristic;
    private boolean scanning;

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
            setStatus("Falha no scan BLE: " + errorCode);
            scanning = false;
        }
    };

    private final BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                setStatus("Conectado. Descobrindo servicos...");
                if (hasConnectPermission()) {
                    gatt.discoverServices();
                }
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                trackCharacteristic = null;
                setStatus("Desconectado");
                runOnUiThread(() -> sendButton.setEnabled(false));
                closeGatt();
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
            if (trackCharacteristic == null) {
                setStatus("Caracteristica de musica nao encontrada");
                return;
            }

            setStatus("Pronto para enviar titulo/artista");
            runOnUiThread(() -> sendButton.setEnabled(true));
        }

        @Override
        public void onCharacteristicWrite(BluetoothGatt gatt,
                                          BluetoothGattCharacteristic characteristic,
                                          int status) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                setStatus("Enviado para ESP32");
            } else {
                setStatus("Falha ao enviar: " + status);
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        BluetoothManager manager = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
        bluetoothAdapter = manager != null ? manager.getAdapter() : null;
        scanner = bluetoothAdapter != null ? bluetoothAdapter.getBluetoothLeScanner() : null;

        setContentView(createLayout());
        requestNeededPermissions();

        if (scanner == null) {
            setStatus("Bluetooth LE indisponivel");
            scanButton.setEnabled(false);
        }
    }

    private View createLayout() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(32, 36, 32, 32);
        root.setGravity(Gravity.CENTER_HORIZONTAL);

        TextView title = new TextView(this);
        title.setText("CyberDeck Spotify Bridge");
        title.setTextSize(22);
        title.setGravity(Gravity.CENTER);
        root.addView(title, matchWrap());

        statusView = new TextView(this);
        statusView.setText("Aguardando");
        statusView.setTextSize(16);
        statusView.setPadding(0, 24, 0, 24);
        root.addView(statusView, matchWrap());

        scanButton = new Button(this);
        scanButton.setText("SCAN / CONECTAR");
        scanButton.setOnClickListener(v -> startScan());
        root.addView(scanButton, matchWrap());

        titleEdit = new EditText(this);
        titleEdit.setHint("Titulo");
        titleEdit.setSingleLine(true);
        titleEdit.setText("Teste Musica");
        root.addView(titleEdit, matchWrap());

        artistEdit = new EditText(this);
        artistEdit.setHint("Artista");
        artistEdit.setSingleLine(true);
        artistEdit.setText("Teste Artista");
        root.addView(artistEdit, matchWrap());

        sendButton = new Button(this);
        sendButton.setText("ENVIAR PARA ESP32");
        sendButton.setEnabled(false);
        sendButton.setOnClickListener(v -> sendTrack());
        root.addView(sendButton, matchWrap());

        return root;
    }

    private LinearLayout.LayoutParams matchWrap() {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        params.setMargins(0, 10, 0, 10);
        return params;
    }

    private void requestNeededPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            requestPermissions(new String[] {
                    Manifest.permission.BLUETOOTH_SCAN,
                    Manifest.permission.BLUETOOTH_CONNECT
            }, 1);
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            requestPermissions(new String[] {
                    Manifest.permission.ACCESS_FINE_LOCATION
            }, 1);
        }
    }

    private void startScan() {
        if (!hasScanPermission()) {
            requestNeededPermissions();
            setStatus("Permissao Bluetooth necessaria");
            return;
        }
        if (scanner == null) {
            setStatus("Scanner BLE indisponivel");
            return;
        }

        closeGatt();
        setStatus("Procurando CyberDeck_Spotify...");
        scanning = true;
        scanner.startScan(scanCallback);

        handler.postDelayed(() -> {
            if (scanning) {
                stopScan();
                setStatus("Nao encontrou CyberDeck_Spotify");
            }
        }, 12000);
    }

    private void stopScan() {
        if (scanner != null && scanning && hasScanPermission()) {
            scanner.stopScan(scanCallback);
        }
        scanning = false;
    }

    private void connect(BluetoothDevice device) {
        if (!hasConnectPermission()) {
            requestNeededPermissions();
            setStatus("Permissao de conexao Bluetooth necessaria");
            return;
        }
        gatt = device.connectGatt(this, false, gattCallback);
    }

    private void sendTrack() {
        if (gatt == null || trackCharacteristic == null) {
            setStatus("Nao conectado");
            return;
        }
        if (!hasConnectPermission()) {
            requestNeededPermissions();
            return;
        }

        String payload = titleEdit.getText().toString() + ";" + artistEdit.getText().toString();
        byte[] data = payload.getBytes(StandardCharsets.UTF_8);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            gatt.writeCharacteristic(trackCharacteristic, data, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
        } else {
            trackCharacteristic.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
            trackCharacteristic.setValue(data);
            gatt.writeCharacteristic(trackCharacteristic);
        }

        setStatus("Enviando: " + payload);
    }

    private boolean hasScanPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
        }
        return true;
    }

    private boolean hasConnectPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
        }
        return true;
    }

    private void setStatus(String text) {
        runOnUiThread(() -> statusView.setText(text));
    }

    private void closeGatt() {
        if (gatt != null && hasConnectPermission()) {
            gatt.close();
        }
        gatt = null;
        trackCharacteristic = null;
    }

    @Override
    protected void onDestroy() {
        stopScan();
        closeGatt();
        super.onDestroy();
    }
}
