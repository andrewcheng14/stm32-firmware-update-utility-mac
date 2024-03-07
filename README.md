### Compile
```
gcc ota_fw_update.c stm32_firmware_updater.c -lserialport -o build/stm32_firmware_updater
```

### Run
```
./build/stm32_firmware_updater /PATH/TO/FIRMWARE.bin /dev/tty.usbserial-XXXXX 115200 
```