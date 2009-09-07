SRC = apt-queue.c
OBJ = $(SRC:.c=)

DESTDIR=
PREFIX=$(DESTDIR)/usr

all: $(OBJ)

clean:
	rm -f $(OBJ)

%: %.c
	gcc $*.c -o $@

install: $(OBJ)
	install -m 0755 -d $(PREFIX)/bin
	install -m 0755 $^ $(PREFIX)/bin
