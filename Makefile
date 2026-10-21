# ==========================================
# LAMBDA COMPILER TEST MAKEFILE
# ==========================================

# 1. Compiler Settings
CXX = g++
CXXFLAGS = -std=c++11 -Wall -I./include

# 2. Dynamic File Discovery
# Grab all .cpp files in src/ EXCEPT the main driver (to avoid multiple main() errors)
SRC_FILES = $(filter-out src/lambda_compiler.cpp, $(wildcard src/*.cpp))

# Grab every single .cpp file inside the tests/ directory
TEST_FILES = $(wildcard tests/*.cpp)

# 3. Output Executable Name
TARGET = run_all_tests

.PHONY: all test test-v test-q clean

# ==========================================
# COMMAND RULES
# ==========================================

# Default command ('make') builds the executable
all: $(TARGET)

$(TARGET): $(SRC_FILES) $(TEST_FILES)
	$(CXX) $(CXXFLAGS) $^ -o $@
	@echo "\nBuild complete! Run with ./$(TARGET)"

# 'make test' builds and runs normally
test: $(TARGET)
	./$(TARGET)

# 'make test-v' builds and runs in verbose mode
test-v: $(TARGET)
	./$(TARGET) -v

# 'make test-q' builds and runs in quiet mode
test-q: $(TARGET)
	./$(TARGET) -q

# 'make clean' removes the compiled executable
clean:
	rm -f $(TARGET)