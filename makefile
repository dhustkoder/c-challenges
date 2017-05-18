ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
PROJECTS:=$(sort $(dir $(wildcard $(ROOT_DIR)/projects/*/*)))
BUILD_DIR=$(ROOT_DIR)/build
CC:=gcc
CFLAGS:=-std=c11 -Wall -Wextra -O0 -ggdb -fsanitize=address

$(info "ROOT DIR:" $(ROOT_DIR))
$(info "PROJECTS:" $(PROJECTS))

all:
	mkdir -p $(BUILD_DIR)
	$(foreach proj, $(PROJECTS), $(CC) $(CFLAGS) $(proj)/*.c -o $(BUILD_DIR)/$(shell basename $(proj));)

