/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <set>
#include <sstream>

#include <util/std_types.h>
#include <util/std_code.h>
#include <util/std_expr.h>
#include <util/i2string.h>
#include <util/namespace.h>
#include <util/pointer_offset_size.h>
#include <util/prefix.h>
#include <util/expr_util.h>

#include "java_object_factory.h"
#include "java_types.h"

/*******************************************************************\

Function: gen_nondet_init

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

class gen_nondet_state {

  code_blockt& init_code;
  std::set<irep_idt> recursion_set;
  bool assume_non_null;
  int max_nondet_array_length;
  symbol_tablet& symbol_table;
  namespacet ns;

public:

  gen_nondet_state(code_blockt& ic, bool ann, int mnal, symbol_tablet& st) :
    init_code(ic),
    assume_non_null(ann),
    max_nondet_array_length(mnal),
    symbol_table(st),
    ns(st) {}

  exprt allocate_object(const exprt&, const typet&,
                        const source_locationt &,
			bool create_dynamic_objects);

  void gen_nondet_array_init(const exprt &expr, const source_locationt &);

  void gen_nondet_init(const exprt &expr,
		       bool is_sub,
		       irep_idt class_identifier,
                       const source_locationt &loc,
		       bool skip_classid,
		       bool create_dynamic_objects,
		       const typet *override_type = 0);


};

// Returns false if we can't figure out the size of allocate_type.
// allocate_type may differ from target_expr, e.g. for target_expr having
// type int* and allocate_type being an int[10].
exprt gen_nondet_state::allocate_object(
  const exprt& target_expr,
  const typet& allocate_type,
  const source_locationt &loc,
  bool create_dynamic_objects)
{
  const typet& allocate_type_resolved=ns.follow(allocate_type);
  const typet& target_type=ns.follow(target_expr.type().subtype());
  bool cast_needed=allocate_type_resolved!=target_type;
  if(!create_dynamic_objects)
  {
    symbolt &aux_symbol=new_tmp_symbol(symbol_table);
    aux_symbol.type=allocate_type;
    aux_symbol.is_static_lifetime=true;
      
    exprt object=aux_symbol.symbol_expr();
    exprt aoe=address_of_exprt(object);
    if(cast_needed)
      aoe=typecast_exprt(aoe, target_expr.type());
    code_assignt code(target_expr,aoe);
    code.add_source_location()=loc;
    init_code.copy_to_operands(code);
    return aoe;
  }
  else
  {
    // build size expression
    exprt object_size=size_of_expr(allocate_type, namespacet(symbol_table));

    if(allocate_type.id()!=ID_empty && !object_size.is_nil())
    {
      // malloc expression
      exprt malloc_expr = side_effect_exprt(ID_malloc);
      malloc_expr.copy_to_operands(object_size);
      typet result_type=pointer_typet(allocate_type);
      malloc_expr.type()=result_type;
      // Create a symbol for the malloc expression so we can initialise
      // without having to do it potentially through a double-deref, which
      // breaks the to-SSA phase.
      symbolt &malloc_sym=new_tmp_symbol(symbol_table,"malloc_site");
      malloc_sym.type=pointer_typet(allocate_type);
      code_assignt assign = code_assignt(malloc_sym.symbol_expr(), malloc_expr);
      code_assignt &malloc_assign = assign;
      malloc_assign.add_source_location() = loc;
      init_code.copy_to_operands(malloc_assign);
      malloc_expr=malloc_sym.symbol_expr();
      if(cast_needed)
        malloc_expr=typecast_exprt(malloc_expr,target_expr.type());
      code_assignt code(target_expr, malloc_expr);
      code.add_source_location() = loc;
      init_code.copy_to_operands(code);
      return malloc_sym.symbol_expr();
    }
    else
    {
      // make null
      null_pointer_exprt null_pointer_expr(to_pointer_type(target_expr.type()));
      code_assignt code(target_expr, null_pointer_expr);
      code.add_source_location() = loc;
      init_code.copy_to_operands(code);
      return exprt();
    }
  }
}

// Override type says to ignore the LHS' real type, and init it with the given
// type regardless. Used at the moment for reference arrays, which are implemented
// as void* arrays but should be init'd as their true type with appropriate casts.
void gen_nondet_state::gen_nondet_init(
  const exprt &expr,
  bool is_sub,
  irep_idt class_identifier,
  const source_locationt &loc,
  bool skip_classid,
  bool create_dynamic_objects,
  const typet *override_type)
{
  const typet &type=
    override_type ? ns.follow(*override_type) : ns.follow(expr.type());
  
  if(type.id()==ID_pointer)
  {
    // dereferenced type
    const pointer_typet &pointer_type=to_pointer_type(type);
    const typet &subtype=ns.follow(pointer_type.subtype());

    if(subtype.id()==ID_struct)
    {
      const struct_typet &struct_type=to_struct_type(subtype);
      const irep_idt struct_tag=struct_type.get_tag();

      if(recursion_set.find(struct_tag)!=recursion_set.end())
      {
        // make null
        null_pointer_exprt null_pointer_expr(pointer_type);
        code_assignt code(expr, null_pointer_expr);
        code.add_source_location() = loc;
        init_code.copy_to_operands(code);

        return;
      }
    }

    code_labelt set_null_label;
    code_labelt init_done_label;

    static unsigned long synthetic_constructor_count=0;

    if(!assume_non_null)
    {
      auto returns_null_sym=
          new_tmp_symbol(symbol_table,"opaque_returns_null");
      returns_null_sym.type=c_bool_typet(1);
      auto returns_null=returns_null_sym.symbol_expr();
      auto assign_returns_null=
          code_assignt(returns_null,get_nondet_bool(returns_null_sym.type));
      assign_returns_null.add_source_location() = loc;
      init_code.move_to_operands(assign_returns_null);

      auto set_null_inst=code_assignt(
          expr,null_pointer_exprt(pointer_type));
      set_null_inst.add_source_location() = loc;

      std::ostringstream fresh_label_oss;
      fresh_label_oss<<"post_synthetic_malloc_"
          <<(++synthetic_constructor_count);
      std::string fresh_label=fresh_label_oss.str();
      set_null_label=code_labelt(fresh_label,set_null_inst);

      init_done_label=code_labelt(fresh_label + "_init_done",code_skipt());

      code_ifthenelset null_check;
      null_check.cond()=notequal_exprt(
          returns_null,constant_exprt("0",returns_null_sym.type));
      null_check.then_case()=code_gotot(fresh_label);
      init_code.move_to_operands(null_check);
    }
    
    if(subtype.id()==ID_struct &&
       has_prefix(id2string(to_struct_type(subtype).get_tag()), "java::array["))
    {
      gen_nondet_array_init(expr, loc);
    }
    else {
      exprt allocated=allocate_object(expr,subtype,loc,create_dynamic_objects);
      {
        exprt init_expr;
        if(allocated.id()==ID_address_of)
          init_expr=allocated.op0();
        else
          init_expr=dereference_exprt(allocated,allocated.type().subtype());
        gen_nondet_init(init_expr,false,"",loc,false,create_dynamic_objects);
      }
    }

    if(!assume_non_null)
    {
      init_code.copy_to_operands(code_gotot(init_done_label.get_label()));
      init_code.move_to_operands(set_null_label);
      init_code.move_to_operands(init_done_label);
    }
    
  }
  else if(type.id()==ID_struct)
  {
    typedef struct_typet::componentst componentst;

    const struct_typet &struct_type=to_struct_type(type);
    const irep_idt struct_tag=struct_type.get_tag();

    const componentst &components=struct_type.components();

    if(!is_sub)
      class_identifier=struct_tag;
    
    recursion_set.insert(struct_tag);
    assert(!recursion_set.empty());

    for(const auto & component : components)
    {
      const typet &component_type=component.type();
      irep_idt name=component.get_name();

      member_exprt me(expr, name, component_type);

      if(name=="@class_identifier")
      {
	if(skip_classid)
	  continue;

        irep_idt qualified_clsid="java::"+as_string(class_identifier);
	constant_exprt ci(qualified_clsid,string_typet());

	code_assignt code(me, ci);
        code.add_source_location() = loc;
	init_code.copy_to_operands(code);
      }
      else if(name=="@lock")
      {
        code_assignt code(me, gen_zero(me.type()));
        code.add_source_location() = loc;
        init_code.copy_to_operands(code);
      }
      else
      {
        assert(!name.empty());

        bool _is_sub = name[0]=='@';
#if 0	
        irep_idt _class_identifier=
          _is_sub?(class_identifier.empty()?struct_tag:class_identifier):"";
#endif

        gen_nondet_init(me, _is_sub, class_identifier, loc, false, create_dynamic_objects);
      }
    }
    recursion_set.erase(struct_tag);
  }
  else
  {
    side_effect_expr_nondett se=side_effect_expr_nondett(type);

    code_assignt code(expr, se);
    code.add_source_location() = loc;
    init_code.copy_to_operands(code);
  }
}

// Borrowed from java_bytecode_convert.cpp -- todo find a sensible place to factor this.
static constant_exprt as_number(const mp_integer value, const typet &type)
{
  const unsigned int java_int_width(type.get_unsigned_int(ID_width));
  const std::string significant_bits(integer2string(value, 2));
  std::string binary_width(java_int_width - significant_bits.length(), '0');
  return constant_exprt(binary_width += significant_bits, type);
}  

void gen_nondet_state::gen_nondet_array_init(const exprt &expr, const source_locationt &loc)
{
  assert(expr.type().id()==ID_pointer);
  const typet &type=ns.follow(expr.type().subtype());
  const struct_typet &struct_type=to_struct_type(type);
  assert(expr.type().subtype().id() == ID_symbol);
  const typet &element_type=static_cast<const typet &>(expr.type().subtype().find(ID_C_element_type));
  
  auto max_length_expr=as_number(max_nondet_array_length,java_int_type());

  typet allocate_type;
  symbolt &length_sym=new_tmp_symbol(symbol_table,"nondet_array_length");
  length_sym.type=java_int_type();
  const auto &length_sym_expr=length_sym.symbol_expr();

  // Initialise array with some undetermined length:
  gen_nondet_init(length_sym_expr,false,irep_idt(),loc,false,false);

  // Insert assumptions to bound its length:
  binary_relation_exprt assume1(length_sym_expr,ID_ge,
                                as_number(0, java_int_type()));
  binary_relation_exprt assume2(length_sym_expr,ID_le,
                                max_length_expr);
  code_assumet assume_inst1(assume1);
  code_assumet assume_inst2(assume2);
  init_code.move_to_operands(assume_inst1);
  init_code.move_to_operands(assume_inst2);

  side_effect_exprt java_new_array(ID_java_new_array,expr.type());
  java_new_array.copy_to_operands(length_sym_expr);
  java_new_array.set("skip_initialise",true);
  java_new_array.type().subtype().set(ID_C_element_type,element_type);
  codet assign = code_assignt(expr,java_new_array);
  assign.add_source_location()=loc;
  init_code.copy_to_operands(assign);

  exprt init_array_expr=member_exprt(dereference_exprt(expr, expr.type().subtype()),
                                     "data", struct_type.components()[2].type());
  if(init_array_expr.type()!=pointer_typet(element_type))
    init_array_expr=typecast_exprt(init_array_expr,pointer_typet(element_type));

  // Interpose a new symbol, as the goto-symex stage can't handle array indexing via a cast.
  symbolt &array_init_symbol=new_tmp_symbol(symbol_table,"array_data_init");
  array_init_symbol.type=init_array_expr.type();
  const auto &array_init_symexpr=array_init_symbol.symbol_expr();
  codet data_assign = code_assignt(array_init_symexpr,init_array_expr);
  data_assign.add_source_location() = loc;
  init_code.copy_to_operands(data_assign);

  // Emit init loop for(array_init_iter=0; array_init_iter!=array.length; ++array_init_iter)
  //                  init(array[array_init_iter]);
  symbolt &counter=new_tmp_symbol(symbol_table,"array_init_iter");
  counter.type=length_sym_expr.type();
  exprt counter_expr=counter.symbol_expr();

  init_code.copy_to_operands(
    code_assignt(counter_expr,as_number(0, java_int_type())));

  std::string head_name=as_string(counter.base_name)+"_header";
  code_labelt init_head_label(head_name,code_skipt());
  code_gotot goto_head(head_name);

  init_code.move_to_operands(init_head_label);
  
  std::string done_name=as_string(counter.base_name)+"_done";
  code_labelt init_done_label(done_name,code_skipt());
  code_gotot goto_done(done_name);

  code_ifthenelset done_test;
  done_test.cond()=equal_exprt(counter_expr,length_sym_expr);
  done_test.then_case()=goto_done;

  init_code.move_to_operands(done_test);

  // Add a redundant if(counter == max_length) break that is easier for the unwinder to understand.
  code_ifthenelset max_test;
  max_test.cond()=equal_exprt(counter_expr,max_length_expr);
  max_test.then_case()=goto_done;

  init_code.move_to_operands(max_test);

  exprt arraycellref=dereference_exprt(
    plus_exprt(array_init_symexpr,counter_expr,array_init_symexpr.type()),
    array_init_symexpr.type().subtype());

  gen_nondet_init(arraycellref,false,irep_idt(),loc,false,true,
		  /*override_type=*/&element_type);

  code_assignt incr(counter_expr,
                    plus_exprt(counter_expr,
                               as_number(1, java_int_type())));

  init_code.move_to_operands(incr);
  init_code.move_to_operands(goto_head);
  init_code.move_to_operands(init_done_label);
  
}


/*******************************************************************\

Function: gen_nondet_init

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void gen_nondet_init(
  const exprt &expr,
  code_blockt &init_code,
  symbol_tablet &symbol_table,
  const source_locationt &loc,
  bool skip_classid,
  bool create_dynamic_objects,
  bool assume_non_null,
  int max_nondet_array_length)
{
  gen_nondet_state state(init_code,assume_non_null,max_nondet_array_length,
			 symbol_table);
  state.gen_nondet_init(expr,false,"",loc,skip_classid,create_dynamic_objects);
}

/*******************************************************************\

Function: new_tmp_symbol

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

symbolt &new_tmp_symbol(symbol_tablet &symbol_table, const std::string& prefix)
{
  static int temporary_counter=0;

  auxiliary_symbolt new_symbol;
  symbolt *symbol_ptr;

  do
  {
    new_symbol.name=prefix+"$"+i2string(++temporary_counter);
    new_symbol.base_name=new_symbol.name;
    new_symbol.mode=ID_java;
  } while(symbol_table.move(new_symbol, symbol_ptr));

  return *symbol_ptr;
}

/*******************************************************************\

Function: get_nondet_bool

  Inputs: Desired type (C_bool or plain bool)

 Outputs: nondet expr of that type

 Purpose:

\*******************************************************************/

exprt get_nondet_bool(const typet& type) {
  // We force this to 0 and 1 and won't consider
  // other values.
  return typecast_exprt(side_effect_expr_nondett(bool_typet()), type);
}

/*******************************************************************\

Function: object_factory

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

exprt object_factory(
  const typet &type,
  code_blockt &init_code,
  bool allow_null,
  symbol_tablet &symbol_table,
  int max_nondet_array_length,
  const source_locationt &loc)
{
  if(type.id()==ID_pointer)
  {
    symbolt &aux_symbol=new_tmp_symbol(symbol_table);
    aux_symbol.type=type;
    aux_symbol.is_static_lifetime=true;

    exprt object=aux_symbol.symbol_expr();
    
    const namespacet ns(symbol_table);
    gen_nondet_init(object, init_code, symbol_table, loc,
                    false, false, !allow_null,
		    max_nondet_array_length);

    return object;
  }
  else if(type.id()==ID_c_bool)
  {
    // We force this to 0 and 1 and won't consider
    // other values.
    return get_nondet_bool(type);
  }
  else
    return side_effect_expr_nondett(type);
}

