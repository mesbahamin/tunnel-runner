build:
	g++ -std=c++11 -Wall -Wextra -g sdl_tunnel_flyer.cpp -o tunnel-flyer `sdl2-config --cflags --libs`

run:
	./tunnel-flyer

test: build run
