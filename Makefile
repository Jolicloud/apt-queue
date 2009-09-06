SRC = apt-queue.c
OBJ = $(SRC:.c=)

all: $(OBJ)

clean:
	rm -f $(OBJ)

%: %.c
	gcc -march=i386 -mtune=i386 $*.c -o $@
