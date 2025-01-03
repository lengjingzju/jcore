############################################
# SPDX-License-Identifier: MIT             #
# Copyright (C) 2024-.... Jing Leng        #
# Contact: Jing Leng <lengjingzju@163.com> #
############################################

PACKAGE_NAME    = jcore

.PHONY: all clean install
all:
	@echo "Build $(PACKAGE_NAME) Done!"

VSRCS          := jwraphook.c
INC_MAKES      := app
include inc.makes

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

HOOK_LDFLAGS   += -pthread
$(call set_flags,CFLAGS,jhook.c jlisthook.c jtreehook.c,-O0 -g)
$(eval $(call add-libso-build,libjlisthook.so,jhook.c jlisthook.c,$(HOOK_LDFLAGS)))
$(eval $(call add-libso-build,libjtreehook.so,jhook.c jtreehook.c jtree.c,$(HOOK_LDFLAGS)))

# If you want to debug a static executable, you need to add the following WRAP_LDFLAGS when compiling:
WRAP_LDFLAGS   += -ljtreewrap -pthread
WRAP_LDFLAGS   += -Wl,--wrap=free -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=strdup -Wl,--wrap=strndup
$(call set_flags,CFLAGS,jwraphook.c,-O0 -g -DJHOOK_WRAP)
$(eval $(call compile_vobj,c,$$(CCC),jwraphook.c,jhook.c))
$(eval $(call add-liba-build,libjlistwrap.a,jwraphook.c jlisthook.c))
$(eval $(call add-liba-build,libjtreewrap.a,jwraphook.c jtreehook.c jtree.c))

all: $(LIB_TARGETS)

clean: clean_objs
	@rm -f $(LIB_TARGETS)
	@echo "Clean $(PACKAGE_NAME) Done."

install: install_libs
	@echo "Install $(PACKAGE_NAME) to $(INS_PREFIX) Done!"
