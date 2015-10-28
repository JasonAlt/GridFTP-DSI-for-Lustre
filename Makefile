all: libglobus_gridftp_server_lustre.so libglobus_gridftp_server_lustre_preload.so

globus_gridftp_server_file.o: globus_gridftp_server_file.c
	$(CC) -c -D_GNU_SOURCE -I/usr/include/globus/ -fPIC -DPIC globus_gridftp_server_file.c

libglobus_gridftp_server_lustre.so: globus_gridftp_server_file.o
	$(LD) -shared -o libglobus_gridftp_server_lustre.so globus_gridftp_server_file.o /usr/lib64/liblustreapi.a

malloc.o: malloc.c
	$(CC) -c -D_GNU_SOURCE -I/usr/include/globus/ -fPIC -DPIC malloc.c

libglobus_gridftp_server_lustre_preload.so : malloc.o
	$(LD) -shared -o libglobus_gridftp_server_lustre_preload.so malloc.o

clean:
	rm -f globus_gridftp_server_file.o libglobus_gridftp_server_lustre.so
	rm -f malloc.o libglobus_gridftp_server_lustre_preload.so
