############################################
# SPDX-License-Identifier: MIT             #
# Copyright (C) 2024-.... Jing Leng        #
# Contact: Jing Leng <lengjingzju@163.com> #
############################################

PACKAGE_NAME    = jcore

.PHONY: all clean install
all:
	@echo "Build $(PACKAGE_NAME) Done!"

INC_MAKES      := app
include inc.makes

$(call set_flags,CFLAGS,jhook.c jlisthook.c jtreehook.c,-O0 -g)
$(eval $(call add-libso-build,libjlisthook.so,jhook.c jlisthook.c,-pthread))
$(eval $(call add-libso-build,libjtreehook.so,jhook.c jtreehook.c jtree.c,-pthread))

all: $(LIB_TARGETS)

clean: clean_objs
	@rm -f $(LIB_TARGETS)
	@echo "Clean $(PACKAGE_NAME) Done."

install: install_libs
	@echo "Install $(PACKAGE_NAME) to $(INS_PREFIX) Done!"
