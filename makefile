ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
PROJECTS:=$(sort $(dir $(wildcard $(ROOT_DIR)/projects/*/*)))
BUILD_DIR=$(ROOT_DIR)/build
CC:=clang
CFLAGS:=-std=c11 -Wall -Wextra -pedantic-errors
CFLAGS_DEBUG:=$(CFLAGS) -O0 -ggdb -fsanitize=address
CFLAGS_RELEASE:=$(CFLAGS) -O3 -s 

$(info "ROOT DIR:" $(ROOT_DIR))
$(info "PROJECTS:" $(PROJECTS))

all:
	mkdir -p $(BUILD_DIR)
	$(foreach proj, $(PROJECTS), $(CC) $(CFLAGS_DEBUG) $(proj)/*.c -o $(BUILD_DIR)/$(shell basename $(proj));)

