CXXFLAGS = -std=c++17 -pthread -g

.PHONY: all
all: Main Client

Main: Main.cc Server.hpp
	g++ $(CXXFLAGS) Main.cc -o Main

Client: Client.cc Server.hpp
	g++ $(CXXFLAGS) Client.cc -o Client

.PHONY: clean
clean:
	rm -rf Main Client
