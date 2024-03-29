default: Debug ;

#CFLAGS := -lpthread -Wall -Wno-multichar -Wno-unused-function -Wno-unused-variable -DSDL_PROGRESS -lSDL2 -DFRAMEBUFFER_PROGRESS
CFLAGS := -lpthread -Wall -Wno-multichar -DSDL_PROGRESS -lSDL2 -DFRAMEBUFFER_PROGRESS

%.o: %.c $(wildcard *.h)
	gcc -c $< -o $@ $(CFLAGS)

main.o: main.c $(wildcard *.h)
	gcc -c main.c -o main.o $(CFLAGS)

compile:     main.o setup.o input.o generate_common.o generate_normal.o generate_symmetric.o color.o seed_common.o seed_normal.o seed_symmetric.o progress.o output.o safelib.o pnmlib.o randint.o debug.o
	gcc main.o setup.o input.o generate_common.o generate_normal.o generate_symmetric.o color.o seed_common.o seed_normal.o seed_symmetric.o progress.o output.o safelib.o pnmlib.o randint.o debug.o -o imagegen $(CFLAGS)

main: compile ;

Debug: CFLAGS += -g
Debug: main ;

DebugO2: CFLAGS += -g -O2
DebugO2: main ;

Release: CFLAGS += -DNO_DEBUG -O2
Release: main ;

clean:
	rm -f *.o imagegen
