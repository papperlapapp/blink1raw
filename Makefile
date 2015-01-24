

CC=gcc

blink1raw: blink1raw.c
	$(CC) -g -W -Wall -o $@ $<

all: blink1raw
