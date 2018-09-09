# This makefile provides some convenient shortcuts for common operations. The
# actual build logic happens in build.sh.

BUILD_SCRIPT=./build.sh

.PHONY: all clean debug release run

all:
	$(BUILD_SCRIPT)

clean:
	rm build/ -r

debug:
	$(BUILD_SCRIPT) --debug-only

release:
	$(BUILD_SCRIPT) --release-only

run: release
	./build/release/tunnel-runner
