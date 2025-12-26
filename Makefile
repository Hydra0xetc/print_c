CC = aarch64-linux-android-gcc
SRC = printf.c
FLAGS = -Wall -Wextra -ggdb -nostdlib -ffreestanding
OUT = bin/out

.PHONY = all build clean

all: build

build: $(OUT)

$(OUT): $(SRC)
	$(CC) $(SRC) $(FLAGS) -o $(OUT)

run: build
	@./$(OUT)

clean:
	@echo "Cleaning..."
	rm -f $(OUT)

rebuild: clean build
