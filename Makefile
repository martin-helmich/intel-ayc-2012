# sample Makefile.
# It compiles every .cpp files in the src/ directory to object files in obj/ directory, and build the ./run executable.
# It automatically handles include dependencies.

# You can modify it as you want (add libraries, compiler flags...).
# But if you want it to properly run on our benchmark service, don't rename or delete variables.

# Use Intel Compiler when available, else GCC.
OMA_OPENMP = 1
ICC = $(shell which icpc)
ifneq ($(ICC), )
COMPILER ?= $(ICC_PATH)icpc
FLAGS ?= -std=c++0x -U__GXX_EXPERIMENTAL_COMPILER0X__ -xHOST -fast -w1 $(ICC_SUPPFLAGS)
else
COMPILER ?= $(GCC_PATH)g++
FLAGS ?= -g -O3 -Wall $(GCC_SUPPFLAGS)
endif

LDFLAGS ?= -g
LDLIBS = -ltbb -ltbbmalloc
UNAME = $(shell uname)

ifneq ($(UNAME), Darwin)
LDLIBS += -lrt
endif

ifdef OMA_OPENMP
FLAGS += -fopenmp -DOMA_OPENMP=1
LDLIBS += -lgomp
endif

EXECUTABLE = run

MKDIR_P = mkdir -p obj/oma

SRCS=$(wildcard src/*.cpp) $(wildcard src/oma/*.cpp)
OBJS=$(SRCS:src/%.cpp=obj/%.o)


all: prepare release

obj/%.o: src/%.cpp
	$(COMPILER) $(FLAGS) -o $@ -c $<

# create obj folder if it not exist 
prepare:
	${MKDIR_P} 

release: $(OBJS)
	$(COMPILER) $(LDFLAGS) -o $(EXECUTABLE) $(OBJS) $(LDLIBS) 

zip: dist-clean
ifdef TEAM_ID
	zip $(strip $(TEAM_ID)).zip -r src Makefile
else
	@echo "you need to put your TEAM_ID in the Makefile"
endif

submit: test zip
ifdef TEAM_ID
	curl -F "file=@$(strip $(TEAM_ID)).zip" -L http://www.intel-software-academic-program.com/contests/ayc/2012-11/upload/upload.php
else
	@echo "you need to put your TEAM_ID in the Makefile"
endif
	
#automatically handle include dependencies
depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	@$(foreach SRC, $(SRCS), $(COMPILER) $(FLAGS) -MT $(SRC:src/%.cpp=obj/%.o) -MM $(SRC) >> .depend;)

include .depend

clean:
	rm -rf obj/*
	rm -f run
	rm .depend
dist-clean: clean
	rm -f $(EXECUTABLE) *~ .depend *.zip

test: all
	./test.sh
