.PHONY: all host-test test smoke repro
all:
	python3 tools/build.py --arch all
host-test:
	python3 tools/test.py --host
smoke:
	python3 tools/test.py --qemu
repro:
	python3 tools/test.py --repro
test:
	python3 tools/test.py --all
