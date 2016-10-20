
#include "local_value_set_analysis.h"

#include <util/prefix.h>
#include <util/json_irep.h>

void local_value_set_analysist::initialize(const goto_programt& fun)
{
  summarydb.set_message_handler(get_message_handler());
  value_set_analysist::initialize(fun);
  
  if(fun.instructions.size()!=0)
  {
    auto& initial_state=(*this)[fun.instructions.begin()].value_set;
  
    // Now insert fresh symbols for each parameter, indicating an unknown external points-to set.
    for(const auto& param : function_type.parameters())
    {
      if(param.type().id()==ID_pointer)
      {
        const auto& param_name=param.get_identifier();
        value_sett::entryt param_entry_blank(id2string(param_name),"");
        auto& param_entry=initial_state.get_entry(param_entry_blank, param.type().subtype(), ns);
        irep_idt initial_content;
        switch(mode) {
        case LOCAL_VALUE_SET_ANALYSIS_SINGLE_EXTERNAL_SET:
          initial_content="external_objects";
          break;
        case LOCAL_VALUE_SET_ANALYSIS_EXTERNAL_SET_PER_ACCESS_PATH:
          initial_content=param_name;
          break;
        default:
          throw "Missing mode in switch";          
        }
        external_value_set_exprt param_var(
          param.type().subtype(),constant_exprt(initial_content,string_typet()),mode,false);
        initial_state.insert(param_entry.object_map,param_var);
      }
    }

  }
}

static const std::vector<value_sett::entryt*>& get_all_field_value_sets(
  const std::string& fieldname,
  value_sett& state,
  std::map<std::string, std::vector<value_sett::entryt*> >& suffix_to_entries)
{
  auto insert_result=suffix_to_entries.insert(
    std::make_pair(fieldname, std::vector<value_sett::entryt*>()));
  auto& entrylist=insert_result.first->second;  
  if(insert_result.second)
  {
    // Find all value sets we're currently aware of that may be affected by
    // a write to the given field:
    for(auto& entry : state.values)
    {
      if(entry.second.suffix==fieldname)
        entrylist.push_back(&entry.second);
    }
  }
  return entrylist;
}

void local_value_set_analysist::transform_function_stub_single_external_set(
  const irep_idt& fname, statet& state, locationt l_call, locationt l_return)
{
  auto& valuesets=static_cast<value_set_domaint&>(state).value_set;
  std::map<std::string, std::vector<value_sett::entryt*> > suffix_to_entries;
  const auto& call_summary=static_cast<const lvsaa_single_external_set_summaryt&>(
    *(summarydb[id2string(fname)]));
  for(const auto& assignment : call_summary.field_assignments)
  {
    auto& lhs_entries=get_all_field_value_sets(assignment.first,valuesets,suffix_to_entries);
    if(assignment.second.id()=="external-value-set")
    {
      const auto& rhs_entries=get_all_field_value_sets(
        id2string(to_external_value_set(assignment.second).access_path_back().label()),
        valuesets,
        suffix_to_entries);
      value_sett static_valset;
      for(auto& lhs_entry : lhs_entries)
        for(const auto& rhs_entry : rhs_entries)
          valuesets.make_union(lhs_entry->object_map,rhs_entry->object_map);
    }
    else
    {
      for(auto& lhs_entry : lhs_entries)
        valuesets.insert(lhs_entry->object_map,assignment.second);
    }
  }
}

void local_value_set_analysist::transform_function_stub(
  const irep_idt& fname, statet& state, locationt l_call, locationt l_return)
{
  // Execute a summary description for function fname.
  if(!summarydb.load(id2string(fname)))
    return;
  switch(mode)
  {
  case LOCAL_VALUE_SET_ANALYSIS_SINGLE_EXTERNAL_SET:
    transform_function_stub_single_external_set(fname,state,l_call,l_return);
    break;
  default:
    throw "Summaries not implemented";
  }
}

void local_value_set_analysist::save_summary(const goto_programt& goto_program)
{
  assert(goto_program.instructions.size()!=0);
  locationt last_loc=std::prev(goto_program.instructions.end());
  const auto& final_state=static_cast<const value_set_domaint&>(get_state(last_loc));
  auto summaryptr=std::make_shared<lvsaa_single_external_set_summaryt>();
  summaryptr->from_final_state(final_state.value_set);
  summarydb.insert(std::make_pair(function_name,summaryptr));
  summarydb.save(function_name);
  summarydb.save_index();
}

void lvsaa_single_external_set_summaryt::from_final_state(const value_sett& final_state)
{
  // Just save a list of fields that may be overwritten by this function, and the values
  // they may be assigned.
  for(const auto& entry : final_state.values)
  {
    const std::string prefix="external_objects.";
    const std::string entryname=id2string(entry.first);
    if(has_prefix(entryname,prefix))
    {
      std::string fieldname=entryname.substr(prefix.length());
      const auto& pointsto=entry.second.object_map.read();
      for(const auto& pointsto_number : pointsto)
      {
        const auto& pointsto_expr=final_state.object_numbering[pointsto_number.first];
        field_assignments.push_back(std::make_pair(fieldname,pointsto_expr));
      }
    }
  }
}

json_objectt lvsaa_single_external_set_summaryt::to_json() const
{
  json_arrayt assigns;
  for(const auto& entry : field_assignments)
  {
    json_objectt assign;
    assign["lhs"]=json_stringt(entry.first);
    assign["rhs"]=irep_to_json(entry.second);
    assigns.push_back(assign);
  }
  json_objectt ret;
  ret["assigns"]=std::move(assigns);
  return ret;
}

void lvsaa_single_external_set_summaryt::from_json(const json_objectt& json)
{
  assert(json.is_object());
  assert(json.object.at("assigns").is_array());
  for(const auto& entry : json.object.at("assigns").array)
  {
    assert(entry.object.at("lhs").is_string());
    irept rhs_irep=irep_from_json(entry.object.at("rhs"));
    field_assignments.push_back(std::make_pair(entry.object.at("lhs").value,
                                               static_cast<const exprt&>(rhs_irep)));
  }

}