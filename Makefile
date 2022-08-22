# contrib/leveldb_fdw/Makefile

MODULE_big = leveldb_fdw
OBJS = \
	$(WIN32RES) \
	leveldb_fdw.o
PGFILEDESC = "leveldb_fdw - foreign data wrapper for LevelDB"
PG_CPPFLAGS = -I$(libpq_srcdir)
SHLIB_LINK_INTERNAL = $(libpq)

EXTENSION = leveldb_fdw
DATA = leveldb_fdw--1.0.sql

REGRESS = leveldb_fdw # sql、expected 文件夹下的测试

leveldb_fdw: leveldb_fdw.o
	g++ -Wall -Wmissing-prototypes -Wpointer-arith -Wdeclaration-after-statement -Werror=vla -Werror=unguarded-availability-new -Wendif-labels -Wmissing-format-attribute -Wcast-function-type -Wformat-security -fno-strict-aliasing -fwrapv -Wno-unused-command-line-argument -Wno-compound-token-split-by-macro -g -O0  -bundle -multiply_defined suppress -o leveldb_fdw.so  leveldb_fdw.o -L../../src/port -L../../src/common -L../../src/interfaces/libpq -lpq -lleveldb -L./leveldb_install/lib -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk   -Wl,-dead_strip_dylibs   -bundle_loader ../../src/backend/postgres
leveldb_fdw.o:
	g++ -Wall -Wpointer-arith -Werror=unguarded-availability-new -Wendif-labels -Wmissing-format-attribute -Wcast-function-type -Wformat-security -fno-strict-aliasing -fwrapv -g -O2  -I../../src/interfaces/libpq -std=c++17 -I./leveldb_install/include -I. -I. -I../../src/include  -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk    -c -o leveldb_fdw.o leveldb_fdw.cpp

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
SHLIB_PREREQS = submake-libpq
subdir = contrib/leveldb_fdw
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
