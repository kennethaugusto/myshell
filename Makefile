CC = gcc

TARGET = mysh

SRC = mysh2.c

all:
	$(CC) -o $(TARGET) $(SRC)