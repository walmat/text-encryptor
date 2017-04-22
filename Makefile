encrypt: encrypt.c
	gcc -pthread -o encrypt encrypt.c

clean:
	rm encrypt
