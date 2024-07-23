CC = gcc

TARGET = mysh

SRC = mysh.c

all:
	$(CC) -o $(TARGET) $(SRC)