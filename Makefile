CC = gcc
SRC = print.c
FLAGS = -Wall -Wextra -ggdb
OUT = bin/out

.PHONY = all build clean

all: build

build: $(OUT)

$(OUT): $(SRC)
	$(CC) $(SRC) $(FLAGS) -o $(OUT)

run: build
	@echo "Running the prrogram"
	@./bin/out

clean:
	@echo "Cleaning..."
	rm -f $(OUT)

rebuild: clean build
