TYPE		:= development

NATIVE		:= 0

SLIB		:= libclient.so

all: directories $(SLIB)

directories:
	@mkdir -p $(TARGETDIR)
	@mkdir -p $(BUILDDIR)

install: $(SLIB)
	install -m 644 ./bin/libclient.so /usr/local/lib/
	cp -R ./include/client /usr/local/include

uninstall:
	rm /usr/local/lib/libclient.so
	rm -r /usr/local/include/client

PTHREAD 	:= -l pthread
MATH		:= -lm

DEFINES		:= -D _GNU_SOURCE

DEVELOPMENT := -D CERVER_DEBUG 		\
				-D CLIENT_DEBUG 		\
				-D CONNECTION_DEBUG 	\
				-D HANDLER_DEBUG 		\
				-D PACKETS_DEBUG 		\
				-D AUTH_DEBUG 			\
				-D FILES_DEBUG

CC          := gcc

GCCVGTEQ8 	:= $(shell expr `gcc -dumpversion | cut -f1 -d.` \>= 8)

SRCDIR      := src
INCDIR      := include

BUILDDIR    := objs
TARGETDIR   := bin

SRCEXT      := c
DEPEXT      := d
OBJEXT      := o

# common flags
# -Wconversion
COMMON		:= -Wall -Wno-unknown-pragmas \
				-Wfloat-equal -Wdouble-promotion -Wint-to-pointer-cast -Wwrite-strings \
				-Wtype-limits -Wsign-compare -Wmissing-field-initializers \
				-Wuninitialized -Wmaybe-uninitialized -Wempty-body \
				-Wunused-parameter -Wunused-but-set-parameter -Wunused-result \
				-Wformat -Wformat-nonliteral -Wformat-security -Wformat-overflow -Wformat-signedness -Wformat-truncation

# main
CFLAGS      := $(DEFINES)

ifeq ($(TYPE), development)
	CFLAGS += -g -fasynchronous-unwind-tables $(DEVELOPMENT)
else ifeq ($(TYPE), test)
	CFLAGS += -g -fasynchronous-unwind-tables -D_FORTIFY_SOURCE=2 -fstack-protector -O2
else ifeq ($(TYPE), beta)
	CFLAGS += -g -D_FORTIFY_SOURCE=2 -O2
else
	CFLAGS += -D_FORTIFY_SOURCE=2 -O2
endif

# check which compiler we are using
ifeq ($(CC), g++) 
	CFLAGS += -std=c++11 -fpermissive
else
	CFLAGS += -std=c11 -Wpedantic -pedantic-errors
	# check for compiler version
	ifeq "$(GCCVGTEQ8)" "1"
    	CFLAGS += -Wcast-function-type
	else
		CFLAGS += -Wbad-function-cast
	endif
endif

ifeq ($(NATIVE), 1)
	CFLAGS += -march=native
endif

# common flags
CFLAGS += -fPIC $(COMMON)

LIB         := -L /usr/local/lib $(PTHREAD) $(MATH)

INC         := -I $(INCDIR) -I /usr/local/include
INCDEP      := -I $(INCDIR)

SOURCES     := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS     := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))

# pull in dependency info for *existing* .o files
-include $(OBJECTS:.$(OBJEXT)=.$(DEPEXT))

$(SLIB): $(OBJECTS)
	$(CC) $^ $(LIB) -shared -o $(TARGETDIR)/$(SLIB)

# compile sources
$(BUILDDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) $(LIB) -c -o $@ $<
	@$(CC) $(CFLAGS) $(INCDEP) -MM $(SRCDIR)/$*.$(SRCEXT) > $(BUILDDIR)/$*.$(DEPEXT)
	@cp -f $(BUILDDIR)/$*.$(DEPEXT) $(BUILDDIR)/$*.$(DEPEXT).tmp
	@sed -e 's|.*:|$(BUILDDIR)/$*.$(OBJEXT):|' < $(BUILDDIR)/$*.$(DEPEXT).tmp > $(BUILDDIR)/$*.$(DEPEXT)
	@sed -e 's/.*://' -e 's/\\$$//' < $(BUILDDIR)/$*.$(DEPEXT).tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $(BUILDDIR)/$*.$(DEPEXT)
	@rm -f $(BUILDDIR)/$*.$(DEPEXT).tmp

# examples
EXAMDIR		:= examples
EXABUILD	:= $(EXAMDIR)/objs
EXATARGET	:= $(EXAMDIR)/bin

EXAFLAGS	:= $(DEFINES)

ifeq ($(TYPE), development)
	EXAFLAGS += -g -D EXAMPLES_DEBUG -fasynchronous-unwind-tables
else ifeq ($(TYPE), test)
	EXAFLAGS += -g -fasynchronous-unwind-tables -D_FORTIFY_SOURCE=2 -fstack-protector -O2
else ifeq ($(TYPE), beta)
	EXAFLAGS += -g -D_FORTIFY_SOURCE=2 -O2
else
	EXAFLAGS += -D_FORTIFY_SOURCE=2 -O2
endif

# check which compiler we are using
ifeq ($(CC), g++) 
	EXAFLAGS += -std=c++11 -fpermissive
else
	EXAFLAGS += -std=c11 -Wpedantic -pedantic-errors
endif

ifeq ($(NATIVE), 1)
	EXAFLAGS += -march=native
endif

# common flags
EXAFLAGS += -Wall -Wno-unknown-pragmas

EXALIBS		:= -L ./$(TARGETDIR) -l client
EXAINC		:= -I ./$(INCDIR) -I ./$(EXAMDIR)

EXAMPLES	:= $(shell find $(EXAMDIR) -type f -name *.$(SRCEXT))
EXOBJS		:= $(patsubst $(EXAMDIR)/%,$(EXABUILD)/%,$(EXAMPLES:.$(SRCEXT)=.$(OBJEXT)))

examples: $(EXOBJS)
	@mkdir -p ./examples/bin
	$(CC) -I ./$(INCDIR) -L ./$(TARGETDIR) ./$(EXABUILD)/test.o -o ./$(EXATARGET)/test -l client
	$(CC) -I ./$(INCDIR) -L ./$(TARGETDIR) ./$(EXABUILD)/handlers.o -o ./$(EXATARGET)/handlers -l client
	$(CC) -I ./$(INCDIR) -L ./$(TARGETDIR) ./$(EXABUILD)/multi.o -o ./$(EXATARGET)/multi -l client
	$(CC) -I ./$(INCDIR) -L ./$(TARGETDIR) ./$(EXABUILD)/packets.o -o ./$(EXATARGET)/packets -l client
	$(CC) -I ./$(INCDIR) -L ./$(TARGETDIR) ./$(EXABUILD)/requests.o -o ./$(EXATARGET)/requests -l client
	$(CC) -I ./$(INCDIR) -L ./$(TARGETDIR) ./$(EXABUILD)/files.o -o ./$(EXATARGET)/files -l client
	$(CC) -I ./$(INCDIR) -L ./$(TARGETDIR) ./$(EXABUILD)/auth.o -o ./$(EXATARGET)/auth -l client
	$(CC) -I ./$(INCDIR) -L ./$(TARGETDIR) ./$(EXABUILD)/sessions.o -o ./$(EXATARGET)/sessions -l client
	$(CC) -I ./$(INCDIR) -L ./$(TARGETDIR) ./$(EXABUILD)/admin.o -o ./$(EXATARGET)/admin -l client
	$(CC) -I ./$(INCDIR) -L ./$(TARGETDIR) ./$(EXABUILD)/balancer.o -o ./$(EXATARGET)/balancer -l client
	$(CC) -I ./$(INCDIR) -L ./$(TARGETDIR) ./$(EXABUILD)/logs.o -o ./$(EXATARGET)/logs -l client

# compile examples
$(EXABUILD)/%.$(OBJEXT): $(EXAMDIR)/%.$(SRCEXT)
	@mkdir -p $(dir $@)
	$(CC) $(EXAFLAGS) $(INC) $(EXALIBS) -c -o $@ $<
	@$(CC) $(EXAFLAGS) $(INCDEP) -MM $(EXAMDIR)/$*.$(SRCEXT) > $(EXABUILD)/$*.$(DEPEXT)
	@cp -f $(EXABUILD)/$*.$(DEPEXT) $(EXABUILD)/$*.$(DEPEXT).tmp
	@sed -e 's|.*:|$(EXABUILD)/$*.$(OBJEXT):|' < $(EXABUILD)/$*.$(DEPEXT).tmp > $(EXABUILD)/$*.$(DEPEXT)
	@sed -e 's/.*://' -e 's/\\$$//' < $(EXABUILD)/$*.$(DEPEXT).tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $(EXABUILD)/$*.$(DEPEXT)
	@rm -f $(EXABUILD)/$*.$(DEPEXT).tmp

clear: clean-objects clean-examples

clean: clear
	@$(RM) -rf $(TARGETDIR)

clean-objects:
	@$(RM) -rf $(BUILDDIR)

clean-examples:
	@$(RM) -rf $(EXABUILD)
	@$(RM) -rf $(EXATARGET)

.PHONY: all clean clear examples