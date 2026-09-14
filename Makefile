CXX = c++
CXXFLAGS = -std=c++23 -Wall -Wextra -I./include
DEBUGFLAGS = -g -O0 -DDEBUG
RELEASEFLAGS = -O3

SRCDIR = src
OBJDIR = build
BINDIR = build

SRCS = $(wildcard $(SRCDIR)/*.cpp)
OBJS = $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/app

TEST_FRAME_SRCS = $(SRCDIR)/error.cpp $(SRCDIR)/frame.cpp tests/test_frame.cpp
TEST_FRAME_TARGET = $(BINDIR)/test_frame

TEST_ARQ_SRCS = $(SRCDIR)/error.cpp $(SRCDIR)/frame.cpp $(SRCDIR)/arq.cpp tests/test_arq.cpp
TEST_ARQ_TARGET = $(BINDIR)/test_arq

all: test

test: $(TEST_FRAME_TARGET) $(TEST_ARQ_TARGET)
	./$(TEST_FRAME_TARGET)
	./$(TEST_ARQ_TARGET)

$(TEST_FRAME_TARGET): $(TEST_FRAME_SRCS)
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) $(RELEASEFLAGS) $(TEST_FRAME_SRCS) -o $@

$(TEST_ARQ_TARGET): $(TEST_ARQ_SRCS)
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) $(RELEASEFLAGS) $(TEST_ARQ_SRCS) -o $@

$(TARGET): $(OBJS)
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) $(RELEASEFLAGS) $(OBJS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(RELEASEFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

debug: RELEASEFLAGS = $(DEBUGFLAGS)
debug: clean $(TEST_ARQ_TARGET)
	lldb ./$(TEST_ARQ_TARGET)

clean:
	@rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all test run debug clean help

