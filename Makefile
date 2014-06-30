##===- Contractor Makefile ----------------*- Makefile -*-===##
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
##===----------------------------------------------------------------------===##

CLANG_LEVEL := ../..
LIBRARYNAME = Contractor

# If we don't need RTTI or EH, there's no reason to export anything
# from the plugin.
ifneq ($(REQUIRES_RTTI), 1)
ifneq ($(REQUIRES_EH), 1)
EXPORTED_SYMBOL_FILE = $(PROJ_SRC_DIR)/Contractor.exports
endif
endif

LINK_LIBS_IN_SHARED = 0
LOADABLE_MODULE = 1

include $(CLANG_LEVEL)/Makefile

ifeq ($(OS),Darwin)
  LDFLAGS=-Wl,-undefined,dynamic_lookup
endif

CXXFLAGS += -std=c++0x
LIBS += -lboost_regex

test:
	@git checkout test.c
	@echo "============== running plugin ==============="
	@clang -Xclang -load -Xclang ../../../../Debug+Asserts/lib/Contractor.so -Xclang -plugin -Xclang print-fns -c test.c
	@echo "============================================="
	@echo ""
	@echo "============== generated file ==============="
	@clang-format -style=Google -i test.c
	@source-highlight -fesc -oSTDOUT -itest.c
	@echo "============================================="

test_inheritance:
	@git checkout test.cpp
	@echo "============== running plugin ==============="
	@clang -Xclang -load -Xclang ../../../../Debug+Asserts/lib/Contractor.so -Xclang -plugin -Xclang print-fns -c test.cpp
	@echo "============================================="
	@echo ""
	@echo "============== generated file ==============="
	@clang-format -style=Google -i test.cpp
	@source-highlight -fesc -oSTDOUT -itest.cpp
	@echo "============================================="
