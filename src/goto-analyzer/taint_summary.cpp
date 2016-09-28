/////////////////////////////////////////////////////////////////////////////
//
// Module: taint_summary
// Author: Marek Trtik
//
// @ Copyright Diffblue, Ltd.
//
/////////////////////////////////////////////////////////////////////////////


#include <goto-analyzer/taint_summary.h>
#include <goto-analyzer/taint_summary_dump.h>
#include <summaries/summary_dump.h>
#include <util/std_types.h>
#include <util/file_util.h>
#include <util/msgstream.h>
#include <analyses/ai.h>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <stdexcept>

#include <iostream>


namespace sumfn { namespace taint { namespace detail { namespace {


/**
 *
 */
svaluet  make_symbol()
{
  static uint64_t  counter = 0UL;
  std::string const  symbol_name =
      msgstream() << "T" << ++counter;
  return {{symbol_name},false,false};
}

/**
 *
 */
svaluet  make_bottom()
{
  return {svaluet::expressiont{},true,false};
}

/**
 *
 *
 */
svaluet  make_top()
{
  return {svaluet::expressiont{},false,true};
}


//bool  is_identifier(lvaluet const  lvalue)
//{
//  return lvalue.id() == ID_symbol;
//}

bool  is_parameter(lvaluet const  lvalue, namespacet const&  ns)
{
  if (lvalue.id() == ID_symbol)
  {
    symbolt const*  symbol = nullptr;
    ns.lookup(to_symbol_expr(lvalue).get_identifier(),symbol);
    return symbol != nullptr && symbol->is_parameter;
  }
  return false;
}

bool  is_static(lvaluet const  lvalue, namespacet const&  ns)
{
  if (lvalue.id() == ID_symbol)
  {
    symbolt const*  symbol = nullptr;
    ns.lookup(to_symbol_expr(lvalue).get_identifier(),symbol);
    return symbol != nullptr && symbol->is_static_lifetime;
  }
  else if (lvalue.id() == ID_member)
  {
  }
  return false;
}

bool  is_return_value_auxiliary(lvaluet const  lvalue, namespacet const&  ns)
{
  if (lvalue.id() == ID_symbol)
  {
    irep_idt const&  name = to_symbol_expr(lvalue).get_identifier();
    symbolt const*  symbol = nullptr;
    ns.lookup(name,symbol);
    return symbol != nullptr &&
           symbol->is_static_lifetime &&
           symbol->is_auxiliary &&
           symbol->is_file_local &&
           symbol->is_thread_local &&
           as_string(name).find("#return_value") != std::string::npos
           ;
  }
  return false;
}

bool  is_pure_local(lvaluet const  lvalue, namespacet const&  ns)
{
  return lvalue.id() != ID_member &&
         !is_parameter(lvalue,ns) &&
         !is_static(lvalue,ns)
         ;
}


void  collect_lvalues(
    exprt const&  expr,
    lvalues_sett&  result
    )
{
  if (expr.id() == ID_symbol || expr.id() == ID_member)
    result.insert(expr);
  else
    for (exprt const&  op : expr.operands())
      collect_lvalues(op,result);
}


/**
 *
 */
void  initialise_domain(
    goto_functionst::goto_functiont const&  function,
    namespacet const&  ns,
    domaint&  domain,
    std::ostream* const  log
    )
{
  if (log != nullptr)
  {
    *log << "<h3>Initialising the domain</h3>\n"
            "<p>Locations initialised by element ";
    dump_lvalues_to_svalues_in_html(map_from_lvalues_to_svaluest(),ns,*log);
    *log << " : { ";
  }

  for (auto  it = function.body.instructions.cbegin();
       it != function.body.instructions.cend();
       ++it)
  {
    domain.insert({
        it,
        map_from_lvalues_to_svaluest()
        });

    if (log != nullptr)
      *log << it->location_number << ", ";
  }

  lvalues_sett  environment;
  for (auto  it = function.body.instructions.cbegin();
       it != function.body.instructions.cend();
       ++it)
    if (it->type == ASSIGN)
    {
      code_assignt const&  asgn = to_code_assign(it->code);
      environment.insert(asgn.lhs());
      collect_lvalues(asgn.rhs(),environment);
    }

  if (log != nullptr)
    if (!environment.empty())
      *log << " }</p>\n"
              "<p>Collecting lvalues representing function's environment "
              " and mapping them to fresh symbols:</p>\n"
              "<ul>\n"
           ;

  auto& map = domain.at(function.body.instructions.cbegin());
  for (lvaluet const&  lvalue : environment)
    if (!is_pure_local(lvalue,ns) && !is_return_value_auxiliary(lvalue,ns))
    {
      map.insert({lvalue, detail::make_symbol() });

      if (log != nullptr)
      {
        *log << "<li>";
        dump_lvalue_in_html(lvalue,ns,*log);
        *log << " &rarr; ";
        dump_svalue_in_html(map.at(lvalue),*log);
        *log << "</li>\n";
      }
    }
    else
      if (log != nullptr)
      {
        *log << "<li>!! EXCLUDING !! : ";
        dump_lvalue_in_html(lvalue,ns,*log);
        *log << "</li>\n";
      }

  if (log != nullptr)
  {
    *log << "</ul>\n<p>Updated symbolic value at the entry location "
         << function.body.instructions.cbegin()->location_number
         << ":</p>\n";
    dump_lvalues_to_svalues_in_html(map,ns,*log);
  }
}


/**
 *
 */
typedef std::unordered_set<instruction_iterator_t,
                            detail::instruction_iterator_hasher>
        solver_work_set_t;


/**
 *
 */
void  initialise_workset(
    goto_functionst::goto_functiont const&  function,
    solver_work_set_t&  work_set
    )
{
  for (auto  it = function.body.instructions.cbegin();
       it != function.body.instructions.cend();
       ++it)
    work_set.insert(it);
}


void  __dbg_learn_goto_model_stuff(goto_modelt const&  model)
{
  //namespacet const  ns(instrumented_program.symbol_table);
  for (auto const&  id_sym : model.symbol_table.symbols)
    std::cout << "  " << id_sym.first << " -> " << id_sym.second.name << "\n";

}


void  build_summary_from_computed_domain(
    domain_ptrt const  domain,
    map_from_lvalues_to_svaluest&  input,
    map_from_lvalues_to_svaluest&  output,
    goto_functionst::function_mapt::const_iterator const  fn_iter,
    namespacet const&  ns,
    std::ostream* const  log
    )
{
  if (log != nullptr)
    *log << "<h3>Building summary from the computed domain</h3>\n"
            "<p>Mapping of input to symbols (computed from the symbolic value "
            "at location "
         << fn_iter->second.body.instructions.cbegin()->location_number
         << "):</p>\n"
            "<ul>\n"
         ;

  map_from_lvalues_to_svaluest const&  start_svalue =
      domain->at(fn_iter->second.body.instructions.cbegin());
  for (auto  it = start_svalue.cbegin(); it != start_svalue.cend(); ++it)
    if (!is_pure_local(it->first,ns) &&
        !is_return_value_auxiliary(it->first,ns))
    {
      input.insert(*it);

      if (log != nullptr)
      {
        *log << "<li>";
        dump_lvalue_in_html(it->first,ns,*log);
        *log << " &rarr; ";
        dump_svalue_in_html(it->second,*log);
        *log << "</li>\n";
      }
    }
    else
      if (log != nullptr)
      {
        *log << "<li>!! EXCLUDING !! : ";
        dump_lvalue_in_html(it->first,ns,*log);
        *log << " &rarr; ";
        dump_svalue_in_html(it->second,*log);
        *log << "</li>\n";
      }

  if (log != nullptr)
    *log << "</ul>\n<p>The summary (computed from the symbolic value "
            "at location "
         << std::prev(fn_iter->second.body.instructions.cend())->location_number
         << "):</p>\n<ul>\n"
         ;

  map_from_lvalues_to_svaluest const&  end_svalue =
      domain->at(std::prev(fn_iter->second.body.instructions.cend()));
  for (auto  it = end_svalue.cbegin(); it != end_svalue.cend(); ++it)
    if (!is_pure_local(it->first,ns))
    {
      output.insert(*it);

      if (log != nullptr)
      {
        *log << "<li>";
        dump_lvalue_in_html(it->first,ns,*log);
        *log << " &rarr; ";
        dump_svalue_in_html(it->second,*log);
          *log << "</li>\n";
      }
    }
    else
      if (log != nullptr)
      {
        *log << "<li>!! EXCLUDING !! : ";
        dump_lvalue_in_html(it->first,ns,*log);
        *log << " &rarr; ";
        dump_svalue_in_html(it->second,*log);
        *log << "</li>\n";
      }

  if (log != nullptr)
    *log << "</ul>\n";
}

void  assign(
    map_from_lvalues_to_svaluest&  map,
    lvaluet const&  lvalue,
    svaluet const&  svalue
    )
{
  auto const  it = map.find(lvalue);
  if (it == map.end())
    map.insert({lvalue,svalue});
  else
    it->second = svalue;
}


}}}}

namespace sumfn { namespace taint {


svaluet::svaluet(
    expressiont const&  expression,
    bool  is_bottom,
    bool  is_top
    )
  : m_expression(expression)
  , m_is_bottom(is_bottom)
  , m_is_top(is_top)
{
  assert((m_is_bottom && m_is_top) == false);
}


bool  operator==(svaluet const&  a, svaluet const&  b)
{
  return a.is_top() == b.is_top() &&
         a.is_bottom() == b.is_bottom() &&
         a.expression() == b.expression()
         ;
}


bool  operator<(svaluet const&  a, svaluet const&  b)
{
  if (a.is_top() || b.is_bottom())
    return false;
  if (a.is_bottom() || b.is_top())
    return true;
  return std::includes(b.expression().cbegin(),b.expression().cend(),
                       a.expression().cbegin(),a.expression().cend());
}


svaluet  join(svaluet const&  a, svaluet const&  b)
{
  if (a.is_bottom())
    return b;
  if (b.is_bottom())
    return a;
  if (a.is_top())
    return a;
  if (b.is_top())
    return b;
  svaluet::expressiont  result_set = a.expression();
  result_set.insert(b.expression().cbegin(),b.expression().cend());
  return {result_set,false,false};
}


bool  operator==(
    map_from_lvalues_to_svaluest const&  a,
    map_from_lvalues_to_svaluest const&  b)
{
  auto  a_it = a.cbegin();
  auto  b_it = b.cbegin();
  for ( ;
       a_it != a.cend() && b_it != b.cend() &&
       a_it->first == b_it->first && a_it->second == b_it->second;
       ++a_it, ++b_it)
    ;
  return a_it == a.cend() && b_it == b.cend();
}


bool  operator<(
    map_from_lvalues_to_svaluest const&  a,
    map_from_lvalues_to_svaluest const&  b)
{
  if (b.empty())
    return false;
  for (auto  a_it = a.cbegin(); a_it != a.cend(); ++a_it)
  {
    auto const  b_it = b.find(a_it->first);
    if (b_it == b.cend())
      return false;
    if (!(a_it->second < b_it->second))
      return false;
  }
  return true;
}


map_from_lvalues_to_svaluest  transform(
    map_from_lvalues_to_svaluest const&  a,
    goto_programt::instructiont const&  I,
    namespacet const&  ns,
    std::ostream* const  log
    )
{
  map_from_lvalues_to_svaluest  result = a;
  switch(I.type)
  {
  case ASSIGN:
    {
      code_assignt const&  asgn = to_code_assign(I.code);

      if (log != nullptr)
      {
        *log << "<p>\nRecognised ASSIGN instruction. Left-hand-side "
                "l-value is { ";
        dump_lvalue_in_html(asgn.lhs(),ns,*log);
        *log << " }. Right-hand-side l-values are { ";
      }

      svaluet  rvalue = detail::make_bottom();
      {
        lvalues_sett  rhs;
        detail::collect_lvalues(asgn.rhs(),rhs);
        for (auto const&  lvalue : rhs)
        {
          auto const  it = a.find(lvalue);
          if (it != a.cend())
            rvalue = join(rvalue,it->second);

          if (log != nullptr)
          {
            dump_lvalue_in_html(lvalue,ns,*log);
            *log << ", ";
          }
        }
      }

      if (log != nullptr)
        *log << "}.</p>\n";

      detail::assign(result,asgn.lhs(),rvalue);
    }
    break;
  case DEAD:
    {
      code_deadt const&  dead = to_code_dead(I.code);

      if (log != nullptr)
        *log << "<p>\nRecognised DEAD instruction. Removing these l-values { ";

      lvalues_sett  lvalues;
      detail::collect_lvalues(dead.symbol(),lvalues);
      for (auto const&  lvalue : lvalues)
      {
        result.erase(lvalue);

        if (log != nullptr)
        {
          dump_lvalue_in_html(lvalue,ns,*log);
          *log << ", ";
        }
      }

      if (log != nullptr)
        *log << "}.</p>\n";
    }
    break;
  case NO_INSTRUCTION_TYPE:
    if (log != nullptr)
      *log << "<p>Recognised NO_INSTRUCTION_TYPE instruction. "
              "The transformation function is identity.</p>\n";
    break;
  case SKIP:
    if (log != nullptr)
      *log << "<p>Recognised SKIP instruction. "
              "The transformation function is identity.</p>\n";
    break;
  case END_FUNCTION:
    if (log != nullptr)
      *log << "<p>Recognised END_FUNCTION instruction. "
              "The transformation function is identity.</p>\n";
    break;
  case GOTO:
    if (log != nullptr)
      *log << "<p>Recognised GOTO instruction. "
              "The transformation function is identity.</p>\n";
    break;
  case RETURN:
  case OTHER:
  case DECL:
  case FUNCTION_CALL:
  case ASSUME:
  case ASSERT:
  case LOCATION:
  case THROW:
  case CATCH:
  case ATOMIC_BEGIN:
  case ATOMIC_END:
  case START_THREAD:
  case END_THREAD:
    if (log != nullptr)
      *log << "<p>!!! WARNING !!! : Unsupported instruction reached. "
              "So, we use identity as a transformation function.</p>\n";
    break;
    break;
  default:
    throw std::runtime_error("ERROR: In 'sumfn::taint::transform' - "
                             "Unknown instruction!");
    break;
  }
  return result;
}


map_from_lvalues_to_svaluest  join(
    map_from_lvalues_to_svaluest const&  a,
    map_from_lvalues_to_svaluest const&  b
    )
{
  map_from_lvalues_to_svaluest  result_dict = b;
  for (auto  a_it = a.cbegin(); a_it != a.cend(); ++a_it)
  {
    auto const  r_it = result_dict.find(a_it->first);
    if (r_it == result_dict.cend())
      result_dict.insert(*a_it);
    else
      r_it->second = join(a_it->second,r_it->second);
  }
  return map_from_lvalues_to_svaluest{ result_dict };
}


summaryt::summaryt(
    map_from_lvalues_to_svaluest const&  input,
    map_from_lvalues_to_svaluest const&  output,
    domain_ptrt const  domain
    )
  : m_input(input)
  , m_output(output)
  , m_domain(domain)
{
  assert(m_domain.operator bool());
}


std::string  summaryt::kind() const
{
  return "sumfn::taint::summarise_function";
}

std::string  summaryt::description() const noexcept
{
  return "Function summary of taint analysis of java web applications.";
}



void  summarise_all_functions(
    goto_modelt const&  instrumented_program,
    database_of_summariest&  summaries_to_compute,
    call_grapht const&  call_graph,
    std::ostream* const  log
    )
{
  //detail::__dbg_learn_goto_model_stuff(instrumented_program);
  for (auto const&  elem : instrumented_program.goto_functions.function_map)
    if (elem.second.body_available())
      summaries_to_compute.insert({
          as_string(elem.first),
          summarise_function(
              elem.first,
              instrumented_program,
              summaries_to_compute,
              log),
          });
}

summary_ptrt  summarise_function(
    irep_idt const&  function_id,
    goto_modelt const&  instrumented_program,
    database_of_summariest&  database,
    std::ostream* const  log
    )
{
  if (log != nullptr)
    *log << "<h2>Called sumfn::taint::summarise_function( "
         << to_html_text(as_string(function_id)) << " )</h2>\n"
         ;

  goto_functionst::function_mapt const&  functions =
      instrumented_program.goto_functions.function_map;
  auto const  fn_iter = functions.find(function_id);

  namespacet const  ns(instrumented_program.symbol_table);

  assert(fn_iter != functions.cend());
  assert(fn_iter->second.body_available());

  domain_ptrt  domain = std::make_shared<domaint>();
  detail::initialise_domain(fn_iter->second,ns,*domain,log);

  detail::solver_work_set_t  work_set;
  detail::initialise_workset(fn_iter->second,work_set);
  while (!work_set.empty())
  {
    instruction_iterator_t const  src_instr_it = *work_set.cbegin();
    work_set.erase(work_set.cbegin());

    map_from_lvalues_to_svaluest const&  src_value = domain->at(src_instr_it);

    goto_programt::const_targetst successors;
    fn_iter->second.body.get_successors(src_instr_it, successors);
    for(auto  succ_it = successors.begin();
        succ_it != successors.end();
        ++succ_it)
      if (*succ_it != fn_iter->second.body.instructions.cend())
      {
        instruction_iterator_t const  dst_instr_it = *succ_it;
        map_from_lvalues_to_svaluest&  dst_value = domain->at(dst_instr_it);
        map_from_lvalues_to_svaluest const  old_dst_value = dst_value;

        if (log != nullptr)
        {
          *log << "<h3>Processing transition: "
               << src_instr_it->location_number << " ---[ "
               ;
          dump_instruction_code_in_html(
              *src_instr_it,
              instrumented_program,
              *log
              );
          *log << " ]---> " << dst_instr_it->location_number << "</h3>\n"
               ;
          *log << "<p>Source value:</p>\n";
          dump_lvalues_to_svalues_in_html(src_value,ns,*log);
          *log << "<p>Old destination value:</p>\n";
          dump_lvalues_to_svalues_in_html(old_dst_value,ns,*log);
        }

        map_from_lvalues_to_svaluest const  transformed =
            transform(src_value,*src_instr_it,ns,log);
        dst_value = join(transformed,dst_value);

        if (log != nullptr)
        {
          *log << "<p>Transformed value:</p>\n";
          dump_lvalues_to_svalues_in_html(transformed,ns,*log);
          *log << "<p>Resulting destination value:</p>\n";
          dump_lvalues_to_svalues_in_html(dst_value,ns,*log);

//if (src_instr_it->location_number == 9 &&
//    dst_instr_it->location_number == 10)
//{
//  *log << "<p>COMPARE!!!</p>\n";
//  *log << "<p>old_dst_value:</p>\n";
//  dump_lvalues_to_svalues_in_html(old_dst_value,ns,*log);
//  *log << "<p>dst_value:</p>\n";
//  dump_lvalues_to_svalues_in_html(dst_value,ns,*log);
//  *log << "<p>dst_value <= old_dst_value:"
//       << (dst_value <= old_dst_value)
//       << "</p>\n";
//  *log << "<p>dst_value == old_dst_value:"
//       << (dst_value == old_dst_value)
//       << "</p>\n";
//  *log << "<p>dst_value < old_dst_value:"
//       << (dst_value < old_dst_value)
//       << "</p>\n";
//}
        }

        if (!(dst_value <= old_dst_value))
        {
          work_set.insert(dst_instr_it);

          if (log != nullptr)
            *log << "<p>Inserting instruction at location "
                 << dst_instr_it->location_number << " into 'work_set'.</p>\n"
                 ;
        }
      }
  }
  map_from_lvalues_to_svaluest  input;
  map_from_lvalues_to_svaluest  output;
  detail::build_summary_from_computed_domain(
        domain,
        input,
        output,
        fn_iter,
        ns,
        log
        );
  return std::make_shared<summaryt>(input,output,domain);
}


}}
