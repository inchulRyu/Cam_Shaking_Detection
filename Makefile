APP:= run

CC=g++

LIB_INSTALL_DIR?=lib

SRCS:= $(wildcard ./*.cpp)

INCS:= $(wildcard ./*.h)

PKGS:= opencv4

CFLAGS+=$(shell pkg-config --cflags $(PKGS))
LIBS+=$(shell pkg-config --libs $(PKGS))

OBJS:= $(SRCS:.cpp=.o)

# CFLAGS+= -I./include

# LIBS+= -L$(LIB_INSTALL_DIR) -ltest_lib\
#        -Wl,-rpath,$(LIB_INSTALL_DIR)

all: $(APP)

%.o: %.cpp $(INCS) Makefile
	$(CC) -c -o $@ $(CFLAGS) $<

$(APP): $(OBJS) Makefile
	$(CC) -o $(APP) $(OBJS) $(LIBS)

clean:
	rm -rf $(OBJS) $(APP)