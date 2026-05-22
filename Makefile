# COMP2521 26T0 - Assignment

# List all your supporting .c files here. Do NOT include .h files in this list.
# Example: SUPPORTING_FILES = hello.c world.c

SUPPORTING_FILES =

########################################################################
# !!! DO NOT MODIFY ANYTHING BELOW THIS LINE !!!

CC = clang
CFLAGS = -Wall -Wvla -Werror -gdwarf-4

########################################################################

.PHONY: asan msan nosan

asan: CFLAGS += -fsanitize=address,leak,undefined
asan: all

msan: CFLAGS += -fsanitize=memory,undefined -fsanitize-memory-track-origins
msan: all

nosan: all

########################################################################

.PHONY: all
all: testFs

testFs: testFs.c Path.c Fs.c $(SUPPORTING_FILES)
	$(CC) $(CFLAGS) -o testFs testFs.c Path.c Fs.c $(SUPPORTING_FILES)

########################################################################

.PHONY: clean
clean:
	rm -f testFs

