PROJECT = COMTRADE_transfer
RULES_MAKEFILE = ../../makefile_gnu.ver

LIBS_USER = -rdynamic -ldl -lcurl

ifeq ($(MAKELEVEL),0)
  RPATH=.build/$(VERSION)_$(BOARD)
  include $(RULES_MAKEFILE)
else
  vpath %.c   ../../
  O_DEPEND := $(wildcard ../../*.h)
  include ../../$(RULES_MAKEFILE)
endif

#-----------------------------------
all:
	@mkdir -p $(RPATH)
	@cd $(RPATH) && $(MAKE) -f ../../makefile -j $(MAKEFLAGS) $(SYTEM_TYPE)

#-----------------------------------
LINUX: $(PROJECT).so
ifdef COPY_SO
	cp *.so $(COPY_SO)
endif

WIN32: $(PROJECT).dll
ifdef COPY_DLL
	cp *.dll $(COPY_DLL)
endif

#----------------------------------------------
$(PROJECT).so: $(LIB_DEPEND) $(PROJECT).o
$(PROJECT).dll: $(LIB_DEPEND) $(PROJECT).o
