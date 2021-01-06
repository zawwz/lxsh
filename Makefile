## CONFIG ##

IDIR=include
SRCDIR=src
ODIR=obj
BINDIR=.

# binary name, default name of dir
NAME = $(shell readlink -f . | xargs basename)

# global links
LDFLAGS = -lpthread

# compiler
CC=g++
# compiler flags
CXXFLAGS= -I$(IDIR) -Wall -pedantic -std=c++17
ifeq	($(DEBUG),true)
  # debugging flags
  CC=clang++
  CXXFLAGS += -g -pg
else
  # release flags
  CXXFLAGS += -Ofast
endif

ifneq	($(RELEASE), true)
	VSUFFIX=-dev-$(SHA_SHORT)
endif

ifeq    ($(STATIC),true)
  # static links
  LDFLAGS += -l:libztd.a
else
  # dynamic links
  LDFLAGS += -lztd
endif

## END CONFIG ##

$(shell ./generate_version.sh)
$(shell mkdir -p $(ODIR))
$(shell mkdir -p $(BINDIR))

# automatically find .h and .hpp
DEPS = $(shell find $(IDIR) -type f -regex '.*\.hp?p?' ! -name 'g_version.h')
# automatically find .c and .cpp and make the corresponding .o rule
OBJ = $(shell find $(SRCDIR) -type f -regex '.*\.cp?p?' | sed 's|\.cpp|.o|g;s|\.c|.o|g;s|^$(SRCDIR)/|$(ODIR)/|g')

build: lxsh $(OBJ) $(DEPS)

$(ODIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) $(CXXFLAGS) -c -o $@ $<

$(ODIR)/%.o: $(SRCDIR)/%.cpp $(DEPS)
	$(CC) $(CXXFLAGS) -c -o $@ $<

$(ODIR)/main.o: $(SRCDIR)/main.cpp $(DEPS) $(IDIR)/g_version.h
	$(CC) $(CXXFLAGS) -c -o $@ $<

lxsh: $(OBJ)
	$(CC) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

test: $(BINDIR)/$(NAME)
	$(BINDIR)/$(NAME)

clean:
	rm $(ODIR)/*.o gmon.out

clear:
	rm $(BINDIR)/$(NAME)
