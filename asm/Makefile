CC       = as
LD       = aarch64-linux-android-ld
AS_FLAGS = -g
SRC      = write.asm
OBJ      = bin/out.o
OUT      = bin/out

.PHONY: all build clean

all: build

build: $(OBJ) $(OUT)

$(OBJ): $(SRC)
	$(CC) $(SRC) $(AS_FLAGS) -o $(OBJ)

$(OUT): $(OBJ)
	$(LD) $(OBJ) -o $(OUT)

run: build
	@./$(OUT)

clean:
	@echo "Cleaning..."
	rm -f $(OBJ) $(OUT)

rebuild: clean build
