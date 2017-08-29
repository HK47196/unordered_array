CXX ?= g++
AR = ar

CXXFLAGS += -std=c++14 -Wall -Wextra -g3 -I. -Wpedantic

BASE_OBJ_FOLDER = ./objects
INC_FOLDER = ./includes
SRC_FOLDER = ./sources
OBJ_FOLDER = $(BASE_OBJ_FOLDER)/$(CXX)

ARFLAGS = rcs
INC = -I$(INC_FOLDER)

LDFLAGS = -pthread
LDLIBS = 

RM = rm -f

SOURCES = unordered_array_test.cxx catch.cxx

OBJECTS = $(patsubst %.cxx, $(OBJ_FOLDER)/%.o, $(SOURCES)) 

EXECUTABLE = tests


OPT ?= 0
ifeq ($(OPT), 0)
$(info $(shell tput setaf 3) NOT $(shell tput setaf 7) ENABLING OPTIMIZATION FLAGS. $(shell tput sgr0))
CXXFLAGS += -fsanitize=undefined -fsanitize=address
else
$(info $(shell tput setaf 7) NOT ENABLING OPTIMIZATION FLAGS. $(shell tput sgr0))
CXXFLAGS += -O3
endif

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJECTS) -o $@ $(LDLIBS)

$(OBJ_FOLDER)/%.o: %.cxx | $(OBJ_FOLDER)
	$(CXX) $(CXXFLAGS) $(INC) -o $@ -c $< 

$(OBJ_FOLDER): 
	mkdir -p $(OBJ_FOLDER)

clean:
	$(RM) -r $(BASE_OBJ_FOLDER)
	$(RM) $(EXECUTABLE)
