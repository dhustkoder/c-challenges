ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
PROJECTS:=$(sort $(dir $(wildcard $(ROOT_DIR)/*/*)))
BUILD_DIR=$(ROOT_DIR)/build
CC:=gcc
CFLAGS:=-std=c11 -Wall -Wextra -O0 -ggdb

$(info "ROOT DIR:" $(ROOT_DIR))
$(info "PROJECTS:" $(PROJECTS))

all:
	$(shell mkdir $(BUILD_DIR))
	@$(foreach proj, $(PROJECTS), $(CC) $(CFLAGS) $(proj)/*.c -o $(BUILD_DIR)/$(shell basename $(proj));)
