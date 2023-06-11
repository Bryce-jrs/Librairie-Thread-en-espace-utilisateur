#compiler flag
CC = gcc -I$(SRC_DIR) -g -O0 $(LDFIFOFLAG) $(LDPREEMPTIONFLAG)
CCFLAGS = -Wall -Wextra	-fPIC 
VALFLAGS = valgrind --leak-check=full --show-reachable=yes --track-origins=yes
LDFLAGS = -shared
LDPREEMPTIONFLAG = -DPREEMPTION
LDFIFOFLAG = -DFIFO
LDPRIORITYFLAG = -DPRIORITY
#directories
SRC_DIR = src
TEST_DIR = test
INSTALL_DIR = install
BUILD_DIR = build
BIN_DIR = $(INSTALL_DIR)/bin
LIB_DIR = $(INSTALL_DIR)/lib
PTHREAD_DIR = $(BIN_DIR)/pthreads
STACK_DIR = $(BIN_DIR)/stack

#source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
SRC_OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SOURCES))

#test files 
ALL_TESTS = $(wildcard $(TEST_DIR)/*.c)
EXCLUDED_TESTS = 41-signal.c 63-semaphore.c 65-barrier.c 64-cond.c
TESTS = $(filter-out $(addprefix $(TEST_DIR)/, $(EXCLUDED_TESTS)), $(ALL_TESTS))
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.c, $(TEST_DIR)/%.o, $(TESTS))

#install targets
LIBRARY = $(LIB_DIR)/libthread.so
BINARIES = $(patsubst $(TEST_DIR)/%.c, $(BIN_DIR)/%, $(TESTS)) 

# Default target
.DEFAULT_GOAL = all
all: $(LIBRARY) $(BINARIES)

#compile object files 
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CCFLAGS) -o $@ -c $<

$(LIBRARY): $(SRC_OBJECTS)
	mkdir -p $(LIB_DIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(BIN_DIR)/%: $(TEST_DIR)/%.o $(LIBRARY)
	mkdir -p $(BIN_DIR)
	$(CC) $(CCFLAGS) -o $@ $< -L$(LIB_DIR) -lthread -Wl,-rpath=$(LIB_DIR)

check:
	@for e in $(BIN_DIR)/*; do \
		echo $$e, ./$$e; \
	done

valgrind:
	@for e in $(BIN_DIR)/*; do \
		echo $$e, valgrind $(VALFLAGS) ./$$e; \
	done

pthreads: CCFLAGS+= -DUSE_PTHREAD
pthreads:
	mkdir -p $(PTHREAD_DIR); \
	for file in $(TEST_DIR)/*.c; do \
		bin_file=p$$(basename $$file .c); \
		$(CC) $(CCFLAGS) -o $(PTHREAD_DIR)/$$bin_file $$file -pthread; \
	done

stack: CCFLAGS+= -DSTACKOVERFLOW
stack: all
	mkdir -p $(STACK_DIR); \
	for file in $(TEST_DIR)/*.c; do \
		bin_file=s$$(basename $$file .c); \
		$(CC) $(CCFLAGS) -o $(STACK_DIR)/$$bin_file $$file -L$(LIB_DIR) -lthread -Wl,-rpath=$(LIB_DIR); \
	done

graphs: pthreads stack
	taskset -c 0 python3 $(TEST_DIR)/perf.py

install: all
	mkdir -p $(LIB_DIR) $(BIN_DIR)
	cp -n $(LIBRARY) $(LIB_DIR)
	cp -n $(BINARIES) $(BIN_DIR)

clean: 
	rm -rf $(BUILD_DIR) $(INSTALL_DIR)
