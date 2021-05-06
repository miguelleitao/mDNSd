
-include .config

TARGET=mdnsd
TABLE=${TARGET}.tab
SERVICE=${TARGET}.service
DEV_TARGETS=mdnsc mcast_rec

CFLAGS=-Wall -Wno-unused-local-typedefs -fPIC

default: ${TARGET}

all: default ${TABLE} ${DEV_TARGETS}

${TARGET}: ${TARGET}.o
	g++ -o $@ $^ 

${TARGET}.o: ${TARGET}.cpp tcpudp.h config.h
	g++ -c ${CFLAGS} -g $<

${TABLE}: 
	echo `hostname -s`.local >$@

clean:
	rm -f ${TARGET} ${TARGET}.o ${TABLE}

push:
	git add .
	-git commit -m "auto update"
	git push

install: ${TARGET} ${TABLE}
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 ${TARGET} $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(PREFIX)/etc/
	install -m 644 ${TABLE} $(DESTDIR)$(PREFIX)/etc/
	install -d $(DESTDIR)$(PREFIX)/lib/systemd/system/
	install -m 644 ${SERVICE} $(DESTDIR)$(PREFIX)/lib/systemd/system/ 
	@echo "\nPara configurar arranque: systemctl enable mdnsd"
	@echo "Para controlar: systemctl start/stop mdnsd\n"

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/${TARGET}
	rm -f $(DESTDIR)$(PREFIX)/etc/${TABLE}
	rm -f $(DESTDIR)$(PREFIX)/lib/systemd/system/${SERVICE}
	

