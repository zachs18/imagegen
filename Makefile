default: dbg ;

CFLAGS := -lpthread -Wall -Wno-multichar -Wno-unused-function -Wno-unused-variable -DSDL_PROGRESS -lSDL2

%.o: %.c $(wildcard *.h)
	gcc -c $< -o $@ $(CFLAGS)

main.o: main.c $(wildcard *.h)
	gcc -c main.c -o main.o $(CFLAGS)

compile:     main.o setup.o input.o generate.o color.o seed.o progress.o output.o safelib.o pnmlib.o randint.o debug.o
	gcc main.o setup.o input.o generate.o color.o seed.o progress.o output.o safelib.o pnmlib.o randint.o debug.o -o imagegen $(CFLAGS)

main: compile ;

dbg: main ;

release: CFLAGS += -DNO_DEBUG
release: main ;

clean:
	rm -f *.o imagegen
