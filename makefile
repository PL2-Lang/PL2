PREFIX ?= /usr
CFLAGS := $(CFLAGS) -Wall -Wextra -Wc++-compat -g

LOG := sh -c 'printf "\\t$$0\\t$$1\\n"'

all: libpl2b.so libpl2ext.so pl2b

examples: libpldbg.so

libpldbg.so: pldbg.o libpl2b.so
	@$(LOG) LINK libpl2b.so
	@$(CC) pldbg.o -L. -lpl2b -shared -o libpldbg.so

pldbg.o: examples/pldbg.c pl2b.h
	@$(LOG) CC examples/pldbg.c
	@$(CC) $(CFLAGS) examples/pldbg.c -I. -c -fPIC -o pldbg.o

libpl2ext.so: pl2ext.o
	@$(LOG) LINK libpl2ext.so
	@$(CC) pl2ext.o -shared -o libpl2ext.so

pl2b: main.o libpl2b.so
	@$(LOG) LINK pl2b
	@$(CC) main.o -L. -lpl2b -ldl -o pl2b

main.o: pl2b.h main.c
	@$(LOG) CC pl2b.c
	@$(CC) $(CFLAGS) main.c -c -fPIC -o main.o

libpl2b.so: pl2b.o
	@$(LOG) LINK libpl2b.so
	@$(CC) pl2b.o -shared -o libpl2b.so

pl2ext.o: pl2ext.h pl2ext.c
	@$(LOG) CC pl2ext.c
	@$(CC) $(CFLAGS) pl2ext.c -c -fPIC -o pl2ext.o

pl2b.o: pl2b.c pl2b.h
	@$(LOG) CC pl2b.c
	@$(CC) $(CFLAGS) pl2b.c -c -fPIC -ldl -o pl2b.o

.PHONY: reinstall install uninstall clean

reinstall: uninstall install

install: pl2b.h pl2b libpl2b.so libpl2ext.so
	@$(LOG) MKDIR $(PREFIX)/bin
	@mkdir -p $(PREFIX)/bin
	@$(LOG) MKDIR $(PREFIX)/include
	@mkdir -p $(PREFIX)/include
	@$(LOG) MKDIR $(PREFIX)/lib
	@mkdir -p $(PREFIX)/lib
	@if [ -f $(PREFIX)/bin/pl2b ]; then \
		echo $(PREFIX)/bin/pl2b already exists, skipping; \
	else \
		$(LOG) CP $(PREFIX)/bin/pl2b; \
		cp pl2b $(PREFIX)/bin; \
	fi;
	@if [ -f $(PREFIX)/lib/libpl2b.so ]; then \
		echo $(PREFIX)/bin/libpl2b.so already exists, skipping; \
	else \
		$(LOG) CP $(PREFIX)/lib/libpl2b.so; \
		cp libpl2b.so $(PREFIX)/lib; \
	fi;
	@if [ -f $(PREFIX)/lib/libpl2ext.so ]; then \
		echo $(PREFIX)/bin/libpl2ext.so already exists, skipping; \
	else \
		$(LOG) CP $(PREFIX)/lib/libpl2ext.so; \
		cp libpl2ext.so $(PREFIX)/lib; \
	fi;
	@if [ -f $(PREFIX)/include/pl2b.h ]; then \
		echo $(PREFIX)/include/pl2b.h already exists, skipping; \
	else \@echo
		$(LOG) CP $(PREFIX)/include/pl2b.h; \
		cp pl2b.h $(PREFIX)/include; \
	fi;

uninstall:
	@$(LOG) RM $(PREFIX)/bin/pl2b
	@rm -f $(PREFIX)/bin/pl2b
	@$(LOG) RM $(PREFIX)/include/pl2b.h
	@rm -f $(PREFIX)/include/pl2b.h
	@$(LOG) RM $(PREFIX)/lib/libpl2b.so
	@rm -f $(PREFIX)/lib/libpl2b.so
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
	@$(LOG) RM pl2b
	@rm -f pl2b
