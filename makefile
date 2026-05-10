CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
TARGET   = shell
SRCS     = main.cpp globals.cpp utils.cpp builtins.cpp pinfo_search.cpp \
           history.cpp signals.cpp execute.cpp input.cpp
OBJS     = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp shell.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
