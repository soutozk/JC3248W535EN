package com.cyberdeck.spotifybridge;

import android.Manifest;
import android.app.Activity;
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
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

public class MainActivity extends Activity implements BleBridge.Listener {
    private TextView statusView;
    private TextView spotifyView;
    private EditText titleEdit;
    private EditText artistEdit;
    private Button sendButton;
    private Button notificationButton;

    private BleBridge bridge;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        bridge = BleBridge.get(this);
        bridge.setListener(this);

        setContentView(createLayout());
        requestNeededPermissions();
        updateNotificationButton();
    }

    @Override
    protected void onResume() {
        super.onResume();
        updateNotificationButton();
        SpotifyNotificationListener.pushActiveSpotify(this);
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

        return root;
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
                    Manifest.permission.ACCESS_FINE_LOCATION
            }, 1);
        }
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
        return enabledListeners.toLowerCase().contains(componentName.flattenToString().toLowerCase());
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
    protected void onDestroy() {
        bridge.setListener(null);
        super.onDestroy();
    }
}
