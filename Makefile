CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

all: app test

app: main.cpp feed_simulator.cpp order_book.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

test: tests/test_matching.cpp order_book.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -f app test

.PHONY: all clean