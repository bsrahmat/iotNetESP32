.PHONY: all build upload monitor clean erase test-native

build:
	cd /home/farismnrr/Documents/Programs/IoTNet/plugins/iotNetESP32 && pio run -e esp32doit-devkit-v1

upload:
	cd /home/farismnrr/Documents/Programs/IoTNet/plugins/iotNetESP32 && pio run -e esp32doit-devkit-v1 -t upload

erase:
	cd /home/farismnrr/Documents/Programs/IoTNet/plugins/iotNetESP32 && pio run -e esp32doit-devkit-v1 -t erase

monitor:
	@chmod +x ./monitor.sh
	@./monitor.sh

all: build upload monitor

clean:
	cd /home/farismnrr/Documents/Programs/IoTNet/plugins/iotNetESP32 && pio run -e esp32doit-devkit-v1 -t clean

test-native:
	cd /home/farismnrr/Documents/Programs/IoTNet/plugins/iotNetESP32 && pio test -e native
