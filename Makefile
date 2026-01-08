
CXX = g++
CXXFLAGS = -Iinclude -Wall -g


SRC_DIR = src
TEST_DIR = tests

COMMON_SRCS = $(SRC_DIR)/allocator/physical_memory.cpp \
              $(SRC_DIR)/cache/cache.cpp \
              $(SRC_DIR)/mmu/mmu.cpp

MAIN_TARGET = memsim
TEST_TARGET = run_tests

all: $(MAIN_TARGET) $(TEST_TARGET)

$(MAIN_TARGET): main.cpp $(COMMON_SRCS)
	@echo "Building Main Simulation..."
	$(CXX) $(CXXFLAGS) main.cpp $(COMMON_SRCS) -o $(MAIN_TARGET)
	@echo "Build Complete: ./$(MAIN_TARGET)"

$(TEST_TARGET): $(TEST_DIR)/test_suite.cpp $(COMMON_SRCS)
	@echo "Building Test Suite..."
	$(CXX) $(CXXFLAGS) $(TEST_DIR)/test_suite.cpp $(COMMON_SRCS) -o $(TEST_TARGET)
	@echo "Build Complete: ./$(TEST_TARGET)"

test: $(TEST_TARGET)
	@echo "Running Tests..."
	./$(TEST_TARGET) > test_results.txt
	@echo "Done! Results saved to 'test_results.txt'"
	@echo "Displaying summary:"
	@tail -n 5 test_results.txt

run: $(MAIN_TARGET)
	./$(MAIN_TARGET)


clean:
	rm -f $(MAIN_TARGET) $(TEST_TARGET) test_results.txt *.o

.PHONY: all test run clean