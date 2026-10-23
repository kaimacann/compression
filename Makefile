all:
	gcc -ansi ./src/c/main/main.c ./src/c/login/login.c ./src/c/fileIO/fileInput/fileInput.c ./src/c/fileIO/fileOutput/fileOutput.c ./src/c/encryption/encryption.c ./src/c/compression/compression.c

haadi:
	gcc -ansi ./src/c/main/main.c ./src/c/login/login.c ./src/c/fileIO/fileInput/fileInput.c ./src/c/fileIO/fileOutput/fileOutput.c -o test

run:
	./test

mr:
	gcc -ansi ./src/c/main/main.c ./src/c/login/login.c ./src/c/fileIO/fileInput/fileInput.c ./src/c/fileIO/fileOutput/fileOutput.c -o test
	./test
	
