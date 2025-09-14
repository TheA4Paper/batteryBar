CFLAG = `pkg-config --cflags --libs gtk4` -I/usr/include/gtk4-layer-shell -lgtk4-layer-shell -O3 -Os -s -DNDEBUG -flto -ffunction-sections -fdata-sections -Wl,--gc-sections

all: batteryBar

batteryBar: batteryBar.c
	gcc $(CFLAG) batteryBar.c -o batteryBar
	chmod +x batteryBar

clean:
	rm batteryBar

