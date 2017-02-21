CFLAGS=-Wall -g

SOURCE_MAIN=main.c
SOURCE_SLAVE=slave.c
OBJECT_MAIN=main.o
OBJECT_SLAVE=slave.o
EXE_MAIN=master
EXE_SLAVE=slave

all: $(EXE_MAIN) $(EXE_SLAVE)

$(EXE_MAIN): $(OBJECT_MAIN)
        gcc $(OBJECT_MAIN) -o $(EXE_MAIN)

$(EXE_SLAVE): $(OBJECT_SLAVE)
        gcc $(OBJECT_SLAVE) -o $(EXE_SLAVE)

%.o: %.c
        gcc -c $(CFLAGS) $*.c -o $*.o

.PHONY: clean

clean:
        rm *.o $(EXE_MAIN) $(EXE_SLAVE)