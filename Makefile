MODULE_big = pgbrotli
OBJS = pgbrotli.o
SHLIB_LINK = brotli/libbrotlicommon-static.a brotli/libbrotlidec-static.a brotli/libbrotlienc-static.a

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
