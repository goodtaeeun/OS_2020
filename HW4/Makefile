all:
	gcc -pthread -o abba abba.c
	gcc -pthread -o three three.c
	gcc -pthread -o ddchck ddchck.c
	gcc -shared -fPIC -o ddmon.so ddmon.c -ldl -pthread
