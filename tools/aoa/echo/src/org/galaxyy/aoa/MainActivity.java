package org.galaxyy.aoa;

import android.app.Activity;
import android.app.PendingIntent;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbAccessory;
import android.hardware.usb.UsbManager;
import android.util.Log;
import android.view.WindowManager;
import android.widget.TextView;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

public final class MainActivity extends Activity {
    private static final String TAG = "AOAEcho";
    private static final String PERMISSION = "org.galaxyy.aoa.USB_PERMISSION";
    private UsbManager manager;
    private TextView status;
    private volatile ParcelFileDescriptor connection;
    private final BroadcastReceiver receiver = new BroadcastReceiver() {
        public void onReceive(Context context, Intent intent) {
            if (PERMISSION.equals(intent.getAction())) {
                if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false))
                    connect((UsbAccessory) intent.getParcelableExtra(UsbManager.EXTRA_ACCESSORY));
                else report("USB permission denied");
            } else if (UsbManager.ACTION_USB_ACCESSORY_DETACHED.equals(intent.getAction())) {
                closeConnection();
                report("Accessory detached");
            }
        }
    };
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        status = new TextView(this);
        setContentView(status);
        manager = (UsbManager) getSystemService(USB_SERVICE);
        IntentFilter filter = new IntentFilter(PERMISSION);
        filter.addAction(UsbManager.ACTION_USB_ACCESSORY_DETACHED);
        registerReceiver(receiver, filter);
        findAccessory();
    }
    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        findAccessory();
    }
    private void findAccessory() {
        UsbAccessory[] accessories = manager.getAccessoryList();
        if (accessories == null || accessories.length == 0) {
            report("Waiting for GalaxyY / AOA Echo Test accessory");
            return;
        }
        UsbAccessory accessory = accessories[0];
        if (manager.hasPermission(accessory)) connect(accessory);
        else manager.requestPermission(accessory,
                PendingIntent.getBroadcast(this, 0, new Intent(PERMISSION), 0));
    }
    private synchronized void connect(UsbAccessory accessory) {
        if (connection != null) return;
        connection = manager.openAccessory(accessory);
        if (connection == null) { report("openAccessory returned null"); return; }
        final ParcelFileDescriptor fd = connection;
        report("Connected: " + accessory);
        new Thread(new Runnable() {
            public void run() {
                long total = 0;
                try {
                    FileInputStream input = new FileInputStream(fd.getFileDescriptor());
                    FileOutputStream output = new FileOutputStream(fd.getFileDescriptor());
                    byte[] buffer = new byte[16384];
                    int count;
                    while ((count = input.read(buffer)) >= 0) {
                        if (count == 0) continue;
                        output.write(buffer, 0, count);
                        total += count;
                        report("Echoed " + total + " bytes");
                    }
                } catch (IOException error) {
                    report("Disconnected after " + total + " bytes: " + error);
                } finally {
                    synchronized (MainActivity.this) {
                        if (connection == fd) closeConnection();
                    }
                }
            }
        }, "AOA echo").start();
    }
    private void report(final String text) {
        Log.i(TAG, text);
        runOnUiThread(new Runnable() { public void run() { status.setText(text); } });
    }
    private synchronized void closeConnection() {
        ParcelFileDescriptor fd = connection;
        connection = null;
        if (fd != null) try { fd.close(); } catch (IOException ignored) { }
    }
    @Override protected void onDestroy() {
        unregisterReceiver(receiver);
        closeConnection();
        super.onDestroy();
    }
}
