MODULE_big = pgbrotli
OBJS = pgbrotli.o
SHLIB_LINK = brotli_install/lib/libbrotlicommon-static.a brotli_install/lib/libbrotlidec-static.a brotli_install/lib/libbrotlienc-static.a
PG_CPPFLAGS = -I./brotli_install/include

EXTENSION = pgbrotli
DATA = pgbrotli--1.0.sql

REGRESS = brotli_testdata

ifdef NO_PGXS
subdir = contrib/pgbrotli
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
else
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
endif
