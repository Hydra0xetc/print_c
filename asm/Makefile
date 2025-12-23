CC = as
LD = ld
SRC = write.asm
OBJ = bin/out.o
OUT = bin/out

.PHONY = all build clean

all: build

build: $(OBJ) $(OUT)

$(OBJ): $(SRC)
	$(CC) $(SRC) -o $(OBJ)

$(OUT): $(OBJ)
	$(LD) $(OBJ) -o $(OUT)

run: build
	@./bin/out

clean:
	@echo "Cleaning..."
	rm -f $(OBJ) $(OUT)

rebuild: clean build
