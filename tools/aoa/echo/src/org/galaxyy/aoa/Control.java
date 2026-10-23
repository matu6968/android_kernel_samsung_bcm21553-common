package org.galaxyy.aoa;

import android.content.Context;
import java.lang.reflect.Method;

/** Optional root-only app_process helper for testing both USB-debugging states. */
public final class Control {
    public static void main(String[] args) throws Exception {
        if (android.os.Process.myUid() != 0)
            throw new SecurityException("Run this test helper from a root adb shell");
        android.os.Looper.prepareMainLooper();
        Class<?> cls = Class.forName("android.app.ActivityThread");
        Object thread = cls.getMethod("systemMain").invoke(null);
        Context context = (Context) cls.getMethod("getSystemContext").invoke(thread);
        if ("bluetooth".equals(args[0])) {
            android.bluetooth.BluetoothAdapter adapter = android.bluetooth.BluetoothAdapter.getDefaultAdapter();
            if (adapter == null) throw new IllegalStateException("Bluetooth adapter missing");
            boolean original = adapter.isEnabled();
            boolean passed = original;
            try {
                if (!original) {
                    adapter.enable();
                    for (int i=0; i<20 && !adapter.isEnabled(); i++) Thread.sleep(500);
                    passed = adapter.isEnabled();
                }
                System.out.println("Bluetooth enable test=" + passed);
            } finally {
                if (!original) {
                    adapter.disable();
                    for (int i=0; i<20 && adapter.isEnabled(); i++) Thread.sleep(500);
                }
            }
            System.out.println("Bluetooth original state restored=" + (adapter.isEnabled() == original));
            System.exit(passed && adapter.isEnabled() == original ? 0 : 1);
        }
        if ("grant".equals(args[0])) {
            Object usb = context.getSystemService(Context.USB_SERVICE);
            android.hardware.usb.UsbAccessory[] accessories =
                ((android.hardware.usb.UsbManager) usb).getAccessoryList();
            if (accessories == null || accessories.length == 0)
                throw new IllegalStateException("No accessory attached");
            java.lang.reflect.Field field = usb.getClass().getDeclaredField("mService");
            field.setAccessible(true);
            Object service = field.get(usb);
            int uid = context.getPackageManager().getApplicationInfo("org.galaxyy.aoa", 0).uid;
            service.getClass().getMethod("grantAccessoryPermission",
                android.hardware.usb.UsbAccessory.class, int.class).invoke(service, accessories[0], uid);
            System.out.println("Granted test app accessory access, uid=" + uid);
            System.exit(0);
        }
        if ("function".equals(args[0])) {
            Object usb = context.getSystemService(Context.USB_SERVICE);
            Method method = usb.getClass().getMethod("setCurrentFunction", String.class, boolean.class);
            method.invoke(usb, "default".equals(args[1]) ? null : args[1], false);
            System.out.println("Requested USB function " + args[1]);
            System.exit(0);
        }
        throw new IllegalArgumentException("Use grant, bluetooth, or function NAME");
    }
}
