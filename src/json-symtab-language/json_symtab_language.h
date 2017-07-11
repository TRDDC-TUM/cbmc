/*******************************************************************\

Module: JSON symbol table language. Accepts a JSON format symbol
        table that has been produced out-of-process, e.g. by using
        a compiler's front-end.

Author: Chris Smowton, chris.smowton@diffblue.com

\*******************************************************************/

#ifndef CPROVER_JSON_SYMTAB_LANGUAGE_JSON_SYMTAB_LANGUAGE_H
#define CPROVER_JSON_SYMTAB_LANGUAGE_JSON_SYMTAB_LANGUAGE_H

#include <util/language.h>
#include <util/json.h>

class json_symtab_languaget:public languaget
{
 public:
  bool parse(
    std::istream &instream,
    const std::string &path) override;

  bool typecheck(
    symbol_tablet &symbol_table,
    const std::string &module) override;

  void show_parse(std::ostream &out) override;

  bool final(
    symbol_tablet &symbol_table) override;

  bool to_expr(
    const std::string &code,
    const std::string &module,
    exprt &expr,
    const namespacet &ns) override
  {
    // Not implemented.
    return true;
  }

  virtual std::set<std::string> extensions() const override
  {
    return {"json_symtab"};
  }

  languaget *new_language() override
  {
    return new json_symtab_languaget();
  }

 protected:
  jsont parsed_json_file;
};

inline languaget *new_json_symtab_language()
{
  return new json_symtab_languaget();
}

#endif
