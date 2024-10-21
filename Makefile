TARGET = buddhabrot
CXX = g++
CXXFLAGS = -O3 -std=c++20
LDFLAGS = -lm
BIN = bin
SRC = src
SRCS = $(wildcard $(SRC)/*.cpp)
OBJS = $(SRCS:$(SRC)%.cpp=$(BIN)/%.o)
HDRS = $(wildcard $(SRC)/*.h)


.PHONY: default all clean debug

default: $(TARGET)
all: default

$(BIN)/%.o: $(SRC)/%.cpp $(HDRS) | $(BIN)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN):
	mkdir -p $@

$(TARGET): $(OBJS)
	$(CXX) $^ $(CXXFLAGS) $(LDFLAGS) -o $@

clean:
	-rm -f $(BIN)/*.o
	-rm -f $(TARGET)
