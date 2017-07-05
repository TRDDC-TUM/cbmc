/*******************************************************************\

Module: JSON symbol table language. Accepts a JSON format symbol
        table that has been produced out-of-process, e.g. by using
        a compiler's front-end.

Author: Chris Smowton, chris.smowton@diffblue.com

\*******************************************************************/

#include <sstream>
#include "json_symtab_language.h"
#include <json/json_parser.h>
#include <util/json_symbol_table.h>
#include <linking/linking.h>

void json_internal_additions(std::ostream &out)
{
  out << "[" << '\n';

  // __new_array (ulong, ulong)
  out << "{  \"base_name\": \"__new_array\",  \"is_auxiliary\": false,  \"is_exported\": false,  \"is_extern\": true,  \"is_file_local\": false,  \"is_input\": false,  \"is_lvalue\": false,  \"is_macro\": false,  \"is_output\": false,  \"is_parameter\": false,  \"is_property\": false,  \"is_state_var\": false,  \"is_static_lifetime\": false,  \"is_thread_local\": true,  \"is_type\": false,  \"is_volatile\": false,  \"is_weak\": false,  \"location\": {    \"id\": \"nil\"  },  \"mode\": \"cpp\",  \"name\": \"__new_array\",  \"pretty_name\": \"__new_array\",  \"type\": {    \"id\": \"code\",    \"namedSub\": {      \"parameters\": {        \"id\": \"parameters\",        \"sub\": [          {            \"id\": \"parameter\",            \"namedSub\": {              \"type\": {                \"id\": \"symbol\",                \"namedSub\" : { \"identifier\" : { \"id\" : \"unsigned_long\" }}              }            }          },          {            \"id\": \"parameter\",            \"namedSub\": {              \"type\": {                \"id\": \"symbol\",                \"namedSub\" : { \"identifier\" : { \"id\" : \"unsigned_long\" }}              }            }          }        ]      },      \"return_type\": {        \"id\": \"pointer\",        \"namedSub\": {          \"width\": {            \"id\": \"64\"          }        },        \"sub\": [          {            \"id\": \"empty\"          }        ]      }    }  },  \"value\": {    \"id\": \"nil\"  }   },{    \"base_name\": \"unsigned_long\",    \"is_auxiliary\": false,    \"is_exported\": false,    \"is_extern\": false,    \"is_file_local\": false,    \"is_input\": false,    \"is_lvalue\": false,    \"is_macro\": false,    \"is_output\": false,    \"is_parameter\": false,    \"is_property\": false,    \"is_state_var\": false,    \"is_static_lifetime\": false,    \"is_thread_local\": false,    \"is_type\": true,    \"is_volatile\": false,    \"is_weak\": false,    \"location\": {      \"id\": \"nil\"    },    \"mode\": \"\",    \"module\": \"\",    \"name\": \"unsigned_long\",    \"pretty_name\": \"unsigned_long\",    \"type\": {      \"id\": \"unsignedbv\",      \"namedSub\": {        \"width\": {          \"id\": \"64\"        }      },      \"sub\": []    },    \"value\": {      \"id\": \"nil\"    }  }" << '\n';

  out << "]" << '\n';
  out << std::flush;
}

bool json_symtab_languaget::parse(
  std::istream &instream,
  const std::string &path)
{

  // built-in additions
  std::ostringstream o_additions;
  json_internal_additions(o_additions);
  std::istringstream i_concat(o_additions.str());
  jsont json_additions;
  if (parse_json(
	  i_concat,
      path,
      get_message_handler(),
      json_additions))
  {
	  error() << "Cannot parse JSON built-in additions" << eom;
	  return true;
  }

  // user input
  if (parse_json(
    instream,
    path,
    get_message_handler(),
    parsed_json_file))
  {
	  error() << "Cannot parse JSON input" << eom;
	  return true;
  }

  // append built-in additions to JSON array
  assert (parsed_json_file.is_array() && json_additions.is_array());
  parsed_json_file.array.insert(
		  parsed_json_file.array.end(),
		  json_additions.array.begin(),
		  json_additions.array.end());

  return false;
}


bool json_symtab_languaget::typecheck(
  symbol_tablet &symbol_table,
  const std::string &module)
{
  symbol_tablet new_symbol_table;

  try
  {
    symbol_table_from_json(parsed_json_file, symbol_table);

    // FIXME: we might want to implement a type checker here, similarly to the C frontend:
    //if (json_typecheck(
    //	 tree, new_symbol_table, module, get_message_handler()))
    //  return true;

    return linking(symbol_table, new_symbol_table, get_message_handler());
  }
  catch(const std::string &str)
  {
    error() << "json_symtab_languaget::typecheck: " << str << eom;
    return true;
  }
}

void json_symtab_languaget::show_parse(std::ostream &out)
{
  parsed_json_file.output(out);
}
