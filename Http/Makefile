CXXFLAGS = -std=c++17 -pthread -g

server: main.cc http.hpp ../source/Server.hpp
	g++ $(CXXFLAGS) main.cc -o server

.PHONY: clean
clean:
	rm -rf server
