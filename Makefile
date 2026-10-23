SHELL := /bin/bash
.DEFAULT_GOAL := run
.DELETE_ON_ERROR:

CXX ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -O2
PYTHON ?= python3
CONFIG ?= config.txt
ON_IP ?=
N ?=
ON_PORT ?= 5000
BASE_PORT ?= 6000
LOG_DIR ?= build/logs

.PHONY: all on run check help

all: build/hosts

build/hosts: hosts.cpp
	mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) "$<" $(LDFLAGS) $(LDLIBS) -o "$@"

on:
	$(PYTHON) server.py "$(CONFIG)"

check:
	@if [[ -z "$(ON_IP)" ]]; then \
		echo 'Usage: make ON_IP=192.168.1.10 N=5 [BASE_PORT=6000]' >&2; exit 1; \
	fi; \
	if ! [[ "$(N)" =~ ^[0-9]{1,2}$$ ]] || (( 10#$(N) < 1 || 10#$(N) > 26 )); then \
		echo 'N must be an integer from 1 to 26.' >&2; exit 1; \
	fi; \
	for port in "$(ON_PORT)" "$(BASE_PORT)"; do \
		if ! [[ "$$port" =~ ^[0-9]{1,5}$$ ]] || (( 10#$$port < 1 || 10#$$port > 65535 )); then \
			echo 'ON_PORT and BASE_PORT must be integers from 1 to 65535.' >&2; exit 1; \
		fi; \
	done; \
	if (( 10#$(BASE_PORT) + 10#$(N) - 1 > 65535 )); then \
		echo 'The requested UDP port range exceeds 65535.' >&2; exit 1; \
	fi

run: check build/hosts
	@set -e; \
	mkdir -p "$(LOG_DIR)"; \
	pids=(); \
	cleanup() { \
		if (( $${#pids[@]} )); then \
			kill "$${pids[@]}" 2>/dev/null || true; \
			wait "$${pids[@]}" 2>/dev/null || true; \
		fi; \
	}; \
	trap cleanup EXIT; \
	trap 'exit 130' INT; \
	trap 'exit 143' TERM; \
	for (( i=0; i < 10#$(N); i++ )); do \
		port=$$((10#$(BASE_PORT) + i)); \
		log="$(LOG_DIR)/vn-$$port.log"; \
		./build/hosts "$(ON_IP)" "$$((10#$(ON_PORT)))" "$$port" > "$$log" 2>&1 & \
		pids+=($$!); \
		echo "VN $$((i + 1)): ON=$(ON_IP):$$((10#$(ON_PORT))), UDP=$$port, log=$$log"; \
	done; \
	echo 'VNs running. Press Ctrl+C to stop all VNs started by this command.'; \
	status=0; wait -n || status=$$?; \
	echo "A VN exited (status $$status). Stopping this group; check $(LOG_DIR)/vn-*.log."; \
	exit "$$status"

help:
	@echo 'Start ON: make on [CONFIG=config.txt] [PYTHON=python3]'
	@echo 'Launch VNs: make ON_IP=192.168.1.10 N=5'
	@echo 'Options: ON_PORT=5000 BASE_PORT=6000 LOG_DIR=build/logs'
	@echo 'Build only: make all'
	@echo 'Requires Linux/WSL, GNU Make, Bash 4.3+, and a C++ compiler.'
