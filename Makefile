TARGET = license
CFLAGS = -Wall -Wextra -Icwalk
SRC_FILES = license.c cwalk/cwalk.c

$(TARGET): $(SRC_FILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC_FILES)
