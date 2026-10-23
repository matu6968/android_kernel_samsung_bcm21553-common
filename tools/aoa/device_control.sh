#!/system/bin/sh
# Root-only test helper. Launch using busybox setsid so USB init may restart adbd.
apk=$(pm path org.galaxyy.aoa)
CLASSPATH=${apk#package:} app_process /system/bin org.galaxyy.aoa.Control "$@"
sleep 3
setprop service.adb.tcp.port ${AOA_TEST_ADB_PORT:-5555}
start adbd
