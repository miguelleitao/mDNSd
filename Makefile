
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

install: ${TARGET}
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 ${TARGET} $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(PREFIX)/lib/systemd/system/
	install -m 755 ${SERVICE} $(DESTDIR)$(PREFIX)/lib/systemd/system/ 
	@echo "\nPara configurar arranque: systemctl enable mdnsd"
	@echo "Para controlar: systemctl start/stop mdnsd\n"

uninstall:
	rm -f (DESTDIR)$(PREFIX)/bin/${TARGET}
	rm -f $(DESTDIR)$(PREFIX)/lib/systemd/system/${SERVICE}
	

