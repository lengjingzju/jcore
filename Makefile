############################################
# SPDX-License-Identifier: MIT             #
# Copyright (C) 2024-.... Jing Leng        #
# Contact: Jing Leng <lengjingzju@163.com> #
# https://github.com/lengjingzju/jcore     #
############################################

PACKAGE_NAME    = jcore

majorver       := $(shell cat common/jlog_core.h | grep JCORE_VERSION | sed 's/.*0x//g' | cut -b 1-2 | sed 's/^0//g')
minorver       := $(shell cat common/jlog_core.h | grep JCORE_VERSION | sed 's/.*0x//g' | cut -b 3-4 | sed 's/^0//g')
patchver       := $(shell cat common/jlog_core.h | grep JCORE_VERSION | sed 's/.*0x//g' | cut -b 5-6 | sed 's/^0//g')

lib            := jcore
staticlib      := lib$(lib).a
sharedlib      := lib$(lib).so $(majorver) $(minorver) $(patchver)

OSDIR          ?= posix
CPFLAGS        += -D_GNU_SOURCE -Icommon -I$(OSDIR) -Wno-unused-parameter -Wno-unused-result
CXXFLAGS       += -Wno-missing-field-initializers

ifeq ($(JHEAP_DEBUG),y)
CPFLAGS        += -DJHEAP_DEBUG
endif

.PHONY: all clean install
all:
	@echo "Build $(PACKAGE_NAME) Done!"

ASRCS           = $(OBJ_PREFIX)/jplist.c $(OBJ_PREFIX)/jprbtree.c  $(OBJ_PREFIX)/jphashmap.c
AHDRS           = $(patsubst %.c,%.h,$(ASRCS))
VSRCS          := jhook/jwraphook.c
INC_MAKES      := app
object_byte_size= 16384
frame_byte_size = 16384
ENV_BUILD_TYPE := release
include inc.makes

ifneq ($(test_threads), )
CPFLAGS        += -DTEST_THREADS=$(test_threads)
endif

ifneq ($(test_bufsize), )
CPFLAGS        += -DTEST_BUFSIZE=$(test_bufsize)
endif

ifneq ($(jlog_timestamp), )
CPFLAGS        += -DJLOG_TIMESTAMP=$(jlog_timestamp)
endif

ifeq ($(check_tofile),y)
CPFLAGS        += -DJHOOK_FILE
endif
ifneq ($(check_cycle), )
CPFLAGS        += -DCHECK_COUNT=$(shell expr $(check_cycle) \* 100) -DCHECK_FLAG=1
endif
ifneq ($(check_depth),)
CPFLAGS        += -DJHOOK_DEPTH=$(check_depth)
endif
ifeq ($(check_unwind),y)
CPFLAGS        += -DJHOOK_UNWIND -Wno-unused-parameter
HOOK_LDFLAGS   += -lunwind
WRAP_LDFLAGS   += -lunwind
endif

LIST_CMD       := $(shell pwd)/template/jlist.sh
$(OBJ_PREFIX)/jplist.c $(OBJ_PREFIX)/jplist.h: $(LIST_CMD)
	@mkdir -p $(OBJ_PREFIX) && cd $(OBJ_PREFIX) && $(LIST_CMD) jplist "struct jplist" "void*"

TREE_CMD       := $(shell pwd)/template/jrbtree.sh
$(OBJ_PREFIX)/jprbtree.c $(OBJ_PREFIX)/jprbtree.h: $(TREE_CMD)
	@mkdir -p $(OBJ_PREFIX) && cd $(OBJ_PREFIX) && $(TREE_CMD) jprbtree "struct jprbtree" "void*"

HASH_CMD       := $(shell pwd)/template/jhashmap.sh
$(OBJ_PREFIX)/jphashmap.c $(OBJ_PREFIX)/jphashmap.h: $(HASH_CMD)
	@mkdir -p $(OBJ_PREFIX) && cd $(OBJ_PREFIX) && $(HASH_CMD) jphashmap "struct jphashmap" "void*"

jcore-srcs     := $(wildcard $(OSDIR)/*.c common/*.c) $(ASRCS)
$(eval $(call add-liba-build,$(staticlib),$(jcore-srcs)))
$(eval $(call add-libso-build,$(sharedlib),$(jcore-srcs),-pthread))

ifeq ($(OSDIR),posix)
HOOK_LDFLAGS   += -pthread
$(call set_flags,CFLAGS,$(addprefix jhook/,jhook.c jlisthook.c jtreehook.c),-O0 -g)
$(eval $(call add-libso-build,libjlisthook.so,$(addprefix jhook/,jhook.c jlisthook.c),$(HOOK_LDFLAGS)))
$(eval $(call add-libso-build,libjtreehook.so,$(addprefix jhook/,jhook.c jtreehook.c jtree.c),$(HOOK_LDFLAGS)))

# If you want to debug a static executable, you need to add the following WRAP_LDFLAGS when compiling:
WRAP_LDFLAGS   += -ljtreewrap -pthread
WRAP_LDFLAGS   += -Wl,--wrap=free -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=strdup -Wl,--wrap=strndup
$(call set_flags,CFLAGS,jhook/jwraphook.c,-O0 -g -DJHOOK_WRAP)
$(eval $(call compile_vobj,c,$$(CCC),jhook/jwraphook.c,jhook/jhook.c))
$(eval $(call add-liba-build,libjlistwrap.a,$(addprefix jhook/,jwraphook.c jlisthook.c)))
$(eval $(call add-liba-build,libjtreewrap.a,$(addprefix jhook/,jwraphook.c jtreehook.c jtree.c)))
endif

CHECK_INFO     += JCORE_SHARED_TEST=$(JCORE_SHARED_TEST)
ifneq ($(JCORE_SHARED_TEST),y)
LINKA          := $(call set_links,$(lib)) -pthread
ifeq ($(ENV_BUILD_TYPE),debug)
LINKB          := $(LINKA)
else
LINKB          := $(call set_links,$(lib))
endif
$(eval $(call add-bin-build,jlog_client_test,test/jlog_client_test.c,$(LINKA),,$(OBJ_PREFIX)/$(staticlib)))
$(eval $(call add-bin-build,jsock_client_test,test/jsock_client_test.c,$(LINKB),,$(OBJ_PREFIX)/$(staticlib)))
$(eval $(call add-bin-build,jsock_udp_test,test/jsock_udp_test.c,$(LINKB),,$(OBJ_PREFIX)/$(staticlib)))
$(eval $(call add-bin-build,jlog_server_test,test/jlog_server_test.c,$(LINKA),,$(OBJ_PREFIX)/$(staticlib)))
$(eval $(call add-bin-build,jini_test,test/jini_test.c,$(LINKA),,$(OBJ_PREFIX)/$(staticlib)))
$(eval $(call add-bin-build,jheap_debug_test,test/jheap_debug_test.c,$(LINKA),,$(OBJ_PREFIX)/$(staticlib)))
$(eval $(call add-bin-build,jvector_test,test/jvector_test.c,$(LINKA),,$(OBJ_PREFIX)/$(staticlib)))
$(eval $(call add-bin-build,jstring_test,test/jstring_test.c,$(LINKA),,$(OBJ_PREFIX)/$(staticlib)))
$(eval $(call add-bin-build,jrbtree_test,test/jrbtree_test.c,$(LINKA),,$(OBJ_PREFIX)/$(staticlib)))
$(eval $(call add-bin-build,jpthread_test,test/jpthread_test.c,$(LINKA),,$(OBJ_PREFIX)/$(staticlib)))
$(eval $(call add-bin-build,jpqueue_test,test/jpqueue_test.c,$(LINKA),,$(OBJ_PREFIX)/$(staticlib)))
$(eval $(call add-bin-build,jhashmap_test,test/jhashmap_test.c,$(LINKA),,$(OBJ_PREFIX)/$(staticlib)))
$(eval $(call add-bin-build,jbitmap_test,test/jbitmap_test.c,$(LINKA),,$(OBJ_PREFIX)/$(staticlib)))

else
LINKA          := -l$(lib) -pthread
LINKB          := -l$(lib)
$(eval $(call add-bin-build,jlog_client_test,test/jlog_client_test.c,$(LINKA),,$(OBJ_PREFIX)/lib$(lib).so))
$(eval $(call add-bin-build,jsock_client_test,test/jsock_client_test.c,$(LINKB),,$(OBJ_PREFIX)/lib$(lib).so))
$(eval $(call add-bin-build,jsock_udp_test,test/jsock_udp_test.c,$(LINKB),,$(OBJ_PREFIX)/lib$(lib).so))
$(eval $(call add-bin-build,jlog_server_test,test/jlog_server_test.c,$(LINKA),,$(OBJ_PREFIX)/lib$(lib).so))
$(eval $(call add-bin-build,jini_test,test/jini_test.c,$(LINKB),,$(OBJ_PREFIX)/lib$(lib).so))
$(eval $(call add-bin-build,jheap_debug_test,test/jheap_debug_test.c,$(LINKB),,$(OBJ_PREFIX)/lib$(lib).so))
$(eval $(call add-bin-build,jvector_test,test/jvector_test.c,$(LINKB),,$(OBJ_PREFIX)/lib$(lib).so))
$(eval $(call add-bin-build,jstring_test,test/jstring_test.c,$(LINKB),,$(OBJ_PREFIX)/lib$(lib).so))
$(eval $(call add-bin-build,jrbtree_test,test/jrbtree_test.c,$(LINKB),,$(OBJ_PREFIX)/lib$(lib).so))
$(eval $(call add-bin-build,jpthread_test,test/jpthread_test.c,$(LINKB),,$(OBJ_PREFIX)/lib$(lib).so))
$(eval $(call add-bin-build,jpqueue_test,test/jpqueue_test.c,$(LINKB),,$(OBJ_PREFIX)/lib$(lib).so))
$(eval $(call add-bin-build,jhashmap_test,test/jhashmap_test.c,$(LINKB),,$(OBJ_PREFIX)/lib$(lib).so))
$(eval $(call add-bin-build,jbitmap_test,test/jbitmap_test.c,$(LINKB),,$(OBJ_PREFIX)/lib$(lib).so))
endif

INSTALL_HEADERS   = common/*.h $(OSDIR)/*.h $(AHDRS)
INSTALL_BINARIES  = $(BIN_TARGETS) test/jlog.ini template/*.sh

all: $(BIN_TARGETS) $(LIB_TARGETS)

clean: clean_objs
	@rm -f $(LIB_TARGETS) $(BIN_TARGETS) $(ASRCS)
	@echo "Clean $(PACKAGE_NAME) Done."

pkgconfigdir   := /usr/lib/pkgconfig
INSTALL_PKGCONFIGS := pcfiles/*
$(eval $(call install_obj,pkgconfig))

install: install_hdrs install_libs install_bins install_pkgconfigs
	@sed -i "s/@Version@/$(majorver).$(minorver).$(patchver)/g" $(INS_PREFIX)$(pkgconfigdir)/jcore.pc
	@echo "Install $(PACKAGE_NAME) to $(INS_PREFIX) Done."
