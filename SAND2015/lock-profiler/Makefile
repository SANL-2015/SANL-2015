#/* ########################################################################## */
#/* (C) UPMC, 2010-2011                                                        */
#/*     Authors:                                                               */
#/*       Jean-Pierre Lozi <jean-pierre.lozi@lip6.fr>                          */
#/*       Gaël Thomas <gael.thomas@lip6.fr>                                    */
#/*       Florian David <florian.david@lip6.fr>                                */
#/*       Julia Lawall <julia.lawall@lip6.fr>                                  */
#/*       Gilles Muller <gilles.muller@lip6.fr>                                */
#/* -------------------------------------------------------------------------- */
#/* ########################################################################## */

ROOT=..

include ../Makefile.config

PROJECT=lock-profiler

SRCDIR=.

OBJ=lock-profiler.o

DEPEND_OPTIONS=-MMD -MP -MF ".$*.d.tmp" -MT "$*.o" -MT ".$*.d"
DOM=then mv -f ".$*.d.tmp" ".$*.d"; else rm -f ".$*.d.tmp"; exit 1; fi

CFLAGS   +=  -g -O3 -Wall -D_GNU_SOURCE -fPIC -I../liblock/
LDFLAGS  +=  -ldl -lbfd -L../liblock/ -llock

Echo=@echo [$(PROJECT)]: 

ifndef VERBOSE
  Verb := @
endif

DEPENDENCIES=$(patsubst %.o, .%.d, $(OBJ))

.PHONY: all bootstrap tidy clean distclean
.SECONDARY: 
.SUFFIXES:

all: bootstrap $(PROJECT)

$(PROJECT): $(PROJECT).in Makefile
	$(Echo) Generate $@
	$(Verb) cat $< | sed -e "s/@abs_top_srcdir@/$$(echo $(realpath $(SRCDIR)) | sed -e 's/\([\/&]\)/\\\1/g')/g" > $@
	$(Verb) chmod a+x $@

bootstrap: $(PROJECT).so

$(PROJECT).a: $(PROJECT)-single-object-file.o
	$(Echo) Archiving $@
	$(Verb) ar rcsf $@ $^

$(PROJECT)-single-object-file.o: $(OBJ)
	$(Echo) Building complete $@
	$(Verb) g++ -r -nostdlib -nodefaultlibs -nostartfiles $(LDFLAGS) -o $@ $(OBJ)

$(PROJECT).so: $(OBJ)
	$(Echo) Building complete $@
	$(Verb) g++ -shared -o $@ $(OBJ) $(LDFLAGS)

%.o: %.cc Makefile $(ROOT)/Makefile.config
	$(Echo) Compiling $<
	$(Verb) if g++ $(CXXFLAGS)  $(DEPEND_OPTIONS) -c "$<" -o "$@"; $(DOM)

%.o: %.cpp Makefile $(ROOT)/Makefile.config
	$(Echo) Compiling $<
	$(Verb) if g++ $(CXXFLAGS)  $(DEPEND_OPTIONS) -c "$<" -o "$@"; $(DOM)

%.o: %.cxx Makefile $(ROOT)/Makefile.config
	$(Echo) Compiling $<
	$(Verb) if g++ $(CXXFLAGS)  $(DEPEND_OPTIONS) -c "$<" -o "$@"; $(DOM)

%.o: %.c Makefile $(ROOT)/Makefile.config
	$(Echo) Compiling $<
	$(Verb) if gcc $(CFLAGS)  $(DEPEND_OPTIONS) -c "$<" -o "$@"; $(DOM)

%.s: %.c Makefile $(ROOT)/Makefile.config
	$(Echo) "Compiling $< (asm)"
	$(Verb) if gcc $(CFLAGS)  $(DEPEND_OPTIONS) -S "$<" -o "$@"; $(DOM)

tidy:
	rm -f *~ \#*

clean:
	$(Echo) Cleaning compilation files
	$(Verb) rm -f *.o .*.d $(MAIN)

distclean: clean
	$(Echo) Cleaning distribution
	$(Verb) rm -f $(PROJECT).a $(PROJECT).so $(PROJECT)

ifneq ($(MAKECMDGOALS),tidy)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
-include $(DEPENDENCIES)
endif
endif
endif

