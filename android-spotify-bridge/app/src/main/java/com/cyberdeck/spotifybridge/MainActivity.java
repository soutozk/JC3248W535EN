package com.cyberdeck.spotifybridge;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;

import com.cyberdeck.spotifybridge.obd.ObdManager;
import com.cyberdeck.spotifybridge.obd.models.ObdConnectionState;
import com.cyberdeck.spotifybridge.obd.models.ObdDtc;
import com.cyberdeck.spotifybridge.obd.models.ObdPid;
import com.cyberdeck.spotifybridge.obd.repository.ObdRepository;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Locale;
import java.util.List;
import java.util.Set;

public class MainActivity extends Activity implements BleBridge.Listener, ObdManager.Listener {
    private static final int BG = Color.rgb(11, 17, 32);
    private static final int PANEL = Color.rgb(18, 29, 48);
    private static final int PANEL_LIGHT = Color.rgb(25, 42, 65);
    private static final int TEXT = Color.rgb(244, 247, 251);
    private static final int MUTED = Color.rgb(145, 164, 189);
    private static final int GREEN = Color.rgb(53, 214, 164);
    private static final int BLUE = Color.rgb(91, 156, 255);
    private static final int RED = Color.rgb(255, 105, 115);

    private TextView statusView;
    private TextView spotifyView;
    private EditText titleEdit;
    private EditText artistEdit;
    private Button sendButton;
    private Button notificationButton;
    private TextView obdStatusView;
    private TextView obdConnectionView;
    private TextView obdDetailView;
    private TextView diagnosticsView;
    private TextView dtcView;
    private TextView rpmValue;
    private TextView speedValue;
    private TextView coolantValue;
    private TextView voltageValue;
    private Spinner elmSpinner;
    private FrameLayout pageContainer;
    private View obdPage;
    private View spotifyPage;
    private TextView obdTab;
    private TextView spotifyTab;
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
        renderObdData(obdManager.snapshot());
        updateObdConnectionLabel();
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
        updateObdConnectionLabel();
    }

    private View createLayout() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        root.addView(createToolbar(), new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        pageContainer = new FrameLayout(this);
        pageContainer.setBackgroundColor(BG);
        root.addView(pageContainer, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));

        obdPage = createObdPage();
        spotifyPage = createSpotifyPage();
        pageContainer.addView(obdPage, fillParams());
        pageContainer.addView(spotifyPage, fillParams());
        spotifyPage.setVisibility(View.GONE);

        root.addView(createBottomBar(), new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(72)));
        return root;
    }

    private View createToolbar() {
        LinearLayout toolbar = new LinearLayout(this);
        toolbar.setOrientation(LinearLayout.VERTICAL);
        toolbar.setPadding(dp(24), dp(22), dp(24), dp(16));
        toolbar.addView(text("CyberDeck", 26, TEXT, Typeface.BOLD), wrapParams());
        TextView subtitle = text("PAINEL DE VEÍCULO E ENTRETENIMENTO", 11, MUTED, Typeface.BOLD);
        subtitle.setLetterSpacing(0.08f);
        toolbar.addView(subtitle, topMarginParams(6));
        return toolbar;
    }

    private View createObdPage() {
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setClipToPadding(false);
        LinearLayout content = verticalContent();
        content.addView(sectionTitle("OBD2 / PAINEL DO VEÍCULO", "Dados recebidos diretamente da ECU"));

        LinearLayout connectionCard = cardLayout();
        obdConnectionView = text("ECU DESCONECTADA", 16, RED, Typeface.BOLD);
        connectionCard.addView(obdConnectionView, wrapParams());
        obdStatusView = text("Conecte um ELM327 para começar", 14, TEXT, Typeface.NORMAL);
        connectionCard.addView(obdStatusView, topMarginParams(8));
        obdDetailView = text("ELM: desconectado  •  Protocolo: --", 12, MUTED, Typeface.NORMAL);
        connectionCard.addView(obdDetailView, topMarginParams(8));
        content.addView(connectionCard, marginParams(0, 0, 0, 14));

        content.addView(sectionTitle("LEITURAS PRINCIPAIS", "Atualização automática"), topMarginParams(2));
        LinearLayout metrics = new LinearLayout(this);
        metrics.setOrientation(LinearLayout.VERTICAL);
        LinearLayout firstRow = new LinearLayout(this);
        firstRow.setOrientation(LinearLayout.HORIZONTAL);
        rpmValue = metricValue("--");
        speedValue = metricValue("--");
        firstRow.addView(metricCard("RPM", "rpm", rpmValue), weightedParams(1, 6));
        firstRow.addView(metricCard("VELOCIDADE", "km/h", speedValue), weightedParams(1, 6));
        metrics.addView(firstRow, wrapParams());

        LinearLayout secondRow = new LinearLayout(this);
        secondRow.setOrientation(LinearLayout.HORIZONTAL);
        coolantValue = metricValue("--");
        voltageValue = metricValue("--");
        secondRow.addView(metricCard("TEMPERATURA", "°C", coolantValue), weightedParams(1, 6));
        secondRow.addView(metricCard("TENSÃO", "V", voltageValue), weightedParams(1, 6));
        metrics.addView(secondRow, topMarginParams(12));
        content.addView(metrics, wrapParams());

        content.addView(sectionTitle("CONEXÃO", "Primeiro conecte o painel BLE e depois o ELM327"), topMarginParams(22));
        Button scanButton = primaryButton("PROCURAR E CONECTAR PAINEL BLE");
        scanButton.setOnClickListener(v -> bridge.startScan());
        content.addView(scanButton, wrapParams());

        elmSpinner = new Spinner(this);
        content.addView(elmSpinner, topMarginParams(12));
        Button refreshElmButton = secondaryButton("Atualizar dispositivos pareados");
        refreshElmButton.setOnClickListener(v -> populateBondedDevices());
        content.addView(refreshElmButton, topMarginParams(8));

        LinearLayout elmActions = new LinearLayout(this);
        elmActions.setOrientation(LinearLayout.HORIZONTAL);
        Button connectElmButton = primaryButton("CONECTAR ELM327");
        connectElmButton.setOnClickListener(v -> connectSelectedElm());
        Button disconnectElmButton = secondaryButton("DESCONECTAR");
        disconnectElmButton.setOnClickListener(v -> obdManager.disconnect());
        elmActions.addView(connectElmButton, weightedParams(1, 6));
        elmActions.addView(disconnectElmButton, weightedParams(1, 6));
        content.addView(elmActions, topMarginParams(8));

        content.addView(sectionTitle("DIAGNÓSTICO", "Últimos eventos da conexão"), topMarginParams(22));
        diagnosticsView = text("Nenhum evento registrado", 12, MUTED, Typeface.NORMAL);
        diagnosticsView.setTypeface(Typeface.MONOSPACE, Typeface.NORMAL);
        diagnosticsView.setMaxLines(8);
        diagnosticsView.setEllipsize(TextUtils.TruncateAt.END);
        LinearLayout diagnosticsCard = cardLayout();
        diagnosticsCard.addView(diagnosticsView, wrapParams());
        content.addView(diagnosticsCard, topMarginParams(8));
        Button diagnosticsButton = secondaryButton("Atualizar log");
        diagnosticsButton.setOnClickListener(v -> refreshDiagnostics());
        content.addView(diagnosticsButton, topMarginParams(8));

        content.addView(sectionTitle("CÓDIGOS DE FALHA", "Leitura da ECU: atuais e pendentes"), topMarginParams(22));
        LinearLayout dtcCard = cardLayout();
        dtcView = text("Nenhum código lido", 13, MUTED, Typeface.NORMAL);
        dtcView.setTypeface(Typeface.MONOSPACE, Typeface.NORMAL);
        dtcCard.addView(dtcView, wrapParams());
        content.addView(dtcCard, topMarginParams(8));
        Button readDtcButton = secondaryButton("LER CÓDIGOS DA ECU");
        readDtcButton.setOnClickListener(v -> obdManager.readDtc());
        content.addView(readDtcButton, topMarginParams(8));
        scroll.addView(content);
        return scroll;
    }

    private View createSpotifyPage() {
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setClipToPadding(false);
        LinearLayout content = verticalContent();
        content.addView(sectionTitle("SPOTIFY", "Controle e envie a música para o CyberDeck"));

        LinearLayout statusCard = cardLayout();
        statusView = text("Aguardando conexão BLE", 16, TEXT, Typeface.BOLD);
        statusCard.addView(statusView, wrapParams());
        spotifyView = text("Nenhuma música recebida", 15, MUTED, Typeface.NORMAL);
        statusCard.addView(spotifyView, topMarginParams(10));
        content.addView(statusCard, marginParams(0, 0, 0, 14));

        notificationButton = primaryButton("ATIVAR ACESSO AO SPOTIFY");
        notificationButton.setOnClickListener(v ->
                startActivity(new Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)));
        content.addView(notificationButton, wrapParams());
        Button refreshButton = secondaryButton("Ler Spotify agora");
        refreshButton.setOnClickListener(v -> SpotifyNotificationListener.pushActiveSpotify(this));
        content.addView(refreshButton, topMarginParams(8));

        content.addView(sectionTitle("TESTE MANUAL", "Útil para validar a comunicação BLE"), topMarginParams(24));
        titleEdit = field("Título da música", "Teste Música");
        artistEdit = field("Artista", "Teste Artista");
        content.addView(titleEdit, wrapParams());
        content.addView(artistEdit, topMarginParams(8));
        sendButton = primaryButton("ENVIAR PARA O CYBERDECK");
        sendButton.setEnabled(false);
        sendButton.setOnClickListener(v ->
                bridge.sendManualTrack(titleEdit.getText().toString(), artistEdit.getText().toString()));
        content.addView(sendButton, topMarginParams(12));

        LinearLayout hint = cardLayout();
        hint.addView(text("Conecte o painel BLE na aba OBD2 para liberar o envio.",
                12, MUTED, Typeface.NORMAL), wrapParams());
        content.addView(hint, topMarginParams(22));
        scroll.addView(content);
        return scroll;
    }

    private View createBottomBar() {
        LinearLayout bar = new LinearLayout(this);
        bar.setOrientation(LinearLayout.HORIZONTAL);
        bar.setGravity(Gravity.CENTER);
        bar.setPadding(dp(8), dp(8), dp(8), dp(8));
        bar.setBackground(rounded(PANEL, 18));
        obdTab = navigationItem("◉  OBD2");
        spotifyTab = navigationItem("♫  SPOTIFY");
        obdTab.setOnClickListener(v -> showPage(false));
        spotifyTab.setOnClickListener(v -> showPage(true));
        bar.addView(obdTab, weightedParams(1, 4));
        bar.addView(spotifyTab, weightedParams(1, 4));
        showPage(false);
        return bar;
    }

    private void showPage(boolean spotify) {
        if (obdPage == null || spotifyPage == null) return;
        obdPage.setVisibility(spotify ? View.GONE : View.VISIBLE);
        spotifyPage.setVisibility(spotify ? View.VISIBLE : View.GONE);
        obdTab.setTextColor(spotify ? MUTED : GREEN);
        spotifyTab.setTextColor(spotify ? BLUE : MUTED);
        obdTab.setBackground(spotify ? null : rounded(PANEL_LIGHT, 14));
        spotifyTab.setBackground(spotify ? rounded(PANEL_LIGHT, 14) : null);
    }

    private void renderObdData(ObdRepository.Snapshot snapshot) {
        if (snapshot == null || rpmValue == null) return;
        rpmValue.setText(reading(snapshot, ObdPid.RPM, "%.0f"));
        speedValue.setText(reading(snapshot, ObdPid.SPEED, "%.0f"));
        coolantValue.setText(reading(snapshot, ObdPid.COOLANT, "%.0f"));
        voltageValue.setText(reading(snapshot, ObdPid.CONTROL_VOLTAGE, "%.1f"));
    }

    private String reading(ObdRepository.Snapshot snapshot, ObdPid pid, String format) {
        if ((snapshot.validMask & pid.bit()) == 0) {
            return (snapshot.supportedMask & pid.bit()) == 0 ? "N/D" : "--";
        }
        return String.format(Locale.US, format, snapshot.get(pid));
    }

    private void updateObdConnectionLabel() {
        if (obdConnectionView == null || obdManager == null) return;
        ObdConnectionState state = obdManager.getState();
        String label;
        int color;
        if (state == ObdConnectionState.READY) {
            label = "ECU CONECTADA";
            color = GREEN;
        } else if (state == ObdConnectionState.DEGRADED) {
            label = "ECU INSTÁVEL";
            color = Color.rgb(255, 190, 80);
        } else if (state == ObdConnectionState.DISCONNECTED || state == ObdConnectionState.ERROR) {
            label = "ECU DESCONECTADA";
            color = RED;
        } else {
            label = "CONECTANDO À ECU";
            color = BLUE;
        }
        obdConnectionView.setText(label);
        obdConnectionView.setTextColor(color);
    }

    private void refreshDiagnostics() {
        java.util.List<String> lines = obdManager.logs().snapshot();
        int start = Math.max(0, lines.size() - 8);
        diagnosticsView.setText(lines.isEmpty()
                ? "Nenhum evento registrado"
                : TextUtils.join("\n", lines.subList(start, lines.size())));
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
        Collections.sort(bondedDevices, (left, right) -> deviceLabel(left).compareToIgnoreCase(deviceLabel(right)));
        ArrayList<String> labels = new ArrayList<>();
        for (BluetoothDevice device : bondedDevices) labels.add(deviceLabel(device));
        if (labels.isEmpty()) labels.add("Nenhum dispositivo pareado");
        elmSpinner.setAdapter(new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_dropdown_item, labels));
        String saved = getPreferences(MODE_PRIVATE).getString("elm_address", "");
        for (int i = 0; i < bondedDevices.size(); i++)
            if (bondedDevices.get(i).getAddress().equals(saved)) elmSpinner.setSelection(i);
    }

    private String deviceLabel(BluetoothDevice device) {
        String name = device.getName();
        return (name == null ? "Sem nome" : name) + " / " + device.getAddress();
    }

    private void connectSelectedElm() {
        int selected = elmSpinner == null ? -1 : elmSpinner.getSelectedItemPosition();
        if (selected < 0 || selected >= bondedDevices.size()) {
            onObdStatus("Selecione um ELM327 já pareado");
            return;
        }
        BluetoothDevice device = bondedDevices.get(selected);
        getPreferences(MODE_PRIVATE).edit().putString("elm_address", device.getAddress()).apply();
        obdManager.connect(device);
    }

    private void updateNotificationButton() {
        if (notificationButton == null) return;
        boolean enabled = isNotificationListenerEnabled();
        notificationButton.setText(enabled ? "ACESSO AO SPOTIFY ATIVO" : "ATIVAR ACESSO AO SPOTIFY");
        notificationButton.setEnabled(!enabled);
    }

    private boolean isNotificationListenerEnabled() {
        String enabledListeners = Settings.Secure.getString(
                getContentResolver(), "enabled_notification_listeners");
        if (TextUtils.isEmpty(enabledListeners)) return false;
        ComponentName componentName = new ComponentName(this, SpotifyNotificationListener.class);
        return enabledListeners.toLowerCase(Locale.ROOT)
                .contains(componentName.flattenToString().toLowerCase(Locale.ROOT));
    }

    @Override
    public void onStatus(String status) {
        runOnUiThread(() -> {
            if (statusView != null) statusView.setText(status);
        });
    }

    @Override
    public void onConnectionChanged(boolean connected) {
        runOnUiThread(() -> {
            if (sendButton != null) sendButton.setEnabled(connected && bridge.isReady());
            if (statusView != null && connected) statusView.setText("Painel BLE conectado");
        });
    }

    @Override
    public void onSpotifyTrack(String title, String artist, boolean hasCover) {
        runOnUiThread(() -> {
            if (spotifyView != null) {
                spotifyView.setText(title + "  •  " + artist + (hasCover ? "\nCapa disponível" : ""));
            }
        });
    }

    @Override
    public void onObdStatus(String status) {
        runOnUiThread(() -> {
            if (obdStatusView != null) obdStatusView.setText(status);
            updateObdConnectionLabel();
            if (obdDetailView != null) {
                obdDetailView.setText("Estado: " + obdManager.getState().name().replace('_', ' '));
            }
        });
    }

    @Override
    public void onObdData(ObdRepository.Snapshot snapshot) {
        runOnUiThread(() -> {
            renderObdData(snapshot);
            updateObdConnectionLabel();
        });
    }

    @Override
    public void onDtcData(List<ObdDtc> codes) {
        runOnUiThread(() -> {
            if (dtcView == null) return;
            if (codes == null || codes.isEmpty()) {
                dtcView.setText("Nenhum código atual ou pendente");
                return;
            }
            StringBuilder current = new StringBuilder();
            StringBuilder pending = new StringBuilder();
            for (ObdDtc code : codes) {
                if (code == null) continue;
                StringBuilder target = code.pending ? pending : current;
                if (target.length() > 0) target.append(", ");
                target.append(code.code);
            }
            StringBuilder display = new StringBuilder();
            display.append("Atuais: ").append(current.length() == 0 ? "nenhum" : current);
            display.append("\nPendentes: ").append(pending.length() == 0 ? "nenhum" : pending);
            dtcView.setText(display.toString());
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

    private LinearLayout verticalContent() {
        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(dp(20), dp(4), dp(20), dp(24));
        return content;
    }

    private LinearLayout sectionTitle(String title, String subtitle) {
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        TextView titleView = text(title, 13, GREEN, Typeface.BOLD);
        titleView.setLetterSpacing(0.06f);
        box.addView(titleView, wrapParams());
        TextView subtitleView = text(subtitle, 12, MUTED, Typeface.NORMAL);
        box.addView(subtitleView, topMarginParams(4));
        return box;
    }

    private LinearLayout cardLayout() {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(16), dp(16), dp(16), dp(16));
        card.setBackground(rounded(PANEL, 16));
        return card;
    }

    private View metricCard(String label, String unit, TextView value) {
        LinearLayout card = cardLayout();
        card.setPadding(dp(14), dp(14), dp(10), dp(14));
        TextView title = text(label, 11, MUTED, Typeface.BOLD);
        title.setLetterSpacing(0.04f);
        card.addView(title, wrapParams());
        card.addView(value, topMarginParams(10));
        card.addView(text(unit, 11, MUTED, Typeface.NORMAL), topMarginParams(2));
        return card;
    }

    private TextView metricValue(String initial) {
        return text(initial, 28, TEXT, Typeface.BOLD);
    }

    private TextView navigationItem(String label) {
        TextView item = text(label, 13, MUTED, Typeface.BOLD);
        item.setGravity(Gravity.CENTER);
        item.setPadding(0, dp(12), 0, dp(12));
        return item;
    }

    private EditText field(String hint, String value) {
        EditText field = new EditText(this);
        field.setHint(hint);
        field.setText(value);
        field.setTextColor(TEXT);
        field.setHintTextColor(MUTED);
        field.setSingleLine(true);
        field.setTextSize(15);
        field.setPadding(dp(14), 0, dp(14), 0);
        field.setBackground(rounded(PANEL, 12));
        return field;
    }

    private Button primaryButton(String label) {
        Button button = new Button(this);
        button.setText(label);
        button.setTextColor(BG);
        button.setTextSize(12);
        button.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        button.setAllCaps(false);
        button.setMinHeight(dp(48));
        button.setPadding(dp(12), 0, dp(12), 0);
        button.setBackground(rounded(GREEN, 12));
        return button;
    }

    private Button secondaryButton(String label) {
        Button button = new Button(this);
        button.setText(label);
        button.setTextColor(TEXT);
        button.setTextSize(12);
        button.setAllCaps(false);
        button.setMinHeight(dp(44));
        button.setBackground(rounded(PANEL_LIGHT, 12));
        return button;
    }

    private TextView text(String value, float size, int color, int style) {
        TextView view = new TextView(this);
        view.setText(value);
        view.setTextSize(size);
        view.setTextColor(color);
        view.setTypeface(Typeface.DEFAULT, style);
        return view;
    }

    private GradientDrawable rounded(int color, int radiusDp) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color);
        drawable.setCornerRadius(dp(radiusDp));
        return drawable;
    }

    private LinearLayout.LayoutParams fillParams() {
        return new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT);
    }

    private LinearLayout.LayoutParams wrapParams() {
        return new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private LinearLayout.LayoutParams topMarginParams(int top) {
        LinearLayout.LayoutParams params = wrapParams();
        params.topMargin = dp(top);
        return params;
    }

    private LinearLayout.LayoutParams marginParams(int left, int top, int right, int bottom) {
        LinearLayout.LayoutParams params = wrapParams();
        params.setMargins(dp(left), dp(top), dp(right), dp(bottom));
        return params;
    }

    private LinearLayout.LayoutParams weightedParams(float weight, int horizontalMargin) {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, weight);
        params.setMargins(dp(horizontalMargin / 2), 0, dp(horizontalMargin / 2), 0);
        return params;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}
