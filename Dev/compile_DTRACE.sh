gcc -std=gnu99 -o server2.exe server2/*.c *.c -Iserver2 -lpthread -lm -Wall -DTRACE
gcc -std=gnu99 -o server.exe server1/*.c *.c -Iserver1 -lpthread -lm -Wall -DTRACE
gcc -std=gnu99 -o client.exe client1/*.c *.c -Iclient1 -lpthread -lm -Wall -DTRACE
