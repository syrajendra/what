EXE_NAME 	= what
BIN 		= bin
INCLUDES 	= -Iinc
DEBUG 		= -g2
#ifeq ($(CXX), g++)
STDFLAG = -std=gnu++11
#else
STDFLAG = -std=c++11
#endif
CXXFLAGS 	= -O2 -c $(DEBUG) $(INCLUDES) -Wall $(STDFLAG)
MKDIR 		= mkdir

LDFLAGS     = -lpthread
VPATH       = src

SOURCES		= src/what.cpp

OBJ_SUFFIX 		= o
SOURCES_BARE    = $(notdir $(SOURCES))
OBJECTS 		= ${SOURCES_BARE:%.cpp=.objects/%.$(OBJ_SUFFIX)}

all: compile

.objects/%.$(OBJ_SUFFIX): %.cpp
	@mkdir -p $(dir $@)
	@echo Compiling: $<
	$(CXX) $(CXXFLAGS) -o $@ $<

compile: clean-objs ${OBJECTS}
	$(eval EXE_PATH := $(BIN))
	@$(MKDIR) -p $(EXE_PATH)
	$(eval EXE := $(EXE_PATH)/$(EXE_NAME))
	@echo Linking: $(EXE)
	$(CXX) -o $(EXE) $(OBJECTS) $(LDFLAGS)

clean-objs:
	@rm -rf .objects*

clean:
	@echo "Cleaning..."
	@rm -rf $(BIN)/* .objects*
