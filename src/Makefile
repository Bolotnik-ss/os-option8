CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -ljpeg
SRC = option8.c
OBJ = $(SRC:.c=.o)
TARGET = option8

INSTALL_DIR = /usr/local/bin
SYSTEMD_DIR = /etc/systemd/system
CONFIG_DIR = /etc

install: $(TARGET)
	install -m 755 $(TARGET) $(INSTALL_DIR)
	install -m 644 option8.service $(SYSTEMD_DIR)
	install -m 644 option8.conf $(CONFIG_DIR)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

uninstall:
	rm -f $(INSTALL_DIR)/$(TARGET)
	rm -f $(SYSTEMD_DIR)/option8.service
	rm -f $(CONFIG_DIR)/option8.conf
