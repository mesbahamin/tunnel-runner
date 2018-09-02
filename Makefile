build:
	clang -std=c99 -Wall -Wextra -DTR_LOGLEVEL_DEBUG -g tunnel_runner.c -o tunnel-runner -lm `sdl2-config --cflags --libs`

run: build
	./tunnel-runner
