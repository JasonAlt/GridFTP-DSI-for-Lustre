all: libglobus_gridftp_server_lustre.so libglobus_gridftp_server_lustre_preload.so

globus_gridftp_server_file.o: globus_gridftp_server_file.c
	$(CC) -c -fPIC -D_GNU_SOURCE -I/usr/include/globus/ globus_gridftp_server_file.c

lustre.o: lustre.c
	$(CC) -c -fPIC -D_GNU_SOURCE -I/usr/include/globus lustre.c

libglobus_gridftp_server_lustre.so: globus_gridftp_server_file.o lustre.o
	$(CC) -shared -o libglobus_gridftp_server_lustre.so globus_gridftp_server_file.o lustre.o /usr/lib64/liblustreapi.a -lglobus_gridftp_server

malloc.o: malloc.c
	$(CC) -c -fPIC -D_GNU_SOURCE malloc.c

libglobus_gridftp_server_lustre_preload.so : malloc.o
	$(CC) -shared -o libglobus_gridftp_server_lustre_preload.so malloc.o

clean:
	rm -f globus_gridftp_server_file.o libglobus_gridftp_server_lustre.so lustre.o
	rm -f malloc.o libglobus_gridftp_server_lustre_preload.so
