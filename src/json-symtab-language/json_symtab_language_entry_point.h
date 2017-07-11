/*******************************************************************\

Module:

Author:

\*******************************************************************/

#ifndef CPROVER_JSON_SYMTAB_ENTRY_POINT_H
#define CPROVER_JSON_SYMTAB_ENTRY_POINT_H

#include <util/symbol_table.h>
#include <util/message.h>

bool json_symtab_language_entry_point(
  symbol_tablet &symbol_table,
  const std::string &standard_main,
  message_handlert &message_handler);

#endif // CPROVER_JSON_SYMTAB_ENTRY_POINT_H
