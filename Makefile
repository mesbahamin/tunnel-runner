build:
	g++ -std=c++11 -Wall -Wextra -g sdl_tunnel_runner.cpp -o tunnel-runner `sdl2-config --cflags --libs`

run: build
	./tunnel-runner
