
ROOT=..

include $(ROOT)/Makefile.config

PROJECT=benchmark

BIN=$(PROJECT)
OBJ=benchmark.o mcs_lock.o

DEPEND_OPTIONS=-MMD -MP -MF ".$*.d.tmp" -MT "$*.o" -MT ".$*.d"
DOM=then mv -f ".$*.d.tmp" ".$*.d"; else rm -f ".$*.d.tmp"; exit 1; fi

CFLAGS   += -Wno-format-zero-length -g -Wall -Werror -D_GNU_SOURCE
CXXFLAGS += $(CFLAGS)
LDFLAGS  += -lm -llock -lpapi
Echo=@echo [$(PROJECT)]: 

ifndef VERBOSE
  Verb := @
endif

DEPENDENCIES=$(patsubst %.o, .%.d, $(OBJ))

.PHONY: all bootstrap tidy clean distclean liblock
.SECONDARY: 
.SUFFIXES:

all: liblock bootstrap

bootstrap: $(BIN)

# ../liblock/liblock.a

$(BIN): $(OBJ)
	$(Echo) Linking $@
	$(Verb) g++ -o $@ $^ $(LDFLAGS) 

liblock: 
	make -C $(LIBLOCK)

%.o: %.cc Makefile
	$(Echo) Compiling $<
	$(Verb) if g++ $(CXXFLAGS)  $(DEPEND_OPTIONS) -c "$<" -o "$@"; $(DOM)

%.o: %.cpp Makefile
	$(Echo) Compiling $<
	$(Verb) if g++ $(CXXFLAGS)  $(DEPEND_OPTIONS) -c "$<" -o "$@"; $(DOM)

%.o: %.cxx Makefile
	$(Echo) Compiling $<
	$(Verb) if g++ $(CXXFLAGS)  $(DEPEND_OPTIONS) -c "$<" -o "$@"; $(DOM)

%.o: %.c Makefile
	$(Echo) Compiling $<
	$(Verb) if gcc $(CFLAGS)  $(DEPEND_OPTIONS) -c "$<" -o "$@"; $(DOM)

%.s: %.c Makefile
	$(Echo) "Compiling $< (asm)"
	$(Verb) if gcc $(CFLAGS)  $(DEPEND_OPTIONS) -S "$<" -o "$@"; $(DOM)

tidy:
	rm -f *~ \#*

clean:
	$(Echo) Cleaning compilation files
	$(Verb) rm -f *.o .*.d $(MAIN)

distclean: clean
	$(Echo) Cleaning distribution
	$(Verb) rm -f $(BIN) $(PROJECT).a

ifneq ($(MAKECMDGOALS),tidy)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
-include $(DEPENDENCIES)
endif
endif
endif

