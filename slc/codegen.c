#include <codegen.h>
#include <parser.h>
#include <vector.h>
#include <strhash.h>
#include "optimizer.h"
#include "services.h"

const char* UNKNOWN_TYPE   = "Unknown type";
const char* UNKNOWN_STRUCT = "Unknown struct";
const char* INVALID_TYPE   = "Invalid type";
const char* OUT_OF_BOUNDS = "Out of bounds";
const char* UNKNOWN_FUNCTION = "Unknown function";
const char* UNKNOWN_VAR = "Unknown variable";
const char* INVALID_SIZE = "Invalid Size";
const char* MISSING_NODE = "Missing node";
const char* INVALID_LOCATION = "Invalid location";
const char* UNSUPPORTED = "Unsupported";
const char* EXPECT_IMMED = "Expecting immediate";
const char* INVALID_OPCODE = "Invalid opcode";

#define ERROR_RET(line, msg) { error=1; error_exit(line,msg,1); }
#define ASSERT(x)

#ifndef DEV
void exit(int rc) { (void)rc; }
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dev.h>
#endif

#define POINTER_SIZE sizeof(word)

void error_exit(word line, const char* msg, int rc)
{
#ifdef DEV
	printf("%s\nError in line %d\n",msg, line);
#endif
	exit(rc);
}

typedef enum data_location_
{
	IMMEDIATE = 1,	// Value is known in compile time
	A=2,			// Value is a byte stored in A
	HL=3,			// Value is a word (not address), stored in HL
	STACK=4,		// Value is an address (array / struct) pushed to stack
	GLOBAL=5		// Value is an address stored in HL
} DataLocation;

typedef struct data_type_
{
	BaseType	base_type;
	byte		local;		// 0 for absolute, 1 for relative to IX
} DataType;

typedef struct term_
{
	DataLocation	location;
	DataType		type;
	word			immediate;
} Term;

typedef struct field_
{
	BaseType	type;
	word		name;
	word		length;
} Field;

typedef struct struct_
{
	word	name;
	Vector* fields;
} Struct;

typedef struct variable_
{
	// For global variables, address is absolute
	// For local variables, address is relative to stack frame pointer at entry
	word		name;
	word		address;
	word		size;
	DataType	type;
} Variable;

typedef struct address_
{
	word		name;
	word		address;
} Address;

typedef struct function_prototype_
{
	word		name;
	BaseType	return_type;
	Vector* parameters;
} FunctionPrototype;

static byte error=0;
static Vector* structs;
static Vector* variables;
static Vector* knowns;
static Vector* unknowns;
static Vector* function_addresses;
static Vector* function_prototypes;
static parse_node_func parse_node;
static file_write_func raw_write=0;
static word write_offset=0;
static word function_end=0;
static Node* function_node=0;

#ifdef DEV
#include <stdio.h>
FILE* line_offsets_file = 0;
FILE* abs_addr_file = 0;
void close_line_offsets()
{
	if (line_offsets_file)
		fclose(line_offsets_file);
}
void write_offset_line(word line)
{
	if (!line_offsets_file)
		line_offsets_file = fopen("line_offsets.log", "w");
	fprintf(line_offsets_file,"%hx %hx\n",line,0x1000+write_offset);
}
void close_abs_addr()
{
	if (abs_addr_file)
		fclose(abs_addr_file);
}
void save_unknown_address(word addr)
{
	if (!abs_addr_file)
		abs_addr_file=fopen("abs_addr.bin","wb");
	fwrite(&addr,2,1,abs_addr_file);
}
#else
void write_offset_line(word line) {}
void save_unknown_address(word addr) {}
void close_abs_addr() {}
void close_line_offsets() {}
#endif


word get_known_address(word line, word name)
{
	word i=0,n=vector_size(knowns);
	for (; i < n; ++i)
	{
		Address* a = (Address*)vector_access(knowns,i);
		if (a->name==name)
			return a->address + 0x1000; // Add OS size offset
	}
	char buf[32];
	sh_text(buf,name);
#ifdef DEV
	strcat(buf," missing");
	//printf("Missing symbol %s\n",buf);
#endif
	ERROR_RET(line,buf);
	return 0;
}

void add_known_address(word name, word addr)
{
	Address known = { name,addr };
	vector_push(knowns, &known);
}

// Add an unknown.  Later when the location of 'name' is known, 
// its value should be written to 'addr'
void add_unknown_address(word name, word addr)
{
	Address unknown = { name,addr };
	vector_push(unknowns, &unknown);
}


void write(const byte* data, word len)
{
	write_offset += raw_write(write_offset, data, len);
}

#define WRITE(x) write(x,sizeof(x))

void ld_hl_immed(word address)
{
	byte cmd[] = { 0x21, address & 255, address >> 8 };
	WRITE(cmd);
}

void write_byte(byte b) { write(&b, 1); }
#define MULTI_BYTE_CMD(name) write(name##_cmd,sizeof(name##_cmd))

#define add_c	write_byte(0x81)
#define sub_c	write_byte(0x91)
#define and_c	write_byte(0xA1)
#define or_c	write_byte(0xB1)
#define xor_c	write_byte(0xA9)
const byte lsh_hl_cmd[] = { 0xCB, 0x25, 0xCB, 0x14 };
#define lsh_hl MULTI_BYTE_CMD(lsh_hl)

#define push_af		write_byte(0xF5)
#define push_hl		write_byte(0xE5)
const byte push_ix_cmd[] = { 0xDD,0xE5 };
#define push_ix		WRITE(push_ix_cmd)
#define push_bc		write_byte(0xC5)
#define pop_af		write_byte(0xF1)
#define pop_hl		write_byte(0xE1)
#define pop_bc		write_byte(0xC1)
#define pop_de		write_byte(0xD1)

#define ld_a_mem_hl	write_byte(0x7E)
#define ld_c_mem_hl write_byte(0x4E)
#define ld_b_mem_hl write_byte(0x46)
#define add_hl_bc	write_byte(0x09)
#define add_hl_de	write_byte(0x19)
#define cp_c		write_byte(0xB9)

#define ld_mem_hl_a	write_byte(0x77)
#define sub_a		write_byte(0x97)
#define set_bc_hl	push_hl; pop_bc
#define ld_mem_hl_b write_byte(0x70)
#define ld_mem_hl_c write_byte(0x71)
#define inc_hl		write_byte(0x23)
#define inc_sp		write_byte(0x33)
void ld_a_mem_ix(byte offset) { const byte cmd[] = {0xDD, 0x7E, offset}; WRITE(cmd); }
void ld_h_mem_ix(byte offset) { const byte cmd[] = { 0xDD, 0x66, offset }; WRITE(cmd); }
void ld_l_mem_ix(byte offset) { const byte cmd[] = { 0xDD, 0x6E, offset }; WRITE(cmd); }
void ld_a_mem_immed(word addr) { const byte cmd[] = { 0x3A, (addr & 0xFF), (addr >> 8) }; WRITE(cmd); }
void ld_hl_mem_immed(word addr) { const byte cmd[] = { 0x2A, (addr & 0xFF), (addr >> 8) }; WRITE(cmd); }

const byte sub_hl_bc_cmd[] = { 0xBF, 0xED, 0x42 };  //  Clear-carry,  SBC HL,BC
#define sub_hl_bc MULTI_BYTE_CMD(sub_hl_bc)

const byte ld_bc_mem_hl_cmd[] = { 0x4E, 0x23, 0x46 }; // LD C,(HL)   INC HL    LD B,(HL)
#define ld_bc_mem_hl MULTI_BYTE_CMD(ld_bc_mem_hl)
const byte set_hl_bc_cmd[] = { 0xC5, 0xE1 }; // PUSH BC    POP HL
#define set_hl_bc MULTI_BYTE_CMD(set_hl_bc)
const byte set_hl_a_cmd[] = { 0x6F, 0x26, 0x00 }; // LD L,A    LD H,#0
#define set_hl_a MULTI_BYTE_CMD(set_hl_a)

#define set_e_a write_byte(0x5F)
#define set_b_c write_byte(0x41)
#define set_c_a write_byte(0x4F)
#define set_l_a write_byte(0x6F)
#define set_h_a write_byte(0x67)
#define set_a_l write_byte(0x7D)
const byte set_de_hl_cmd[] = { 0xE5, 0xD1 };
#define set_de_hl MULTI_BYTE_CMD(set_de_hl)

void set_a_immed(byte b) { byte cmd[] = { 0x3E, b }; WRITE(cmd); }
void set_h_immed(byte b) { byte cmd[] = { 0x26, b }; WRITE(cmd); }
void set_d_immed(byte b) { byte cmd[] = { 0x16, b }; WRITE(cmd); }
void set_b_immed(byte b) { byte cmd[] = { 0x06, b }; WRITE(cmd); }
void set_bc_immed(word w) { byte cmd[] = { 0x01, (w & 0xFF), (w >> 8) }; WRITE(cmd); }
void set_de_immed(word w) { byte cmd[] = { 0x11, (w & 0xFF), (w >> 8) }; WRITE(cmd); }
void set_hl_immed(word w) { byte cmd[] = { 0x21, (w & 0xFF), (w >> 8) }; WRITE(cmd); }


void generate_statement(Node* statement);
void generate_block(Node* block);
void generate_call(Node* node, Term* res);


byte is_binary_operator(byte type)
{
	switch (type)
	{
	case PLUS:
	case MINUS:
	case LSH:
	case RSH:
	case AMP:
	case PIPE:
	case CARET:
		return 1;
	}
	return 0;
}

void print_name(word id);
word struct_size(word line, word name);
FunctionPrototype* find_prototype(word name);

void set_a_res(word line, Term* res)
{
	switch (res->location)
	{
	case IMMEDIATE: set_a_immed(res->immediate); break;
	case A: break;
	case HL: set_a_l; break;
	default:
		ERROR_RET(line, INVALID_LOCATION);
	}
}

void set_hl_res(word line, Term* res)
{
	switch (res->location)
	{
	case IMMEDIATE:
		set_hl_immed(res->immediate);
		break;
	case HL: break;
	case A:	 set_hl_a; break;
	case STACK: pop_hl; break;
	default: ERROR_RET(line,INVALID_LOCATION);
	}
}

word type_size(word line, BaseType* base_type)
{
	if (base_type->sub_type == STRUCT) return struct_size(line, base_type->type_name);
	ASSERT(base_type->sub_type == PRIMITIVE);
	switch (base_type->type_name)
	{
	case SBYTE:
	case BYTE: return 1;
	case SWORD:
	case WORD: return 2;
	default:
		ERROR_RET(line,UNKNOWN_TYPE);
	}
	return 0;
}

word calculate_struct_size(word line, Struct* s)
{
	word n=vector_size(s->fields);
	word sum = 0;
	for (word i = 0; i < n; ++i)
	{
		Field* field = (Field*)vector_access(s->fields, i);
		sum+=field->length * type_size(line, &field->type);
	}
	return sum;
}

Struct* find_struct(word line, word name)
{
	word n = vector_size(structs);
	for (word i = 0; i < n; ++i)
	{
		Struct* s = (Struct*)vector_access(structs, i);
		if (s->name == name) return s;
	}
	ERROR_RET(line,UNKNOWN_STRUCT);
	return 0;
}

word struct_size(word line, word name)
{
	return calculate_struct_size(line, find_struct(line, name));
}

// Given a struct name and field name, calculate the relative term.
// If successful, returns an immediate term relative to the start of the struct
void struct_field_offset(word line, word struct_name, word field_name, Term* res, word* length)
{
	res->location = IMMEDIATE;
	res->immediate = 0;
	Struct* s=find_struct(line, struct_name);
	word offset = 0;
	word n=vector_size(s->fields);
	for (word i = 0; i < n; ++i)
	{
		Field* field = (Field*)vector_access(s->fields, i);
		if (field->name == field_name)
		{
			res->immediate=offset;
			res->type.base_type = field->type;
			if (length && field->type.type == ARRAY)
				*length = field->length;
			break;
		}
		offset += field->length * type_size(line, &field->type);
	}
}

word var_size(Node* node)
{
	if (node->data_type.type == VAR)
		return type_size(node->line, &node->data_type);
	else
	if (node->data_type.type == ARRAY)
	{
		if (node->parameters)
			return node->parameters->name * type_size(node->line, &node->data_type);
		//return POINTER_SIZE;
		return 0; // Pointer
	}
	else ERROR_RET(node->line,INVALID_TYPE);
	return 0;
}

word local_var_size(Node* node)
{
	if (!node) return 0;
	if (node->type == VAR)
		return var_size(node);
	word sum = 0;
	Node* child = node->child;
	while (child)
	{
		if (child->type == VAR)
			sum += var_size(child);
		if (child->type==WHILE || child->type==IF)
			sum += local_var_size(child);
		child = child->sibling;
	}
	return sum;
}

word calculate_parameters(Node* param, word offset)
{
	if (param)
	{
		// Use recursion to invert order (first parameter has highest offset)
		if (param->sibling)
			offset = calculate_parameters(param->sibling, offset);
		Variable var;
		offset += 2;
		var.name = param->name;
		var.address = offset;
		var.size = 2;
		var.type.base_type = param->data_type;
		var.type.local = 1;
		vector_push(variables, &var);
	}
	return offset;
}

void scan_parameters(Node* fun)
{
	if (fun)
		calculate_parameters(fun->parameters, 2);
}

// Accepts a block node (root / fun / while / if) and a variables vector.
// Adds local variables to the vector and returns the total size of the variables
word scan_variables(Node* node, word offset, byte local)
{
	if (!node) return 0;
	Variable var;
	word sum = 0;
	Node* child = node->child;
	if (!local) offset = 0x1003; // start of global vars
	while (child)
	{
		if (child->type == VAR)
		{
			var.name = child->name;
			var.type.local = local;
			var.size = var_size(child);
			word effective_size = var.size;
			if (effective_size == 0)
				effective_size = POINTER_SIZE;
			var.type.base_type = child->data_type;
			if (local)
			{
				offset -= effective_size;
				var.address = offset;
			}
			else
			{
				var.address = offset;
				offset += effective_size;
			}
			vector_push(variables, &var);
			sum += effective_size;
		}
		else
		if (child->type == WHILE || child->type == IF || child->type == IFELSE || child->type==BLOCK)
			sum += scan_variables(child, offset, 1);
		child = child->sibling;
	}
	return sum;
}

// Generate   a=a<<c
void generate_shift_a_c(byte opcode)
{
	/*
	* generate code as:
	*   b=a					<--- Save 'A' not to be lost in comparison
	*	if c==0 return
	*   a=b
	*			b=c
	* loop:		a<<=1   (or right shift depending on opcode)
	*			b=b-1
	*			if b!=0 goto loop
	*/
	//                   b=a   a-=a  a-c   a=b   <jz to end> b=c     shift a     jnz loop
	const byte cmd[] = { 0x47, 0x97, 0xB9, 0x78, 0x28, 0x05, 0x41, 0xCB, opcode, 0x10, 0xFC };
	WRITE(cmd);
}

void generate_rsh_hl_c()
{
	/*
	* generate code as:
	*	if c==0 return
	*			b=c
	* loop:		srl h
	*			rr l
	*			b=b-1
	*			if b!=0 goto loop
	*/
	//                   a-=a  a-c   <jz to end> b=c    shift h       rr l     jnz loop
	const byte cmd[] = { 0x97, 0xB9, 0x28, 0x07, 0x41, 0xCB, 0x3C, 0xCB, 0x1D, 0x10, 0xFA };
	WRITE(cmd);
}

void generate_lsh_hl_c()
{
	/*
	* generate code as:
	*	if c==0 return
	*			b=c
	* loop:		srl h
	*			rr l
	*			b=b-1
	*			if b!=0 goto loop
	*/
	//                   a-=a  a-c   <jz to end> b=c    sla l         rl h     jnz loop
	const byte cmd[] = { 0x97, 0xB9, 0x28, 0x07, 0x41, 0xCB, 0x25, 0xCB, 0x14, 0x10, 0xFA };
	WRITE(cmd);
}

byte find_variable(word name, Variable* var)
{
	word n = vector_size(variables);
	for (word i = 0; i < n; ++i)
	{
		vector_get(variables, i, var);
		if (var->name == name) return 1;
	}
	return 0;
}

void set_prim_type(BaseType* t, byte type_name)
{
	t->type = VAR;
	t->sub_type = PRIMITIVE;
	t->type_name = type_name;
}

void get_node_address(Node* node, Term*, word* length);

void calculate_expression(Node* node, Term* res)
{
	if (node->type == NUMBER)
	{
		res->location = IMMEDIATE;
		res->immediate = node->name;
		res->type.local = 0;
	}
	else if (node->type == IDENT)
	{
		Variable var;
		if (find_variable(node->name, &var))
		{
			res->type = var.type;
			if (var.type.base_type.type == ARRAY || var.type.base_type.sub_type == STRUCT)
			{
				ld_hl_immed(var.address);
				if (var.type.local)
				{
					push_ix;
					pop_bc;
					add_hl_bc;
					if (var.address < 0x100) // function parameter
					{
						ld_bc_mem_hl;
						set_hl_bc;
					}
				}
				push_hl;
				res->location = STACK;
			}
			else
			{
				word size = type_size(node->line, &var.type.base_type);
				if (var.type.local)
				{
					word mask = (var.address & 0xFF80);

					if (mask == 0 || mask == 0xFF80) // 0 of FF80 for low offset
					{
						if (size == 1)
						{
							ld_a_mem_ix(var.address);
							res->location = A;
						}
						else
						{
							ld_l_mem_ix(var.address);
							ld_h_mem_ix(var.address + 1);
							res->location = HL;
						}
					}
					else
					{
						set_hl_immed(var.address);
						push_ix;
						pop_bc;
						add_hl_bc;
						if (size == 1)
						{
							ld_a_mem_hl;
							res->location = A;
						}
						else
						{
							ld_c_mem_hl;
							inc_hl;
							ld_b_mem_hl;
							set_hl_bc;
							res->location = HL;
						}
					}
				}
				else
				{
					if (size == 1)
					{
						ld_a_mem_immed(var.address);
						res->location = A;
					}
					else
					{
						ld_hl_mem_immed(var.address);
						res->location = HL;
					}
				}
			}
		}
		else ERROR_RET(node->line, UNKNOWN_VAR);
	}
	else if (node->type == DOT || node->type == INDEX)
	{
		word length=0;
		get_node_address(node, res, &length);
		pop_hl;
		if (res->type.local)
		{
			push_ix;
			pop_bc;
			add_hl_bc;
		}
		word size = type_size(node->line, &res->type.base_type);
		if (size == 1)
		{
			ld_a_mem_hl;
			res->location = A;
			res->type.local = 0;
		}
		else if (size == 2)
		{
			ld_bc_mem_hl;
			set_hl_bc;
			res->location = HL;
			res->type.local = 0;
		}
		else ERROR_RET(node->line,INVALID_SIZE);
	}
	else if (is_binary_operator(node->type))
	{
		if (!node->child || !node->child->sibling) ERROR_RET(node->line,MISSING_NODE);
		Term left,right;
		calculate_expression(node->child,&left);
		// Place the left value on the stack
		set_hl_res(node->line, &left);
		push_hl;
		calculate_expression(node->child->sibling,&right);
		word left_size = 1, right_size = 1;
		if (left.location!=IMMEDIATE) 
			left_size = type_size(node->line, &left.type.base_type);
		if (right.location != IMMEDIATE)
			right_size = type_size(node->line, &right.type.base_type);
		word max_size = (left_size > 1 || right_size > 1) ? 2 : 1;
		set_hl_res(node->line, &right);
		set_bc_hl;
		pop_hl;
		if (max_size > 1)
		{
			res->location = HL; set_prim_type(&res->type.base_type, WORD);
			switch (node->type)
			{
			case PLUS: add_hl_bc; break;
			case MINUS: sub_hl_bc; break;
			case LSH: generate_lsh_hl_c(); break;
			case RSH: generate_rsh_hl_c(); break;
			default: ERROR_RET(node->line,UNSUPPORTED);
			}
		}
		else
		{
			set_a_l;
			switch (node->type)
			{
			case PLUS: add_c; break;
			case MINUS: sub_c; break;
			case LSH: generate_shift_a_c(0x27); break;
			case RSH: generate_shift_a_c(0x3F); break;
			case AMP: and_c; break;
			case PIPE: or_c; break;
			case CARET: xor_c; break;
			default: ERROR_RET(node->line, UNSUPPORTED);
			}
			res->location = A;
			set_prim_type(&res->type.base_type, BYTE);
		}
	}
	else if (node->type == CALL)
	{
		generate_call(node, res);
		//res->location = A;
		//res->immediate = 0;
		//res->type.local = 0;
		//res->type.base_type.type=VAR;
		//res->type.base_type.sub_type=PRIMITIVE;
		//res->type.base_type.type_name=BYTE;
	}
	else if (node->type == LPAREN)
	{
		calculate_expression(node->child, res);
	} else ERROR_RET(node->line,UNSUPPORTED);
}

void shift_left_hl(byte bits)
{
	for (byte b = 0; b < bits; ++b)
		lsh_hl;
}

void multiply_hl(word line, word m)
{
#define SHIFT_CASE(x) case (1<<x): shift_left_hl(x); break
	switch (m)
	{
		SHIFT_CASE(1);
		SHIFT_CASE(2);
		SHIFT_CASE(3);
		SHIFT_CASE(4);
		SHIFT_CASE(5);
		SHIFT_CASE(6);
		SHIFT_CASE(7);
		SHIFT_CASE(8);
	default:
		set_bc_hl;
		set_de_immed(m);
		word addr = get_known_address(line, sh_get("mult_bc_de"));
		const byte cmd[] = { 0xCD, (addr & 0xFF), (addr >> 8) };
		WRITE(cmd);
	}
#undef SHIFT_CASE
}

// Input:  node of address to evaluate
// Outputs:
//		Term - Location on STACK
void get_node_address(Node* node, Term* res, word* length)
{
	res->location = STACK;
	res->immediate = 0;
	if (node->type == IDENT)
	{
		Variable var;
		if (find_variable(node->name, &var))
		{
			res->type = var.type;
			if (length && var.type.base_type.type == ARRAY && var.size>0)
				*length = var.size;
			if (var.type.local && var.address<0x100 &&
				(var.type.base_type.type == ARRAY || var.type.base_type.sub_type == STRUCT))
			{
				// variable is a local parameter pointer.  Load its actual address
				ld_l_mem_ix(var.address);
				ld_h_mem_ix(var.address+1);
				res->type.local=0;
			}
			else
			{
				ld_hl_immed(var.address);
				if (var.size == 0) // Array Pointer on stack (load the pointer)
				{
					res->location = GLOBAL;
					res->type.local = 0;
					push_ix;
					pop_bc;
					add_hl_bc;
					ld_c_mem_hl;
					inc_hl;
					ld_b_mem_hl;
					push_bc;
					pop_hl;
				}
			}
			push_hl;
		}
		else
		ERROR_RET(node->line,UNKNOWN_VAR);
	}
	else if (node->type == INDEX)
	{
		Term array_address;
		get_node_address(node->child,&array_address,length);
		if (array_address.type.base_type.type != ARRAY) ERROR_RET(node->line,INVALID_TYPE);
		word elem_size = type_size(node->line, &array_address.type.base_type);
		Term index;
		calculate_expression(node->child->sibling,&index);
		if (index.location == IMMEDIATE)
		{
			if (length && *length>0 && index.immediate >= *length)
				ERROR_RET(node->line, OUT_OF_BOUNDS);
			set_hl_immed(index.immediate * elem_size);
		}
		else
		{
			set_hl_res(node->line, &index);
			if (length)
			{
				set_de_immed(*length);
				word addr = get_known_address(node->line, sh_get("bounds_check"));
				const byte cmd[] = { 0xCD, (addr & 0xFF), (addr >> 8) };
				WRITE(cmd);
			}
			if (elem_size>1)
				multiply_hl(node->line, elem_size);
		}
		pop_bc; // Get the array address from the stack
		add_hl_bc; // Add the index
		if (array_address.type.local)
		{
			push_ix;
			pop_bc;
			add_hl_bc;
		}
		push_hl;
		res->type.base_type = array_address.type.base_type;
		res->type.base_type.type = VAR;
		res->type.local = 0; // No optimization yet.  TBD
	}
	else if (node->type == DOT)
	{
		Term struct_addr;
		get_node_address(node->child,&struct_addr,0);
		if (struct_addr.type.base_type.sub_type != STRUCT) ERROR_RET(node->line,INVALID_TYPE);
		Node* field_node = node->child->sibling;
		Term field;
		struct_field_offset(node->line, struct_addr.type.base_type.type_name, field_node->name, &field, length);
		if (field.location != IMMEDIATE) ERROR_RET(node->line,EXPECT_IMMED);
		res->type.local = struct_addr.type.local;
		res->type.base_type = field.type.base_type;
		pop_bc; // struct base address
		ld_hl_immed(field.immediate); // offset
		add_hl_bc; // base+offset
		push_hl;
	}
	else ERROR_RET(node->line,UNSUPPORTED);
}

void generate_call(Node* node, Term* res)
{
	FunctionPrototype* fp=find_prototype(node->name);
	if (!fp) ERROR_RET(node->line,UNKNOWN_FUNCTION);
	res->type.base_type=fp->return_type;
	res->location=fp->return_type.type_name==BYTE?A:HL;
	word n = vector_size(fp->parameters);
	Node* p=node->parameters;
	byte param_count=0;
	while (p)
	{
		if (param_count >= n) ERROR_RET(node->line, "Too many parameters");
		BaseType* param_type = vector_access(fp->parameters, param_count);
		++param_count;
		Term res;
		//if (p->data_type.type == ARRAY || p->data_type.sub_type == STRUCT)
		//{
		//	get_node_address(p,&res);
		//	if (res.location!=STACK) ERROR_RET(node->line,INVALID_LOCATION);
		//}
		//else
		{
			calculate_expression(p,&res);
			if (res.location==IMMEDIATE && param_type->type==VAR &&
				param_type->sub_type==PRIMITIVE)
			{
				// Immediate can map to both primitives
			}
			else if (res.type.base_type.type!=param_type->type || 
				res.type.base_type.sub_type!=param_type->sub_type ||
				res.type.base_type.type_name != param_type->type_name)
				ERROR_RET(node->line,INVALID_TYPE);

			set_hl_res(node->line, &res);
			push_hl;
		}
		p=p->sibling;
	}
	add_unknown_address(node->name, write_offset+1);
	const byte cmd[] = {0xCD, 0x00, 0x00};
	WRITE(cmd);
	param_count<<=1; // word per param
	for(byte i=0;i<param_count;++i)
		inc_sp;
}

void load_hl_stack_address(byte local)
{
	pop_hl;
	if (local)
	{
		push_ix;
		pop_de;
		add_hl_de;
	}
}

void generate_assignment(Node* node)
{
	Node* target_node = node->child;
	Term target_term, source_term;
	word length=0;
	get_node_address(target_node,&target_term,&length);
	word target_size = type_size(node->line, &target_term.type.base_type);
	Node* expr_node = target_node->sibling;
	calculate_expression(expr_node,&source_term);
	set_hl_res(node->line, &source_term);
	set_bc_hl;
	load_hl_stack_address(target_term.type.local);
	ld_mem_hl_c;
	if (target_size > 1)
	{
		inc_hl;
		ld_mem_hl_b;
	}
}

byte invert_condition(byte b)
{
	if (b == 0x38) return 0x30;
	if (b == 0x30) return 0x38;
	if (b == 0x28) return 0x20;
	if (b == 0x20) return 0x28;
	ERROR_RET(0xFFFF,INVALID_OPCODE);
	return 0;
}

word clear_condition_flag(byte b)
{
	if (b == 0x38) return 0xA7;		// JR C  ->  AND A
	if (b == 0x30) return 0x37;		// JR NC ->  SCF
	if (b == 0x28) return 0x01F6;	// JR Z -> OR 1
	if (b == 0x20) return 0xBF;		// JR NZ -> CP A
	ERROR_RET(0xFFFF, INVALID_OPCODE);
	return 0;
}

byte generate_condition(Node* node)
{
	if (node->type == PIPE)
	{
		byte b = generate_condition(node->child);
		word success_end = sh_temp();
		add_unknown_address(success_end, write_offset+3);
		const byte left_cmd[] = { invert_condition(b), 0x03, 0xC3, 0x00, 0x00 };
		WRITE(left_cmd);
		b = generate_condition(node->child->sibling);
		byte right_cmd[] = { b, 0x02, 0x00, 0x00 };
		word fail = clear_condition_flag(b);
		right_cmd[2] = fail & 0xFF;
		right_cmd[3] = fail >> 8;
		WRITE(right_cmd);
		add_known_address(success_end, write_offset);
		return b;
	}
	else
	if (node->type == AMP)
	{
		byte b = generate_condition(node->child);
		word failure_end = sh_temp();
		add_unknown_address(failure_end, write_offset + 3);
		const byte left_cmd[] = { b, 0x03, 0xC3, 0x00, 0x00 };
		WRITE(left_cmd);
		b = generate_condition(node->child->sibling);
		byte right_cmd[] = { b, 0x02, 0x00, 0x00 };
		word fail = clear_condition_flag(b);
		right_cmd[2] = fail & 0xFF;
		right_cmd[3] = fail >> 8;
		add_known_address(failure_end, write_offset + 2);
		WRITE(right_cmd);
		return b;
	}
	else
	{
		if (node->type==LPAREN) return generate_condition(node->child);
		else
		{
			byte swap = (node->type == GT || node->type == LE);
			if (node->type == LT || node->type == GT || node->type == LE ||
				node->type == GE || node->type == EQ || node->type == NE)
			{
				Term left,right;
				calculate_expression(node->child, &left);
				set_a_res(node->line, &left);
				push_af;
				calculate_expression(node->child->sibling, &right);
				set_a_res(node->line, &right);
				if (swap)
				{
					pop_hl;
				}
				else
				{
					set_h_a;
					pop_af;
				}
			}
			else ERROR_RET(node->line,UNSUPPORTED);
			write_byte(0xBC); // CP H
			switch (node->type)
			{
			case LT: return 0x38; // JR C
			case GT: return 0x38; // JR C
			case LE: return 0x30; // JR NC
			case GE: return 0x30; // JR NC
			case EQ: return 0x28; // JR Z
			case NE: return 0x20; // JR NZ
			default: ERROR_RET(node->line,UNSUPPORTED);
			}
		}
	}
	ERROR_RET(node->line,UNSUPPORTED);
	return 0;
}

void generate_cond_block(Node* node, byte loop)
{
	if (!node->parameters) ERROR_RET(node->line,MISSING_NODE);
	word start_addr = write_offset+0x1000;
	byte jump = generate_condition(node->parameters);
	word end_of_block = sh_temp();
	add_unknown_address(end_of_block, write_offset + 3);
	const byte cmd[] = { jump, 0x03, 0xC3, 0x00, 0x00 };
	WRITE(cmd);
	generate_block(node);
	if (loop)
	{
		const byte jump_back[] = { 0xC3, (start_addr & 0xFF), (start_addr >> 8) };
		WRITE(jump_back);
	}
	add_known_address(end_of_block, write_offset);
}

void generate_ifelse(Node* node)
{
	if (!node->parameters) ERROR_RET(node->line,MISSING_NODE);
	byte jump = generate_condition(node->parameters);
	word end_of_true = sh_temp();
	add_unknown_address(end_of_true, write_offset + 3);
	const byte true_cmd[] = { jump, 0x03, 0xC3, 0x00, 0x00 };
	WRITE(true_cmd);
	generate_block(node->child); // True side of if-else
	word end_of_else = sh_temp();
	add_unknown_address(end_of_else, write_offset+1);
	const byte jump_to_end[] = { 0xC3, 0x00, 0x00 };
	WRITE(jump_to_end);
	add_known_address(end_of_true, write_offset);
	generate_block(node->child->sibling);
	add_known_address(end_of_else,write_offset);
}

void generate_return(Node* node)
{
	if (!node->parameters) ERROR_RET(node->line, MISSING_NODE);
	Term res;
	calculate_expression(node->parameters, &res);
	if (function_node->data_type.type_name == BYTE)
	{
		set_a_res(node->line, &res);
	}
	else
	{
		set_hl_res(node->line, &res);
		set_de_hl;
	}
	add_unknown_address(function_end, write_offset+1);
	const byte cmd[] = { 0xC3, 0x00, 0x00 };
	WRITE(cmd);
}

void generate_statement(Node* statement)
{
	Term res;
#ifdef DEV
	write_offset_line(statement->line);
#endif
	if (statement->type == ASSIGN) generate_assignment(statement);
	else if (statement->type == WHILE) generate_cond_block(statement,1);
	else if (statement->type == IF) generate_cond_block(statement,0);
	else if (statement->type == CALL) generate_call(statement,&res);
	else if (statement->type == IFELSE) generate_ifelse(statement);
	else if (statement->type == RETURN) generate_return(statement);
	else if (statement->type == VAR);
	else ERROR_RET(statement->line,UNSUPPORTED);
}

void generate_block(Node* block)
{
	Node* child = block->child;
	while (child)
	{
		generate_statement(child);
		child = child->sibling;
	}
}

// Accepts node of funuction, memory location of the function (offset) and size of local variables
// Returns the offset beyond the function
void generate_function(Node* func, word locals_size)
{
	FunctionAddress fa;
	function_end = sh_temp();
	function_node = func;
	write_offset_line(func->line);
	add_known_address(func->name,write_offset);
	fa.start=write_offset;
	word neg_locals = -locals_size;
	byte init_stack[] = {
		0xDD, 0xE5,								// PUSH IX
		0xDD,0x21,0x00,0x00,					// LD IX,#0
		0xDD,0x39,								// ADD IX,SP
		0x21,(neg_locals&255),(neg_locals>>8),	// LD HL,-locals_size
		0x39,									// ADD HL,SP
		0xF9									// LD SP,HL
	};
	byte close_stack[] = {
		0xDD, 0xF9,								// LD SP,IX
		0xDD, 0xE1,								// POP IX
		0xC9									// RET
	};
	WRITE(init_stack);
	generate_block(func);
	add_known_address(function_end,write_offset);
	WRITE(close_stack);
	fa.stop=write_offset;
	vector_push(function_addresses, &fa);
}

// Common functions only accept and return primitives
// Format string is one letter (B/W) for return type and one such letter
// per parameter.  Example:  "BWW" ->  fun name(word a, word b)
void add_common_prototype(word name, const char* proto)
{
	if (proto && *proto)
	{
		FunctionPrototype fp;
		BaseType param_type;
		fp.name=name;
		set_prim_type(&fp.return_type, *proto == 'B' ? BYTE : WORD);
		fp.parameters=vector_new(sizeof(BaseType));
		while (*(++proto))
		{
			if (*proto == 'P')
			{
				param_type.type=ARRAY;
				param_type.sub_type=PRIMITIVE;
				param_type.type_name=BYTE;
			}
			else
				set_prim_type(&param_type, *proto == 'B' ? BYTE : WORD);
			vector_push(fp.parameters,&param_type);
		}
		vector_push(function_prototypes, &fp);
	}
}

void generate_common_functions()
{
#define COMMON_FUNC(func_name,proto,...) {\
Address address; address.name=sh_get(func_name); address.address=write_offset;\
add_common_prototype(address.name,proto);\
vector_push(knowns, &address); const byte code_bytes[] = __VA_ARGS__; WRITE(code_bytes); }

	byte mult_offset=write_offset; // Assume low 0x1000 address, store low byte
	// Generic multiplication   HL = BC * DE
	COMMON_FUNC("mult_bc_de", "", { 0x21, 0x00, 0x00, 0x78, 0x06, 0x10, 0x29, 0xCB,
								    0x21,0x17,0x30,0x01,0x19,0x10,0xF7,0xC9 });

	COMMON_FUNC("multiply", "BWW",
	//            pop hl  pop bc  pop de  push de   push bc  push hl  jp mult_bc_de
				{ 0xE1,   0xC1,   0xD1,   0xD5,     0xC5,    0xE5,    0xC3, mult_offset, 0x10 });
	
	// OS Service, send block to GPU
	//                               pop bc  pop hl  push hl  push bc    ld a,service              RST 1  ret
	COMMON_FUNC("gpu_block", "BP", { 0xC1,   0xE1,   0xE5,    0xC5,      0x3E, SERVICE_GPU_BLOCK,  0xCF,  0xC9 });

	// OS Service, flush GPU
	//                              ld a,service     RST 1   ret
	COMMON_FUNC("gpu_flush", "B", { 0x3E, SERVICE_GPU_FLUSH, 0xCF,   0xC9 });

	// OS Service, rng
	//                        ld a,service       RST 1  ret
	COMMON_FUNC("rng", "W", { 0x3E, SERVICE_RNG, 0xCF, 0xC9 });

	//                          ld a,service         RST 1  ret
	COMMON_FUNC("timer", "W", { 0x3E, SERVICE_TIMER, 0xCF, 0xC9});

	// OS Service, cls
	//                        ld a,service       RST 1  ret
	COMMON_FUNC("cls", "B", { 0x3E, SERVICE_CLS, 0xCF, 0xC9 });

	//							      ld a,service               RST   RET
	COMMON_FUNC("input_empty", "B", { 0x3E, SERVICE_INPUT_EMPTY, 0xCF, 0xC9 });

	//                               ld a,service              RST   RET
	COMMON_FUNC("input_read", "B", { 0x3E, SERVICE_INPUT_READ, 0xCF, 0xC9 });

	//                        pop hl  pop af  push af  jp (hl)
	//COMMON_FUNC("highbyte", "BW", { 0xE1, 0xF1, 0xF5, 0xE9 });

	//                       pop hl  pop bc  push bc  ld a,c  jp (hl)
	//COMMON_FUNC("lowbyte", "BW", { 0xE1, 0xC1, 0xC5, 0x79, 0xE9 });

	//                                 LD A,service                RST   RET
	COMMON_FUNC("bounds_check", "B", { 0x3E, SERVICE_BOUNDS_CHECK, 0xCF, 0xC9 });

#undef COMMON_FUNC
}

void fill_unknowns()
{
	word i=0,n=vector_size(unknowns);
	for (; i < n; ++i)
	{
		Address* unk=(Address*)vector_access(unknowns,i);
		word addr=get_known_address(0xFFFF,unk->name);
		raw_write(unk->address,(byte*)&addr,2);
	}
}

void add_variable(Node* node)
{
	Variable var;
	var.type.local = 0;
	var.name = node->name;
	var.address = write_offset  + 0x1000;
	var.size = var_size(node);
	var.type.base_type = node->data_type;
	vector_push(variables, &var);
	const byte* data=0;
	if (node->data_type.type==ARRAY && node->data)
		data = vector_access(node->data, 0);
	else if (node->data_type.type==VAR && node->parameters)
		data = (const byte*)&node->parameters->name;
	for (word i = 0; i < var.size; ++i)
	{
		if (data) write_byte(*data++);
		else write_byte(0);
	}
}

void add_struct(Node* node)
{
	Struct s;
	s.name= node->name;
	s.fields = vector_new(sizeof(Field));
	Node* child=node->child;
	Field field;
	while (child)
	{
		field.name = child->name;
		field.type = child->data_type;
		field.length = 1;
		if (child->data_type.type == ARRAY)
			field.length = child->parameters->name;
		vector_push(s.fields, &field);
		child=child->sibling;
	}
	vector_push(structs, &s);
}

FunctionPrototype* find_prototype(word name)
{
	word n = vector_size(function_prototypes);
	for (word i = 0; i < n; ++i)
	{
		FunctionPrototype* fp = vector_access(function_prototypes, i);
		if (fp->name == name) return fp;
	}
	return 0;
}

void add_function_prototype(Node* node)
{
	if (find_prototype(node->name)) return;
	FunctionPrototype fp;
	fp.name=node->name;
	fp.return_type=node->data_type;
	fp.parameters=vector_new(sizeof(BaseType));
	Node* param=node->parameters;
	while (param)
	{
		vector_push(fp.parameters,&param->data_type);
		param=param->sibling;
	}
	vector_push(function_prototypes, &fp);
}

void add_function(Node* node)
{
	add_function_prototype(node);
	if (node->child) // not extern
	{
		word globals_size = vector_size(variables);
		scan_parameters(node);
		word locals_size = scan_variables(node, 0, 1);
		generate_function(node, locals_size);
		vector_resize(variables, globals_size); // Remove local vars
	}
}

byte generate_code(parse_node_func parse_node_, file_write_func fwf)
{
	parse_node = parse_node_;
	raw_write = fwf;

	byte header[] = { 0xC3, 0x00, 0x00 };
	WRITE(header);
	add_unknown_address(sh_get("main"), 1);
	generate_common_functions();
	while (1)
	{
		Node* node = parse_node();
		if (!node) break;
		switch (node->type)
		{
		case VAR:		add_variable(node); break;
		case STRUCT:	add_struct(node);	break;
		case FUN:		add_function(node);	break;
		default: ERROR_RET(node->line,UNSUPPORTED);
		}
		release_node(node); // Rolling generation, release completed nodes
	}
	fill_unknowns();
	return 1;
}

void gen_init()
{
	write_offset = 0;
	structs = vector_new(sizeof(Struct));
	variables = vector_new(sizeof(Variable));
	knowns = vector_new(sizeof(Address));
	unknowns = vector_new(sizeof(Address));
	function_addresses = vector_new(sizeof(FunctionAddress));
	function_prototypes = vector_new(sizeof(FunctionPrototype));
}

Vector* gen_get_functions()
{
	return function_addresses;
}

Vector* gen_get_unknowns()
{
	return unknowns;
}

void gen_shut()
{
#ifdef DEV
	close_line_offsets();
	close_abs_addr();
#endif
	vector_shut(function_addresses);
	vector_shut(unknowns);
	vector_shut(knowns);
	vector_shut(variables);
	word n=vector_size(structs);
	for (word i = 0; i < n; ++i)
	{
		Struct* s=vector_access(structs,i);
		vector_shut(s->fields);
	}
	n = vector_size(function_prototypes);
	for (word i = 0; i < n; ++i)
	{
		FunctionPrototype* fp = vector_access(function_prototypes, i);
		vector_shut(fp->parameters);
	}
	vector_shut(function_prototypes);
	vector_shut(structs);
}

