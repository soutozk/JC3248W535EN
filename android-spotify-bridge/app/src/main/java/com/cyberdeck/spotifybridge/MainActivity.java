package com.cyberdeck.spotifybridge;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;

import com.cyberdeck.spotifybridge.obd.ObdManager;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Locale;
import java.util.Set;

public class MainActivity extends Activity implements BleBridge.Listener, ObdManager.Listener {
    private TextView statusView;
    private TextView spotifyView;
    private EditText titleEdit;
    private EditText artistEdit;
    private Button sendButton;
    private Button notificationButton;
    private TextView obdStatusView;
    private TextView diagnosticsView;
    private Spinner elmSpinner;
    private final ArrayList<BluetoothDevice> bondedDevices = new ArrayList<>();

    private BleBridge bridge;
    private ObdManager obdManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        bridge = BleBridge.get(this);
        bridge.setListener(this);
        obdManager = new ObdManager(this, bridge);
        obdManager.setListener(this);

        setContentView(createLayout());
        requestNeededPermissions();
        updateNotificationButton();
        populateBondedDevices();
    }

    @Override
    protected void onResume() {
        super.onResume();
        updateNotificationButton();
        SpotifyNotificationListener.pushActiveSpotify(this);
        populateBondedDevices();
    }

    private View createLayout() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(32, 36, 32, 32);
        root.setGravity(Gravity.CENTER_HORIZONTAL);

        ScrollView scroll = new ScrollView(this);
        TextView title = new TextView(this);
        title.setText("CyberDeck Bridge");
        title.setTextSize(22);
        title.setGravity(Gravity.CENTER);
        root.addView(title, matchWrap());

        statusView = new TextView(this);
        statusView.setText("Aguardando");
        statusView.setTextSize(16);
        statusView.setPadding(0, 18, 0, 18);
        root.addView(statusView, matchWrap());

        spotifyView = new TextView(this);
        spotifyView.setText("Spotify: nenhum dado recebido");
        spotifyView.setTextSize(16);
        root.addView(spotifyView, matchWrap());

        Button scanButton = new Button(this);
        scanButton.setText("SCAN / CONECTAR");
        scanButton.setOnClickListener(v -> bridge.startScan());
        root.addView(scanButton, matchWrap());

        TextView obdTitle = new TextView(this);
        obdTitle.setText("OBD-II / ELM327 Bluetooth classico");
        obdTitle.setTextSize(19);
        obdTitle.setPadding(0, 24, 0, 8);
        root.addView(obdTitle, matchWrap());

        obdStatusView = new TextView(this);
        obdStatusView.setText("OBD: desconectado");
        root.addView(obdStatusView, matchWrap());

        elmSpinner = new Spinner(this);
        root.addView(elmSpinner, matchWrap());

        Button refreshElmButton = new Button(this);
        refreshElmButton.setText("ATUALIZAR DISPOSITIVOS PAREADOS");
        refreshElmButton.setOnClickListener(v -> populateBondedDevices());
        root.addView(refreshElmButton, matchWrap());

        Button connectElmButton = new Button(this);
        connectElmButton.setText("CONECTAR ELM327");
        connectElmButton.setOnClickListener(v -> connectSelectedElm());
        root.addView(connectElmButton, matchWrap());

        Button disconnectElmButton = new Button(this);
        disconnectElmButton.setText("DESCONECTAR OBD");
        disconnectElmButton.setOnClickListener(v -> obdManager.disconnect());
        root.addView(disconnectElmButton, matchWrap());

        diagnosticsView = new TextView(this);
        diagnosticsView.setText("Diagnostico OBD sem eventos");
        diagnosticsView.setTextSize(12);
        root.addView(diagnosticsView, matchWrap());

        Button diagnosticsButton = new Button(this);
        diagnosticsButton.setText("ATUALIZAR LOG OBD");
        diagnosticsButton.setOnClickListener(v -> {
            java.util.List<String> lines = obdManager.logs().snapshot();
            int start = Math.max(0, lines.size() - 12);
            diagnosticsView.setText(android.text.TextUtils.join("\n", lines.subList(start, lines.size())));
        });
        root.addView(diagnosticsButton, matchWrap());

        notificationButton = new Button(this);
        notificationButton.setText("ATIVAR ACESSO AO SPOTIFY");
        notificationButton.setOnClickListener(v ->
                startActivity(new Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)));
        root.addView(notificationButton, matchWrap());

        Button refreshButton = new Button(this);
        refreshButton.setText("LER SPOTIFY AGORA");
        refreshButton.setOnClickListener(v -> SpotifyNotificationListener.pushActiveSpotify(this));
        root.addView(refreshButton, matchWrap());

        titleEdit = new EditText(this);
        titleEdit.setHint("Titulo manual");
        titleEdit.setSingleLine(true);
        titleEdit.setText("Teste Musica");
        root.addView(titleEdit, matchWrap());

        artistEdit = new EditText(this);
        artistEdit.setHint("Artista manual");
        artistEdit.setSingleLine(true);
        artistEdit.setText("Teste Artista");
        root.addView(artistEdit, matchWrap());

        sendButton = new Button(this);
        sendButton.setText("ENVIAR MANUAL");
        sendButton.setEnabled(false);
        sendButton.setOnClickListener(v ->
                bridge.sendManualTrack(titleEdit.getText().toString(), artistEdit.getText().toString()));
        root.addView(sendButton, matchWrap());

        scroll.addView(root);
        return scroll;
    }

    private LinearLayout.LayoutParams matchWrap() {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        params.setMargins(0, 8, 0, 8);
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
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
            }, 1);
        }
    }

    private void populateBondedDevices() {
        if (elmSpinner == null) return;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S &&
                checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED)
            return;
        BluetoothManager manager = (BluetoothManager) getSystemService(BLUETOOTH_SERVICE);
        BluetoothAdapter adapter = manager == null ? null : manager.getAdapter();
        Set<BluetoothDevice> bonded = adapter == null ? null : adapter.getBondedDevices();
        bondedDevices.clear();
        if (bonded != null) bondedDevices.addAll(bonded);
        Collections.sort(bondedDevices, (left, right) -> {
            String leftName = left.getName();
            String rightName = right.getName();
            String leftLabel = leftName == null ? left.getAddress() : leftName;
            String rightLabel = rightName == null ? right.getAddress() : rightName;
            return leftLabel.compareToIgnoreCase(rightLabel);
        });
        ArrayList<String> labels = new ArrayList<>();
        for (BluetoothDevice device : bondedDevices) {
            String name = device.getName();
            labels.add((name == null ? "Sem nome" : name) + " / " + device.getAddress());
        }
        if (labels.isEmpty()) labels.add("Nenhum dispositivo pareado");
        elmSpinner.setAdapter(new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_dropdown_item, labels));
        String saved = getPreferences(MODE_PRIVATE).getString("elm_address", "");
        for (int i = 0; i < bondedDevices.size(); i++)
            if (bondedDevices.get(i).getAddress().equals(saved)) elmSpinner.setSelection(i);
    }

    private void connectSelectedElm() {
        int selected = elmSpinner == null ? -1 : elmSpinner.getSelectedItemPosition();
        if (selected < 0 || selected >= bondedDevices.size()) {
            onObdStatus("Selecione um ELM327 ja pareado");
            return;
        }
        BluetoothDevice device = bondedDevices.get(selected);
        getPreferences(MODE_PRIVATE).edit().putString("elm_address", device.getAddress()).apply();
        obdManager.connect(device);
    }

    private void updateNotificationButton() {
        if (notificationButton == null) {
            return;
        }
        boolean enabled = isNotificationListenerEnabled();
        notificationButton.setText(enabled
                ? "ACESSO AO SPOTIFY ATIVO"
                : "ATIVAR ACESSO AO SPOTIFY");
        notificationButton.setEnabled(!enabled);
    }

    private boolean isNotificationListenerEnabled() {
        String enabledListeners = Settings.Secure.getString(
                getContentResolver(), "enabled_notification_listeners");
        if (TextUtils.isEmpty(enabledListeners)) {
            return false;
        }
        ComponentName componentName = new ComponentName(this, SpotifyNotificationListener.class);
        return enabledListeners.toLowerCase(Locale.ROOT)
                .contains(componentName.flattenToString().toLowerCase(Locale.ROOT));
    }

    @Override
    public void onStatus(String status) {
        statusView.setText(status);
    }

    @Override
    public void onConnectionChanged(boolean connected) {
        sendButton.setEnabled(connected && bridge.isReady());
    }

    @Override
    public void onSpotifyTrack(String title, String artist, boolean hasCover) {
        spotifyView.setText("Spotify: " + title + " - " + artist
                + (hasCover ? " + capa" : ""));
    }

    @Override
    public void onObdStatus(String status) {
        runOnUiThread(() -> {
            if (obdStatusView != null) obdStatusView.setText("OBD: " + status);
        });
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        populateBondedDevices();
    }

    @Override
    protected void onDestroy() {
        bridge.setListener(null);
        obdManager.setListener(null);
        obdManager.shutdown();
        super.onDestroy();
    }
}
