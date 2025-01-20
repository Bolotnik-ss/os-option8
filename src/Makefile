CC=gcc
CFLAGS=-Wall -g
LDFLAGS=-ljpeg
SRC=option8.c
BINARY=option8
SERVICE=option8.service
CONFIG=option8.conf
BINARY_DIR=/usr/local/bin
SERVICE_DIR=/etc/systemd/system
CONFIG_DIR=/etc

.PHONY: all clean install uninstall

all: $(BINARY)

$(BINARY): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BINARY) $(LDFLAGS)

install: all
	install -m 0755 $(BINARY) $(BINARY_DIR)
	install -m 0644 $(SERVICE) $(SERVICE_DIR)
	install -m 0644 $(CONFIG) $(CONFIG_DIR)

	systemctl daemon-reload
	systemctl enable option8.service
	systemctl start option8.service

uninstall:
	systemctl stop option8.service
	systemctl disable option8.service

	rm -f $(CONFIG_DIR)/$(CONFIG)
	rm -f $(SERVICE_DIR)/$(SERVICE)
	rm -f $(BINARY_DIR)/$(BINARY)

	systemctl daemon-reload

clean:
	rm -f $(BINARY)
