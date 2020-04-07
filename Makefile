CC = gcc
CFLAGS = -Wall -pedantic -std=c99 -g -lrt -pthread
ALL_OBJ = *.o
MASTER_OBJ = master.o
SLAVE_OBJ = slave.o
VIEW_OBJ = view.o
MASTER_OUT = master
SLAVE_OUT = slave
VIEW_OUT = view

all: $(MASTER_OUT) $(SLAVE_OUT) $(VIEW_OUT)

$(MASTER_OUT): $(MASTER_OBJ)
	$(CC) -o $(MASTER_OUT) $(MASTER_OBJ) $(CFLAGS)

$(SLAVE_OUT): $(SLAVE_OBJ)
	$(CC) -o $(SLAVE_OUT) $(SLAVE_OBJ) $(CFLAGS)

$(VIEW_OUT): $(VIEW_OBJ)
	$(CC) -o $(VIEW_OUT) $(VIEW_OBJ) $(CFLAGS)

clean:
	rm -rf $(MASTER_OUT) $(MASTER_OBJ) $(SLAVE_OBJ) $(SLAVE_OUT) $(VIEW_OUT) $(VIEW_OBJ)

.PHONY: all clean
