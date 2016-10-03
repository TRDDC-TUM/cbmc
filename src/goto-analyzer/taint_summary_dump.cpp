/////////////////////////////////////////////////////////////////////////////
//
// Module: taint_summary
// Author: Marek Trtik
//
// @ Copyright Diffblue, Ltd.
//
/////////////////////////////////////////////////////////////////////////////


#include <goto-analyzer/taint_summary_dump.h>
#include <memory>
#include <set>
#include <iostream>
#include <iomanip>
#include <cassert>

namespace sumfn { namespace taint { namespace detail { namespace {


}}}}

namespace sumfn { namespace taint {


void  dump_lvalue_in_html(
    lvaluet const&  lvalue,
    namespacet const&  ns,
    std::ostream&  ostr
    )
{
  if (is_identifier(lvalue))
    ostr << to_html_text(name_of_symbol_access_path(lvalue));
  else
    ostr << to_html_text(from_expr(ns, "", lvalue));
}

void  dump_svalue_in_html(
    svaluet const&  svalue,
    std::ostream&  ostr
    )
{
  if (svalue.is_top())
    ostr << "TOP";
  else if (svalue.is_bottom())
    ostr << "BOTTOM";
  else
  {
    bool first = true;
    for (auto const&  symbol : svalue.expression())
    {
       ostr << (first ? "" : " &#x2210; ") << symbol;
       first = false;
    }
  }
}

void  dump_lvalues_to_svalues_in_html(
    map_from_lvalues_to_svaluest const&  lvalues_to_svalues,
    namespacet const&  ns,
    std::ostream&  ostr
    )
{
  if (lvalues_to_svalues.empty())
    ostr << "BOTTOM";
  else
  {
    ostr << "    <table>\n";
    for (auto const&  elem : lvalues_to_svalues)
    {
      ostr << "      <tr>\n";
      ostr << "        <td>";
      dump_lvalue_in_html(elem.first,ns,ostr);
      ostr << "</td>\n";
      ostr << "        <td>";
      dump_svalue_in_html(elem.second,ostr);
      ostr << "</td>\n";
      ostr << "      </tr>\n";
    }
    ostr << "    </table>\n";
  }
}


std::string  dump_in_html(
    object_summaryt const  obj_summary,
    goto_modelt const&  program,
    std::ostream&  ostr
    )
{
  namespacet const  ns(program.symbol_table);
  summarised_object_idt const&  function_id = obj_summary.first;
  summary_ptrt const  summary =
      std::dynamic_pointer_cast<summaryt const>(obj_summary.second);
  if (!summary.operator bool())
    return "ERROR: cannot cast the passed summary to 'taint' summary.";

  ostr << "<h2>Taint summary</h2>\n"
       << "<p>Mapping of input to symbols:</p>\n"
          "<table>\n"
          "  <tr>\n"
          "    <th>L-value</th>\n"
          "    <th>Symbol</th>\n"
          "  </tr>\n"
       ;
  for (auto const&  elem : summary->input())
  {
    ostr << "  <tr>\n";
    ostr << "    <td>";
    dump_lvalue_in_html(elem.first,ns,ostr);
    ostr << "</td>\n";
    ostr << "    <td>"; dump_svalue_in_html(elem.second,ostr); ostr << "</td>\n";
    ostr << "  </tr>\n";
  }
  ostr << "</table>\n";
  ostr << "<p>The summary:</p>\n"
          "<table>\n"
          "  <tr>\n"
          "    <th>L-value</th>\n"
          "    <th>Expression</th>\n"
          "  </tr>\n"
       ;
  for (auto const&  elem : summary->output())
  {
    ostr << "  <tr>\n";
    ostr << "    <td>";
    dump_lvalue_in_html(elem.first,ns,ostr);
    ostr << "</td>\n";
    ostr << "    <td>"; dump_svalue_in_html(elem.second,ostr); ostr << "</td>\n";
    ostr << "  </tr>\n";
  }
  ostr << "</table>\n";

  if (summary->domain())
  {
    auto const  fn_it =
        program.goto_functions.function_map.find(irep_idt(function_id));
    if (fn_it != program.goto_functions.function_map.cend())
    {
      goto_programt const&  fn_body = fn_it->second.body;
      ostr << "<h3>Domain</h3>\n"
              "<table>\n"
              "  <tr>\n"
              "    <th>Loc</th>\n"
              "    <th>Targets</th>\n"
              "    <th>Instruction</th>\n"
              "    <th>Domain value</th>\n"
              "  </tr>\n"
           ;
      namespacet const  ns(program.symbol_table);
      for (auto  instr_it = fn_body.instructions.cbegin();
          instr_it != fn_body.instructions.cend();
          ++instr_it)
      {
        ostr << "  <tr>\n";

        // Dumping program location
        ostr << "    <td>"
             << instr_it->location_number
             << "</td>\n"
             ;

        // Dumping targets
        if (instr_it->is_target())
          ostr << "    <td>" << instr_it->target_number << "</td>\n";
        else
          ostr << "    <td>    </td>\n";

        // Dumping instruction
        ostr << "    <td>\n";
        dump_instruction_code_in_html(*instr_it,program,ostr);
        ostr << "</td>\n";

        // Dumping taint domain
        auto const  vars_to_values_it = summary->domain()->find(instr_it);
        if (vars_to_values_it != summary->domain()->cend())
        {
          ostr << "  <td>\n";
          dump_lvalues_to_svalues_in_html(vars_to_values_it->second,ns,ostr);
          ostr << "  </td>\n";
        }

        ostr << "  </tr>\n";
      }
      ostr << "</table>\n";
    }
  }

  return ""; // no error.
}
  
  
}}
