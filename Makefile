CFLAGS = -Wall -Wextra -pthread

all: mps

mps: mps.c
	gcc $(CFLAGS) -o mps mps.c -lm -ggdb3
	gcc $(CFLAGS) -o mps_cv mps_cv.c -lm -ggdb3

clear:
	rm -f mps
	rm -f mps_cv
