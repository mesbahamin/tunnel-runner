build:
	clang -std=c99 -Wall -Wextra -g tunnel_runner.c -o tunnel-runner -lm `sdl2-config --cflags --libs`

run: build
	./tunnel-runner
