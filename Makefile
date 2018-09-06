history_influxdb: ./src/history_influxdb.c
	gcc -fPIC -shared -o dist/history_influxdb.so ./src/*.c -I../../../include
