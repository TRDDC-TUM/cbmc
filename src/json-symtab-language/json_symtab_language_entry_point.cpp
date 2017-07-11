/*******************************************************************\

Module:

Author:

\*******************************************************************/

#include <cassert>
#include <cstdlib>

#include <util/namespace.h>
#include <util/std_expr.h>
#include <util/arith_tools.h>
#include <util/std_code.h>
#include <util/config.h>
#include <util/cprover_prefix.h>
#include <util/prefix.h>

#include <goto-programs/goto_functions.h>
#include <linking/static_lifetime_init.h>

#include "json_symtab_language_entry_point.h"

bool json_symtab_language_entry_point(
  symbol_tablet &symbol_table,
  const std::string &standard_main,
  message_handlert &message_handler)
{

	messaget message(message_handler);
	message.warning() << "json_symtab_languaget::final: User entry point ignored." << messaget::eom;

	// TODO: find entry point similarly to ansi_c_language.cpp::ansi_c_entry_point
	return false;
}
