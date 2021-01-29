
-include .config

TARGET=mdnsd
CFLAGS=-Wall -Wno-unused-local-typedefs -fPIC

default: ${TARGET}

all: default

${TARGET}: ${TARGET}.o
	g++ -o $@ $^ 

${TARGET}.o: ${TARGET}.cpp tcpudp.h config.h
	g++ -c ${CFLAGS} -g $<

clean:
	rm -f ${TARGET} ${TARGET}.o

push:
	git add .
	git commit -m "auto update"
	git push
