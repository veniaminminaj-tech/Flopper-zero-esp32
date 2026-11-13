package tech.veniaminminaj.flopperzero;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import com.hoho.android.usbserial.driver.UsbSerialDriver;
import com.hoho.android.usbserial.driver.UsbSerialPort;
import com.hoho.android.usbserial.driver.UsbSerialProber;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends Activity {
    private static final String ACTION_USB_PERMISSION =
            "tech.veniaminminaj.flopperzero.USB_PERMISSION";
    private static final int BAUD_RATE = 115200;

    private UsbManager usbManager;
    private UsbSerialPort serialPort;
    private ExecutorService reader;
    private TextView status;
    private TextView output;
    private ScrollView scroll;
    private Button connectButton;
    private boolean reading = false;

    private final BroadcastReceiver usbReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (ACTION_USB_PERMISSION.equals(intent.getAction())) {
                UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                boolean granted = intent.getBooleanExtra(
                        UsbManager.EXTRA_PERMISSION_GRANTED, false);
                if (granted && device != null) {
                    openDevice(device);
                } else {
                    setStatus("USB-доступ не надано");
                }
            }
        }
    };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);
        buildUi();

        IntentFilter filter = new IntentFilter(ACTION_USB_PERMISSION);
        if (Build.VERSION.SDK_INT >= 33) {
            registerReceiver(usbReceiver, filter, Context.RECEIVER_NOT_EXPORTED);
        } else {
            registerReceiver(usbReceiver, filter);
        }
        setStatus("Готово до підключення · 115200 baud");
        appendLog("Під'єднай ESP32 через USB OTG та натисни «Підключити».");
        appendLog("Режим лише читання: застосунок показує UART-лог і не надсилає команди.");
    }

    private void buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(18, 22, 18, 18);
        root.setBackgroundColor(0xFF101820);

        TextView title = new TextView(this);
        title.setText("FLOPPER ZERO");
        title.setTextColor(0xFF70E1C1);
        title.setTextSize(25);
        title.setGravity(Gravity.CENTER);
        root.addView(title, new LinearLayout.LayoutParams(-1, -2));

        TextView subtitle = new TextView(this);
        subtitle.setText("ANDROID USB OTG · SERIAL MONITOR");
        subtitle.setTextColor(0xFF9FB3C8);
        subtitle.setTextSize(11);
        subtitle.setGravity(Gravity.CENTER);
        subtitle.setPadding(0, 4, 0, 16);
        root.addView(subtitle);

        status = new TextView(this);
        status.setTextColor(0xFFFFFFFF);
        status.setTextSize(14);
        status.setPadding(12, 12, 12, 12);
        status.setBackgroundColor(0xFF203040);
        root.addView(status, new LinearLayout.LayoutParams(-1, -2));

        LinearLayout buttons = new LinearLayout(this);
        buttons.setOrientation(LinearLayout.HORIZONTAL);
        buttons.setPadding(0, 12, 0, 12);

        connectButton = makeButton("ПІДКЛЮЧИТИ");
        connectButton.setOnClickListener(v -> connectUsb());
        buttons.addView(connectButton, new LinearLayout.LayoutParams(0, 52, 1));

        Button disconnect = makeButton("ВІД'ЄДНАТИ");
        disconnect.setOnClickListener(v -> disconnectUsb());
        LinearLayout.LayoutParams dp = new LinearLayout.LayoutParams(0, 52, 1);
        dp.leftMargin = 8;
        buttons.addView(disconnect, dp);
        root.addView(buttons);

        scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        output = new TextView(this);
        output.setTextColor(0xFFD6E2EA);
        output.setTextSize(12);
        output.setPadding(12, 12, 12, 12);
        output.setTypeface(android.graphics.Typeface.MONOSPACE);
        output.setBackgroundColor(0xFF080D12);
        scroll.addView(output);
        root.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));

        LinearLayout bottom = new LinearLayout(this);
        bottom.setOrientation(LinearLayout.HORIZONTAL);
        Button clear = makeButton("ОЧИСТИТИ ЛОГ");
        clear.setOnClickListener(v -> output.setText(""));
        bottom.addView(clear, new LinearLayout.LayoutParams(0, 48, 1));

        Button info = makeButton("ПРО ПІДКЛЮЧЕННЯ");
        info.setOnClickListener(v -> appendLog(
                "ESP32-WROOM · USB UART · 115200 baud · 8N1\n" +
                "Потрібен USB OTG-адаптер і кабель із передаванням даних."));
        LinearLayout.LayoutParams ip = new LinearLayout.LayoutParams(0, 48, 1);
        ip.leftMargin = 8;
        bottom.addView(info, ip);
        root.addView(bottom);

        setContentView(root);
    }

    private Button makeButton(String text) {
        Button button = new Button(this);
        button.setText(text);
        button.setTextSize(11);
        button.setTextColor(0xFFFFFFFF);
        button.setBackgroundTintList(
                android.content.res.ColorStateList.valueOf(0xFF176B62));
        return button;
    }

    private void connectUsb() {
        List<UsbSerialDriver> drivers =
                UsbSerialProber.getDefaultProber().findAllDrivers(usbManager);

        if (drivers.isEmpty()) {
            setStatus("ESP32 не знайдено · перевір USB OTG");
            appendLog("USB serial-пристроїв не знайдено.");
            return;
        }

        UsbSerialDriver driver = drivers.get(0);
        UsbDevice device = driver.getDevice();

        if (!usbManager.hasPermission(device)) {
            int flags = PendingIntent.FLAG_UPDATE_CURRENT;
            if (Build.VERSION.SDK_INT >= 31) flags |= PendingIntent.FLAG_MUTABLE;
            PendingIntent permissionIntent = PendingIntent.getBroadcast(
                    this, 0, new Intent(ACTION_USB_PERMISSION).setPackage(getPackageName()),
                    flags);
            usbManager.requestPermission(device, permissionIntent);
            setStatus("Очікування дозволу USB…");
            return;
        }

        openDevice(device);
    }

    private void openDevice(UsbDevice device) {
        try {
            UsbSerialDriver selected = null;
            for (UsbSerialDriver driver :
                    UsbSerialProber.getDefaultProber().findAllDrivers(usbManager)) {
                if (driver.getDevice().getDeviceId() == device.getDeviceId()) {
                    selected = driver;
                    break;
                }
            }
            if (selected == null || selected.getPorts().isEmpty()) {
                setStatus("USB UART драйвер не знайдено");
                return;
            }

            UsbSerialPort port = selected.getPorts().get(0);
            android.hardware.usb.UsbDeviceConnection connection =
                    usbManager.openDevice(device);
            if (connection == null) {
                setStatus("Не вдалося відкрити USB-пристрій");
                return;
            }

            port.open(connection);
            port.setParameters(BAUD_RATE, 8, UsbSerialPort.STOPBITS_1,
                    UsbSerialPort.PARITY_NONE);
            serialPort = port;

            setStatus("ПІДКЛЮЧЕНО · " + device.getDeviceName());
            appendLog("UART відкрито. Очікування повідомлень ESP32…");
            startReader();
        } catch (Exception e) {
            setStatus("Помилка USB: " + e.getMessage());
            appendLog("Помилка: " + e);
            disconnectUsb();
        }
    }

    private void startReader() {
        reading = true;
        reader = Executors.newSingleThreadExecutor();
        reader.execute(() -> {
            byte[] buffer = new byte[256];
            while (reading && serialPort != null) {
                try {
                    int count = serialPort.read(buffer, 500);
                    if (count > 0) {
                        String text = new String(buffer, 0, count,
                                StandardCharsets.UTF_8);
                        runOnUiThread(() -> appendLog(text));
                    }
                } catch (IOException e) {
                    runOnUiThread(() -> {
                        appendLog("UART читання завершено: " + e.getMessage());
                        setStatus("З'єднання перервано");
                    });
                    break;
                }
            }
        });
    }

    private void disconnectUsb() {
        reading = false;
        if (reader != null) {
            reader.shutdownNow();
            reader = null;
        }
        if (serialPort != null) {
            try {
                serialPort.close();
            } catch (IOException ignored) { }
            serialPort = null;
        }
        setStatus("Від'єднано");
    }

    private void appendLog(String text) {
        output.append(text);
        if (!text.endsWith("\n")) output.append("\n");
        scroll.post(() -> scroll.fullScroll(View.FOCUS_DOWN));
    }

    private void setStatus(String text) {
        runOnUiThread(() -> status.setText("● " + text));
    }

    @Override
    protected void onDestroy() {
        disconnectUsb();
        try {
            unregisterReceiver(usbReceiver);
        } catch (Exception ignored) { }
        super.onDestroy();
    }
}
