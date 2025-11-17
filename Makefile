CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -O2
CPPFLAGS = -Isrc
LDFLAGS = 

# Targets
LIBRARY = libspi.a
EXAMPLE_BIN = spi_example

SRC_DIR = src
EXAMPLE_DIR = example

# Source files
LIB_SOURCES = $(SRC_DIR)/spi.cpp $(SRC_DIR)/link_layer.cpp
LIB_OBJECTS = $(LIB_SOURCES:.cpp=.o)

EXAMPLE_SOURCES = $(EXAMPLE_DIR)/example.cpp
EXAMPLE_OBJECTS = $(EXAMPLE_SOURCES:.cpp=.o)

# Default target
all: $(LIBRARY) $(EXAMPLE_BIN)

# Build static library
$(LIBRARY): $(LIB_OBJECTS)
	ar rcs $@ $^
	@echo "Built library: $(LIBRARY)"

# Build example
$(EXAMPLE_BIN): $(EXAMPLE_OBJECTS) $(LIBRARY)
	$(CXX) $(CXXFLAGS) -o $@ $(EXAMPLE_OBJECTS) $(LIBRARY) $(LDFLAGS)
	@echo "Built example: $(EXAMPLE_BIN)"

# Compile object files
$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(EXAMPLE_DIR)/%.o: $(EXAMPLE_DIR)/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(LIB_OBJECTS) $(EXAMPLE_OBJECTS) $(LIBRARY) $(EXAMPLE_BIN)
	@echo "Cleaned build artifacts"

.PHONY: all clean
