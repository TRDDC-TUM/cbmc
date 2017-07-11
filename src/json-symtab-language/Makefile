SRC = json_symtab_language.cpp json_symtab_language_entry_point.cpp

INCLUDES= -I ..

include ../config.inc
include ../common

CLEANFILES = json_symtab_language$(LIBEXT)

all: json_symtab_language$(LIBEXT)

###############################################################################

json_symtab_language$(LIBEXT): $(OBJ)
	$(LINKLIB)
