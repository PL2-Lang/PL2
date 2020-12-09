PREFIX ?= /usr
CFLAGS := $(CFLAGS) -Wall -Wextra -Wc++-compat

LOG := sh -c 'printf "\\t$$0\\t$$1\\n"'

all: libpl2a.so libpl2ext.so pl2a

examples: libecho.so libpldbg.so libezecho.so

libezecho.so: ezecho.o
	@$(LOG) LINK libezecho.so
	@$(CC) ezecho.o -shared -o libezecho.so

ezecho.o: examples/ezecho.c
	@$(LOG) CC examples/ezecho.c
	@$(CC) $(CFLAGS) examples/ezecho.c -I. -c -fPIC -o ezecho.o

libecho.so: echo.o libpl2a.so
	@$(LOG) LINK libecho.so
	@$(CC) echo.o -L. -lpl2a -shared -o libecho.so

echo.o: examples/echo.c pl2a.h
	@$(LOG) CC examples/echo.c
	@$(CC) $(CFLAGS) examples/echo.c -I. -c -fPIC -o echo.o

libpldbg.so: pldbg.o libpl2a.so
	@$(LOG) LINK libpl2a.so
	@$(CC) pldbg.o -L. -lpl2a -shared -o libpldbg.so

pldbg.o: examples/pldbg.c pl2a.h
	@$(LOG) CC examples/pldbg.c
	@$(CC) $(CFLAGS) examples/pldbg.c -I. -c -fPIC -o pldbg.o

libpl2ext.so: pl2ext.o
	@$(LOG) LINK libpl2ext.so
	@$(CC) pl2ext.o -shared -o libpl2ext.so

pl2a: main.o libpl2a.so
	@$(LOG) LINK pl2a
	@$(CC) main.o -L. -lpl2a -ldl -o pl2a

main.o: pl2a.h main.c
	@$(LOG) CC pl2a.c
	@$(CC) $(CFLAGS) main.c -c -fPIC -o main.o

libpl2a.so: pl2a.o
	@$(LOG) LINK libpl2a.so
	@$(CC) pl2a.o -shared -o libpl2a.so

pl2ext.o: pl2ext.h pl2ext.c
	@$(LOG) CC pl2ext.c
	@$(CC) $(CFLAGS) pl2ext.c -c -fPIC -o pl2ext.o

pl2a.o: pl2a.c pl2a.h
	@$(LOG) CC pl2a.c
	@$(CC) $(CFLAGS) pl2a.c -c -fPIC -ldl -o pl2a.o

.PHONY: reinstall install uninstall clean

reinstall: uninstall install

install: pl2a.h pl2a libpl2a.so libpl2ext.so
	@$(LOG) MKDIR $(PREFIX)/bin
	@mkdir -p $(PREFIX)/bin
	@$(LOG) MKDIR $(PREFIX)/include
	@mkdir -p $(PREFIX)/include
	@$(LOG) MKDIR $(PREFIX)/lib
	@mkdir -p $(PREFIX)/lib
	@if [ -f $(PREFIX)/bin/pl2a ]; then \
		echo $(PREFIX)/bin/pl2a already exists, skipping; \
	else \
		$(LOG) CP $(PREFIX)/bin/pl2a; \
		cp pl2a $(PREFIX)/bin; \
	fi;
	@if [ -f $(PREFIX)/lib/libpl2a.so ]; then \
		echo $(PREFIX)/bin/libpl2a.so already exists, skipping; \
	else \
		$(LOG) CP $(PREFIX)/lib/libpl2a.so; \
		cp libpl2a.so $(PREFIX)/lib; \
	fi;
	@if [ -f $(PREFIX)/lib/libpl2ext.so ]; then \
		echo $(PREFIX)/bin/libpl2ext.so already exists, skipping; \
	else \
		$(LOG) CP $(PREFIX)/lib/libpl2ext.so; \
		cp libpl2ext.so $(PREFIX)/lib; \
	fi;
	@if [ -f $(PREFIX)/include/pl2a.h ]; then \
		echo $(PREFIX)/include/pl2a.h already exists, skipping; \
	else \@echo
		$(LOG) CP $(PREFIX)/include/pl2a.h; \
		cp pl2a.h $(PREFIX)/include; \
	fi;

uninstall:
	@$(LOG) RM $(PREFIX)/bin/pl2a
	@rm -f $(PREFIX)/bin/pl2a
	@$(LOG) RM $(PREFIX)/include/pl2a.h
	@rm -f $(PREFIX)/include/pl2a.h
	@$(LOG) RM $(PREFIX)/lib/libpl2a.so
	@rm -f $(PREFIX)/lib/libpl2a.so
	@$(LOG) RM $(PREFIX)/lib/libpl2ext.so
	@rm -f $(PREFIX)/lib/libpl2ext.so

clean:
	@$(LOG) RM *.o
	@rm -f *.o
	@$(LOG) RM *.a
	@rm -f *.a
	@$(LOG) RM *.so
	@rm -f *.so
	@$(LOG) RM *.lib
	@rm -f *.lib
	@$(LOG) RM *.dll
	@rm -f *.dll
	@$(LOG) RM pl2a
	@rm -f pl2a
