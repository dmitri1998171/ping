BIN := ping
SRC := main.c
CC := gcc
CFLAGS :=
LDFLAGS :=

$(BIN):
	clear && $(CC) $(SRC) -o $(BIN)

clean:
	rm -rf $(BIN)