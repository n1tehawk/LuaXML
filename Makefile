# 2009-03-16 by gf

# generic compiler and linker settings:
INCDIR ?= -I../lua/src
LIBDIR ?= -L. -L../lua/src
LIB    = 
CFLAGS = -Os -Wall -c #-g

# generic platform specific rules:
ARCH            = $(shell uname -s)
ifeq ($(ARCH),Linux)
  CFLAGS += -fPIC
  LFLAGS =  -fPIC -shared
  LIBS          = $(LIBDIR) $(LIB) -llua -ldl
  EXESUFFIX =
  SHLIBSUFFIX = .so

else  
  ifeq ($(ARCH),Darwin) # MacOSX
    LFLAGS = -bundle 
    LIBS          = $(LIBDIR) -L/usr/local/lib $(LIB) -llua
    EXESUFFIX = .app
    SHLIBSUFFIX = .so
    
  else  # windows, MinGW
    LFLAGS =  -shared
    LIBS          = $(LIBDIR) $(LIB) -llua51 -mconsole -s
    EXESUFFIX = .exe
    SHLIBSUFFIX = .dll

  endif
endif

# project specific targets:
all:  LuaXML_lib$(SHLIBSUFFIX)

# project specific link rules:
LuaXML_lib$(SHLIBSUFFIX): LuaXML_lib.o utf8.o
	$(CC) -o $@ $(LFLAGS) $^ $(LIBS) 

# project specific dependencies:
LuaXML_lib.o:  LuaXML_lib.c
utf8.o:  utf8.c

# generic rules and targets:
.c.o:
	$(CC) $(CFLAGS) $(INCDIR) -c $<
clean:
	rm -f *.o *~ LuaXML_lib.so LuaXML_lib.dll

# run tests
LUA ?= lua
test:
	$(LUA) -v unittest.lua
	$(LUA) test.lua

# generate documentation (requires LDoc)
doc:
	ldoc -c .ldoc/config.ld .
