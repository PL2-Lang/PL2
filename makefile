CFLAGS := $(CFLAGS) -Wall -Wextra -Wc++-compat

LOG := sh -c 'printf "\\t$$0\\t$$1\\n"'

all: libpl2a.so libpl2ext.so pl2a libecho.so libpldbg.so

libecho.so: echo.o libpl2a.so
	@$(LOG) LINK libecho.so
	@$(CC) echo.o -L. -lpl2a -shared -o libecho.so

echo.o: echo.c pl2a.h
	@$(LOG) CC echo.c
	@$(CC) $(CFLAGS) echo.c -c -fPIC -o echo.o

libpldbg.so: pldbg.o libpl2a.so
	@$(LOG) LINK libpl2a.so
	@$(CC) pldbg.o -L. -lpl2a -shared -o libpldbg.so

pldbg.o: pldbg.c pl2a.h
	@$(LOG) CC pldbg.c
	@$(CC) $(CFLAGS) pldbg.c -c -fPIC -o pldbg.o

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