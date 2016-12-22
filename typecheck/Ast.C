#include "Ast.h"					
#include "ParserUtil.h"					
#include "Instruction.h"
#define MAXBUF	256

// indicate we are in the scope of rule, to allievate the check of assignments
// where only assignments to global variable is allowed when in rule scope
bool inRuleScope = false;
extern int REG_BP;
extern int REG_SP;
extern int REG_RV;
extern int REG_RA;
extern int REG_RL;

AstNode::AstNode(NodeType nt, int line, int column, string file):
  ProgramElem(NULL, line, column, file) {
	// Add your code here
	nodeType_ = nt;
}

AstNode::AstNode(const AstNode& ast): ProgramElem(ast) {
	// Add your code here
	nodeType_ = ast.nodeType();
}

/****************************************************************/

ExprNode::ExprNode(ExprNodeType et, const Value* val, int line, int column, 
				   string file):
	AstNode(AstNode::NodeType::EXPR_NODE, line, column, file)
{
	// Add your code here
	exprType_ = et;
	val_ = val;
}


ExprNode::ExprNode(const ExprNode& e) : AstNode(e)
{
	// Add your code here
	exprType_ = e.exprNodeType();
	val_ = e.value();
}
/****************************************************************/
extern const OpNode::OpInfo opInfo[] = {
  // print name, arity, paren_flag, fixity, arg types, out type, constraints
  //
  // Paren_flag -- opnode->print() outputs is surrounded by parenthesis if 
  // this flag is set. As set below, the expression may not print correctly
  // in some rare cases, e.g., ~(b * c) will get printed as ~b * c,
  // which actually corresponds to (~b)*c. To ensure that things get printed
  // correctly all the time, more paren_flags should be set to 1, but this
  // will lead to more clutter in printed output. Basically, what we have done
  // here is to look are expressions by type -- arithmetic, relational, 
  // boolean, bit operations, etc. Within each type, the highest priority 
  // operator is printed without paren. This will work correctly, as long
  // as the language doesn't permit mixing of different types of expressions.
  // But this assumption doesn't always hold, as in the example above. Also,
  // there is an exception to this general approach in the case of unary minus
  // and * -- since (-a)*b and -(a*b) have the same meaning, we can exclude
  // paren for * without an error.
  //
  // Codes for constraints:
  // first character:
  //    N: No additional constraint over what is given by argTypes
  //    I: all arguments must have identical type
  //    S: one of the arguments must have a type that is a supertype of
  //        of all other arguments. All other arguments require a coercion 
  //        operation to be introduced so as to convert their type to S.
  //    s: one of the arguments must have a type that is a subtype of
  //        of all other arguments. 
  //    L: all list arguments (and list output) must have same type. In 
  //        addition, all non-list arguments (and output) must have same 
  //        type as that of elements in these lists
  //    T: all tuple arguments to the function must have same type.
  //    A: (assignment). Type of second argument must be a subtype of
  //       the first argument
  //
  // second character:
  //    O: output type is the same as out type. (In the following cases,
  //        the output type need not be equal to out type, but a subtype
  //        of it.) Since a TypeTag provides complete type information only
  //        for primitive types, `O' is applicable only in this case.
  //    digit: output type is the same as that of the digit'th argument
  //       In this case, a third character may be used, the code for
  //       which is as follows:
  //         'e' denotes that the output is of type alpha, where
  //             the type of digit'th argument is list(alpha)
  //         'l' denotes that the output is of type list(alpha), where
  //             alpha is the type of the digit'th argument.
  //    S: The output type is the same as that of the argument with the
  //        most general type. (Typically used with first character 'S')
  //    s: The output type is the same as that of the argument with the
  //        least general type. (Typically used with first character 'S')
  //    P: The output type is the product of the types of all arguments
  //    p: The output type is a component of the input tuple type. The
  //        following character specifies the component. A digit k specifies
  //        that the component number as k. The character 'a' indicates that
  //        the component number is given by an integer argument to the
  //        operator. The argument number is given by the following digit.
  //        'p' can be used only in conjunction with first character 'P'.
  //    L: Output type is the same as type of list arguments. Can be used
  //        only in conjunction with first character L.
  //    e: Output type is the same as type of element of list arguments. 
  //        Can be used only in conjunction with first character L.
  //
  {OpNode::OpCode::UMINUS, "-",  1, 0, OpNode::OpPrintType::PREFIX, {Type::SIGNED}, Type::SIGNED, "N1"},
  {OpNode::OpCode::PLUS, "+",  2, 1, OpNode::OpPrintType::INFIX, {Type::NUMERIC, Type::NUMERIC}, Type::NUMERIC, "SS"},
  {OpNode::OpCode::MINUS, "-",  2, 1, OpNode::OpPrintType::INFIX, {Type::NUMERIC, Type::NUMERIC}, Type::NUMERIC, "SS"},
  {OpNode::OpCode::MULT, "*",  2, 0, OpNode::OpPrintType::INFIX, {Type::NUMERIC, Type::NUMERIC}, Type::NUMERIC, "SS"},
  {OpNode::OpCode::DIV, "/",  2, 1, OpNode::OpPrintType::INFIX, {Type::NUMERIC, Type::NUMERIC}, Type::NUMERIC, "SS"},
  {OpNode::OpCode::MOD, "%",  2, 1, OpNode::OpPrintType::INFIX, {Type::INTEGRAL, Type::INTEGRAL}, Type::INTEGRAL, "S2"},
  {OpNode::OpCode::EQ, "==", 2, 0, OpNode::OpPrintType::INFIX, {Type::PRIMITIVE, Type::PRIMITIVE}, Type::BOOL, "SO"},
  {OpNode::OpCode::NE, "!=", 2, 0, OpNode::OpPrintType::INFIX, {Type::PRIMITIVE, Type::PRIMITIVE}, Type::BOOL, "SO"},
  {OpNode::OpCode::GT, ">",  2, 0, OpNode::OpPrintType::INFIX, {Type::SCALAR, Type::SCALAR}, Type::BOOL, "SO"},
  {OpNode::OpCode::LT, "<",  2, 0, OpNode::OpPrintType::INFIX, {Type::SCALAR, Type::SCALAR}, Type::BOOL, "SO"},
  {OpNode::OpCode::GE, ">=", 2, 0, OpNode::OpPrintType::INFIX, {Type::SCALAR, Type::SCALAR}, Type::BOOL, "SO"},
  {OpNode::OpCode::LE, "<=", 2, 0, OpNode::OpPrintType::INFIX, {Type::SCALAR, Type::SCALAR}, Type::BOOL, "SO"},
  {OpNode::OpCode::AND, "&&",  2, 1, OpNode::OpPrintType::INFIX, {Type::BOOL, Type::BOOL}, Type::BOOL, "NO"},
  {OpNode::OpCode::OR, "||",  2, 1, OpNode::OpPrintType::INFIX, {Type::BOOL, Type::BOOL}, Type::BOOL, "NO"},
  {OpNode::OpCode::NOT, "!",  1, 0, OpNode::OpPrintType::PREFIX, {Type::BOOL}, Type::BOOL, "NO"}, 
  {OpNode::OpCode::BITNOT, "~",  1, 0, OpNode::OpPrintType::PREFIX, {Type::INTEGRAL}, Type::INTEGRAL, "N1"},
  {OpNode::OpCode::BITAND, "&",  2, 1, OpNode::OpPrintType::INFIX, {Type::INTEGRAL, Type::INTEGRAL}, Type::INTEGRAL, "Ss"},
  {OpNode::OpCode::BITOR, "|",  2, 1, OpNode::OpPrintType::INFIX, {Type::INTEGRAL, Type::INTEGRAL}, Type::INTEGRAL, "SS"},
  {OpNode::OpCode::BITXOR, "^",  2, 0, OpNode::OpPrintType::INFIX, {Type::INTEGRAL, Type::INTEGRAL}, Type::INTEGRAL, "SS"},
  {OpNode::OpCode::SHL, "<<", 2, 1, OpNode::OpPrintType::INFIX, {Type::INTEGRAL, Type::INTEGRAL}, Type::INTEGRAL, "N1"},
  {OpNode::OpCode::SHR, ">>", 2, 1, OpNode::OpPrintType::INFIX, {Type::INTEGRAL, Type::INTEGRAL}, Type::INTEGRAL, "N1"},
  {OpNode::OpCode::ASSIGN, "=",  2, 0, OpNode::OpPrintType::INFIX, {Type::NATIVE, Type::NATIVE}, Type::VOID, "AO"},
  {OpNode::OpCode::PRINT, "print", OpNode::VARIABLE, 1, OpNode::OpPrintType::PREFIX, {Type::NATIVE}, Type::VOID, "NO"},
  {OpNode::OpCode::INVALID, "invalid",            0, 0, OpNode::OpPrintType::PREFIX, {}, Type::ERROR, "NO"}
};

OpNode::OpNode(OpCode op, ExprNode* a1, ExprNode* a2, 
			   int ln, int col, string file):
  ExprNode(ExprNode::ExprNodeType::OP_NODE, NULL, ln,col,file) {
  opCode_ = op;
  if (a1 != NULL) {
	arity_ = 1;
	arg_.push_back(a1);
	if (a2 != NULL) {
	  arity_++;
	  arg_.push_back(a2);
	}
  }
}

OpNode::OpNode(const OpNode &other):
  ExprNode(other) {
  arity_ = other.arity();
  opCode_ = other.opCode();
  for (unsigned int i=0; (i < other.arity()); i++) {
    if (other.arg_[i]) {
      arg_.push_back((other.arg_[i])->clone());
    } 
	else {
      arg_.push_back(NULL);
    }
  }
}

const Type* OpNode::typeCheck() {
	const Type *type, *ltype, *rtype;
	char msg[MAXBUF];

	OpCode opCode = this->opCode();
	ltype = this->arg(0)->typeCheck();
	if (this->arg(1) != NULL)
		rtype = this->arg(1)->typeCheck();
	int argIdx = 0;

	switch(opCode) {
		case OpNode::OpCode::UMINUS:
			if(!ltype->isNumeric(ltype->tag())) {
				// TODO handle type error here
				sprintf(msg,"Incompatible type for argument 1 for operator `%s'",opInfo[(int)opCode].name_);
				errMsg(msg, this->line(), this->column(), this->file().c_str());
			}

			if (ltype->isIntegral(ltype->tag()))
				type = new Type::Type(Type::TypeTag::INT);
			else
				type = ltype;
			break;
		case OpNode::OpCode::PLUS:
		case OpNode::OpCode::MINUS:
		case OpNode::OpCode::MULT:
		case OpNode::OpCode::DIV:
			if (!ltype->isNumeric(ltype->tag()) || !rtype->isNumeric(rtype->tag())) {
				// TODO handle type error here
				if (!ltype->isNumeric(ltype->tag()))
					argIdx = 1;
				else
					argIdx = 2;
				sprintf(msg,"Incompatible type for argument %d for operator `%s'",argIdx,opInfo[(int)opCode].name_);
				errMsg(msg, this->line(), this->column(), this->file().c_str());
				// TODO should we create a numeric type to avoid propagate of type errors?
				// maybe we can choose the numeric type and return
				if (ltype->isNumeric(ltype->tag()))
					type = ltype;
				else if (rtype->isNumeric(rtype->tag()))
					type = rtype;
				else
					type = new Type(Type::TypeTag::INT);
					//type = NULL;

			} else if (ltype->tag() == rtype->tag()) {
				type = ltype;
			} else if (ltype->isSubType(rtype) || rtype->isSubType(ltype)) {
				// type not match, set coerced type here
				if (ltype->tag() > rtype->tag()) {
					type = ltype;
					this->arg(1)->coercedType(type);
				} else {
					type = rtype;
					this->arg(0)->coercedType(type);
				}

			} else {
				// TODO
				errMsg("type error: unexpected type error");
				// type = new Type(Type::TypeTag::INT);
				type = NULL;
			}
			break;
		case OpNode::OpCode::BITAND:
		case OpNode::OpCode::BITOR:
		case OpNode::OpCode::BITXOR:
		case OpNode::OpCode::SHL:
		case OpNode::OpCode::SHR:
		case OpNode::OpCode::MOD:
			if ((! ltype->isIntegral(ltype->tag())) || (! rtype->isIntegral(rtype->tag()))) {
				// TODO handle type error here
				if (!ltype->isIntegral(ltype->tag()))
					argIdx = 1;
				else
					argIdx = 2;
				sprintf(msg,"Incompatible type for argument %d for operator `%s'",argIdx, opInfo[(int)opCode].name_);
				errMsg(msg, this->line(), this->column(), this->file().c_str());
				// TODO should we create a numeric type to avoid propagate of type errors?
				// maybe we can choose the numeric type and return
				if (ltype->isIntegral(ltype->tag()))
					type = ltype;
				else if (rtype->isIntegral(rtype->tag()))
					type = rtype;
				else
					// type = new Type(Type::TypeTag::INT);
					type = NULL;
			} else if (ltype->tag() == rtype->tag()) {
				type = ltype;
			} else {
				// type not match, set coerced type here
				if (ltype->tag() > rtype->tag()) {
					type = ltype;
					this->arg(1)->coercedType(type);
				} else {
					type = rtype;
					this->arg(0)->coercedType(type);
				}
			}
			break;

		case OpNode::OpCode::BITNOT:
			if (! ltype->isIntegral(ltype->tag())) {
				// TODO handle type error here
				sprintf(msg,"Incompatible type for argument 1 for operator `%s'",opInfo[(int)opCode].name_);
				errMsg(msg, this->line(), this->column(), this->file().c_str());
				//type = NULL;
				type = new Type(Type::TypeTag::INT);
			} else {
				type = ltype;
			}
			break;

		case OpNode::OpCode::EQ:
		case OpNode::OpCode::NE:
		case OpNode::OpCode::GT:
		case OpNode::OpCode::LT:
		case OpNode::OpCode::GE:
		case OpNode::OpCode::LE:
			if (!ltype->isNumeric(ltype->tag()) || !rtype->isNumeric(rtype->tag())) {
				// TODO handle type error here
				if (!ltype->isIntegral(ltype->tag()))
					argIdx = 1;
				else
					argIdx = 2;
				sprintf(msg,"Incompatible type for argument %d for operator `%s'",argIdx,opInfo[(int)opCode].name_);
				errMsg(msg, this->line(), this->column(), this->file().c_str());

			} else if (ltype->tag() == rtype->tag()) {
				// Do nothing
			} else if (ltype->isSubType(rtype) || rtype->isSubType(ltype)) {
				// type not match, set coerced type here
				if (ltype->tag() > rtype->tag()) {
					this->arg(1)->coercedType(type);
				} else {
					this->arg(0)->coercedType(type);
				}
			} else {
				// TODO
				errMsg("type error: unexpected type error");
			}

			// return bool type
			type = new Type(Type::TypeTag::BOOL);
			break;
		case OpNode::OpCode::NOT:
			if (! ltype->isBool(ltype->tag())) {
				// TODO handle type error here
				sprintf(msg,"Incompatible type for argument 1 for operator `%s'",opInfo[(int)opCode].name_);
				errMsg(msg, this->line(), this->column(), this->file().c_str());
			}
			type = new Type(Type::TypeTag::BOOL);
			break;
		case OpNode::OpCode::AND:
		case OpNode::OpCode::OR:
			if (!ltype->isBool(ltype->tag()) || !rtype->isBool(rtype->tag())) {
				// TODO handle type error here
				if (!ltype->isBool(ltype->tag()))
					argIdx = 1;
				else
					argIdx = 2;
				sprintf(msg,"Incompatible type for argument %d for operator `%s'",argIdx,opInfo[(int)opCode].name_);
				errMsg(msg, this->line(), this->column(), this->file().c_str());
			}
			type = new Type(Type::TypeTag::BOOL);
			break;
		case OpNode::OpCode::ASSIGN:
			if (ltype->tag() == rtype->tag()) {
				// Do nothing
			} else if (ltype->isSubType(rtype)) {
					this->arg(1)->coercedType(ltype);
			} else {
				// TODO
				errMsg("Assigned expression must be a subtype of target", this->line(), this->column(), this->file().c_str());
			}

			// extra check for assignment if in rule scope
			if (inRuleScope) {
				SymTabEntry *ste = stm.lookUp(((RefExprNode *)this->arg(0))->ext());
				// because we are in global scope now, so if we can find symtabentry, it's ok
				if (ste == NULL)
					errMsg("type error: assignment inside rule can only assign to global variables", this->line(), this->column(), this->file().c_str());
			}

			// return bool type, TODO for assignment, we are required to return true!
			type = new Type(Type::TypeTag::BOOL);
			break;
		default:
			errMsg("Unsupported OpCode: type check not support here, default action: return ltype");
			type = ltype;
			break;
	}

	return type;
}

void 
OpNode::print(ostream& os, int indent) const {
	int iopcode = static_cast<int>(opCode_);
  if (opInfo[iopcode].prtType_ == OpNode::OpPrintType::PREFIX) {
	os << opInfo[iopcode].name_;
	if (arity_ > 0) {
	  if (opInfo[iopcode].needParen_) 
		os << '(';
	  for (unsigned i=0; i < arity_-1; i++) {
		if (arg_[i])
		  arg_[i]->print(os, indent);
	    else os << "NULL";
		os << ", ";
	  }
      if (arg_[arity_-1])
		arg_[arity_-1]->print(os, indent);
	  else os << "NULL";
	  if (opInfo[iopcode].needParen_) 
		os << ") ";
	}
  }
  else if ((opInfo[iopcode].prtType_ == OpNode::OpPrintType::INFIX) && (arity_ == 2)) {
	if (opInfo[iopcode].needParen_) 
	  os << "(";
	if(arg_[0])
	  arg_[0]->print(os, indent);
	else os << "NULL";
	os << opInfo[iopcode].name_; 
	if(arg_[1])
	  arg_[1]->print(os, indent);
	else os << "NULL";
	if (opInfo[iopcode].needParen_) 
	  os << ")";
  }
  else internalErr("Unhandled case in OpNode::print");
}

void 
OpNode::typePrint(ostream& os, int indent) const {
	int iopcode = static_cast<int>(opCode_);
  if (opInfo[iopcode].prtType_ == OpNode::OpPrintType::PREFIX) {
	os << opInfo[iopcode].name_;
	if (arity_ > 0) {
	  if (opInfo[iopcode].needParen_) 
		os << '(';
	  for (unsigned i=0; i < arity_-1; i++) {
		if (arg_[i])
		  arg_[i]->typePrint(os, indent);
	    else os << "NULL";
		os << ", ";
	  }
      if (arg_[arity_-1])
		arg_[arity_-1]->typePrint(os, indent);
	  else os << "NULL";
	  if (opInfo[iopcode].needParen_) 
		os << ") ";
	}
  }
  else if ((opInfo[iopcode].prtType_ == OpNode::OpPrintType::INFIX) && (arity_ == 2)) {
	if (opInfo[iopcode].needParen_) 
	  os << "(";
	if(arg_[0])
	  arg_[0]->typePrint(os, indent);
	else os << "NULL";
	os << opInfo[iopcode].name_; 
	if(arg_[1])
	  arg_[1]->typePrint(os, indent);
	else os << "NULL";
	if (opInfo[iopcode].needParen_) 
	  os << ")";
  }
  else internalErr("Unhandled case in OpNode::print");
}

string OpNode::codeGen(RegManager *rm) {
	string code;
	int tmpReg1, tmpReg2, destReg;
	unsigned int arity, i;
	bool isFloat = false;
	bool useDestReg = false;
	//const Type *ctype = this->coercedType();
	const Type *type = this->type();
	Instruction::Operand *arg1, *arg2, *dest;
	ExprNode *op1, *op2, *op;
	//MovIns *mi;
	ArithIns *ai = NULL;
	RelOpIns *roi = NULL;
	FloatArithIns *fai = NULL;
	FloatRelOpIns *froi = NULL;
	MovIns *mvi, *mvi1, *mvi2;  // all instructions for SHR, SHL
	OpNode::OpCode opcode = this->opCode();
	
	int tmpReg3, tmpReg4; // specifically for SHR, SHL

	// inherit existing code
	arity = this->arity();
	for (i = 0; i < arity; i++) {
		op = this->arg(i);
		code += op->codeGen(rm);
	}

	// 1. get original type
	if(type->tag() == Type::TypeTag::DOUBLE)
		isFloat = true;
	else
		isFloat = false;

	op1 = this->arg(0);
	op2 = this->arg(1);

	tmpReg1 = op1->getTmpReg();
	if (op2 != NULL)
		tmpReg2 = op2->getTmpReg();
	else
		tmpReg2 = -1;

	//code += ai->toString();
	if (isFloat) {
		// alloc a caller-save, float reg
		destReg = rm->getReg(true, true);
		arg1 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, tmpReg1);
		arg2 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, tmpReg2);
		dest = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, destReg);

		switch (opcode) {
			case OpNode::OpCode::UMINUS:
				fai = new FloatArithIns(FloatArithIns::FloatArithInsType::FNEG, arg1, NULL, dest);
				useDestReg = true;
				break;
			case OpNode::OpCode::PLUS:
				fai = new FloatArithIns(FloatArithIns::FloatArithInsType::FADD, arg1, arg2, dest);
				useDestReg = true;
				break;
			case OpNode::OpCode::MINUS:
				fai = new FloatArithIns(FloatArithIns::FloatArithInsType::FSUB, arg1, arg2, dest);
				useDestReg = true;
				break;
			case OpNode::OpCode::DIV:
				fai = new FloatArithIns(FloatArithIns::FloatArithInsType::FDIV, arg1, arg2, dest);
				useDestReg = true;
				break;
			case OpNode::OpCode::MULT:
				fai = new FloatArithIns(FloatArithIns::FloatArithInsType::FMUL, arg1, arg2, dest);
				useDestReg = true;
				break;
			case OpNode::OpCode::EQ:
				froi = new FloatRelOpIns(FloatRelOpIns::FloatRelOpInsType::FEQ, arg1, arg2);
				useDestReg = false;
				break;
			case OpNode::OpCode::NE:
				froi = new FloatRelOpIns(FloatRelOpIns::FloatRelOpInsType::FNE, arg1, arg2);
				useDestReg = false;
				break;
			case OpNode::OpCode::GT:
				froi = new FloatRelOpIns(FloatRelOpIns::FloatRelOpInsType::FGT, arg1, arg2);
				useDestReg = false;
				break;
			case OpNode::OpCode::GE:
				froi = new FloatRelOpIns(FloatRelOpIns::FloatRelOpInsType::FGE, arg1, arg2);
				useDestReg = false;
				break;
			// NOTE! we don't have FLT Instruction! use FGT instead
			case OpNode::OpCode::LT:
				froi = new FloatRelOpIns(FloatRelOpIns::FloatRelOpInsType::FGT, arg2, arg1);
				useDestReg = false;
				break;
			case OpNode::OpCode::LE:
				froi = new FloatRelOpIns(FloatRelOpIns::FloatRelOpInsType::FGE, arg2, arg1);
				useDestReg = false;
				break;
			default:
				break;
			// TODO handle assignment separately
			// TODO handle print separately
			//case OpNode::OpCode::ASSIGN:
			//	fai = new MovIns(MovIns::MovInsType::MOVF, arg1, dest);
			//	break;
		}
		// TODO
		// handle other things
		if (fai != NULL)
			code += fai->toString();
		if (froi != NULL)
			code += froi->toString();
		if (!useDestReg) {
			rm->releaseReg(destReg, true);
		}
		// TODO
		// release op1, op2 when necessary
	} else {
		// alloc a caller-save, integer reg
		destReg = rm->getReg(true, false);
		arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
		arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg2);
		dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, destReg);

		switch (opcode) {
			case OpNode::OpCode::UMINUS:
				ai = new ArithIns(ArithIns::ArithInsType::NEG, arg1, NULL, dest);
				useDestReg = true;
				break;
			case OpNode::OpCode::PLUS:
				ai = new ArithIns(ArithIns::ArithInsType::ADD, arg1, arg2, dest);
				useDestReg = true;
				break;
			case OpNode::OpCode::MINUS:
				ai = new ArithIns(ArithIns::ArithInsType::SUB, arg1, arg2, dest);
				useDestReg = true;
				break;
			case OpNode::OpCode::DIV:
				ai = new ArithIns(ArithIns::ArithInsType::DIV, arg1, arg2, dest);
				useDestReg = true;
				break;
			case OpNode::OpCode::MULT:
				ai = new ArithIns(ArithIns::ArithInsType::MUL, arg1, arg2, dest);
				useDestReg = true;
				break;
			case OpNode::OpCode::MOD:
				ai = new ArithIns(ArithIns::ArithInsType::MOD, arg1, arg2, dest);
				useDestReg = true;
				break;
			case OpNode::OpCode::BITAND:
				ai = new ArithIns(ArithIns::ArithInsType::AND, arg1, arg2, dest);
				useDestReg = true;
				break;
			case OpNode::OpCode::BITOR:
				ai = new ArithIns(ArithIns::ArithInsType::OR, arg1, arg2, dest);
				useDestReg = true;
				break;
			case OpNode::OpCode::BITXOR:
				ai = new ArithIns(ArithIns::ArithInsType::XOR, arg1, arg2, dest);
				useDestReg = true;
				break;
			case OpNode::OpCode::BITNOT:
				// TODO
				// BITNOT can be implementated using XOR arg1 1111
				// arg2 overriden here since it is a constant
				arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, -1);
				ai = new ArithIns(ArithIns::ArithInsType::XOR, arg1, arg2, dest);
				useDestReg = true;
				break;
				
			case OpNode::OpCode::SHL:
				// TODO
				// SHL can be implementated using repeat 'MUL arg1 2' arg2 times
				// step 1: store the 1st arg in a temp register
				tmpReg3 = op1->getTmpReg();
				Instruction::Operand* dest_ = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg3);
				mvi = new MovIns(MovIns::MovInsType::MOVI, arg1, dest_);
				
				// step 2: store the 2nd arg also to a temp register
				tmpReg4 = op2->getTmpReg();
				dest1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg4);
				mvi1 = new MovIns(MovIns::MovInsType::MOVI, arg2, dest1);
				
				// add label
				std::string newLabel = Instruction::getLabel();
				arg_jump = new Instruction::Operand(Instruction::Operand::STR_CONST, newLabel);
            				
				// step 3 : check if 2nd arg became/is 0
				
				Instruction::Operand arg = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST,0);
				
				// construct an Instruction RELOP that compares arg2 with 0.
				RelOpIns* relop = new RelOpIns(RelOpIns::RelOpInsType::EQ, dest1, arg);
				JumpIns* jmpi = new JumpIns(JumpIns::JumpInsType::JMPC, relop, arg_jump);
				
				// step 4: multiply arg1 with 2
				arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 2);
				dest2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg5); // another temp register
				a1 = new ArithIns(ArithIns::ArithInsType::MUL, dest_, arg2, dest2);
				
				// step 5: decrement arg2 by 1
				
				Instruction::Operand arg_sub = new Instruction::Operand(Instruction::Operand::INT_CONST,1);
				ArithIns* dec1 = new ArithIns(ArithIns::ArithInsType::SUB, dest1, arg_sub, dest1);
				
				// step 6: jump to beginning of SHL 
				
				JumpIns* jmp = new JumpIns(JumpIns::JumpInsType::JMP, newLabel);
				
				// step 7: move final result into destination
				
				
				MovIns* movFinalIns = new MovIns(MovIns::MovInsType::MOVI, dest2, dest ); 
				
				
				
			case OpNode::OpCode::SHR:
				// TODO
				// SHR can be implementated using repeat 'DIV arg1 2' arg2 times
				
				tmpReg3 = op1->getTmpReg();
				Instruction::Operand* dest_ = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg3);
				mvi = new MovIns(MovIns::MovInsType::MOVI, arg1, dest_);
				
				// step 2: store the 2nd arg also to a temp register
				tmpReg4 = op2->getTmpReg();
				dest1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg4);
				mvi1 = new MovIns(MovIns::MovInsType::MOVI, arg2, dest1);
				
				// add label
				std::string newLabel = Instruction::getLabel();
				arg_jump = new Instruction::Operand(Instruction::Operand::STR_CONST, newLabel);
            				
				// step 3 : check if 2nd arg became/is 0
				
				Instruction::Operand arg = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST,0);
				
				// construct an Instruction RELOP that compares arg2 with 0.
				RelOpIns* relop = new RelOpIns(RelOpIns::RelOpInsType::EQ, dest1, arg);
				JumpIns* jmpi = new JumpIns(JumpIns::JumpInsType::JMPC, relop, arg_jump);
				
				// step 4: multiply arg1 with 2
				arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 2);
				dest2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg5); // another temp register
				a1 = new ArithIns(ArithIns::ArithInsType::DIV, dest_, arg2, dest2);
				
				// step 5: decrement arg2 by 1
				
				Instruction::Operand arg_sub = new Instruction::Operand(Instruction::Operand::INT_CONST,1);
				ArithIns* dec1 = new ArithIns(ArithIns::ArithInsType::SUB, dest1, arg_sub, dest1);
				
				// step 6: jump to beginning of SHL 
				
				JumpIns* jmp = new JumpIns(JumpIns::JumpInsType::JMP, arg_jump);
				
				// step 7: move final result into destination
				
				
				MovIns* movFinalIns = new MovIns(MovIns::MovInsType::MOVI, dest2, dest ); 
			case OpNode::OpCode::PRINT:
				// TODO
				// PRINT is not supported from lex parser ast type check to now
				useDestReg = true;
				break;
			case OpNode::OpCode::EQ:
				roi = new RelOpIns(RelOpIns::RelOpInsType::EQ, arg1, arg2);
				useDestReg = false;
				break;
			case OpNode::OpCode::NE:
				roi = new RelOpIns(RelOpIns::RelOpInsType::NE, arg1, arg2);
				useDestReg = false;
				break;
			case OpNode::OpCode::GT:
				roi = new RelOpIns(RelOpIns::RelOpInsType::GT, arg1, arg2);
				useDestReg = false;
				break;
			case OpNode::OpCode::GE:
				roi = new RelOpIns(RelOpIns::RelOpInsType::GE, arg1, arg2);
				useDestReg = false;
				break;
			// NOTE! we don't have FLT Instruction! use FGT instead
			case OpNode::OpCode::LT:
				roi = new RelOpIns(RelOpIns::RelOpInsType::GT, arg2, arg1);
				useDestReg = false;
				break;
			case OpNode::OpCode::LE:
				roi = new RelOpIns(RelOpIns::RelOpInsType::GE, arg2, arg1);
				useDestReg = false;
				break;
			default:
				break;
		}
		// TODO
		// handel other cases
		if (ai != NULL)
			code += ai->toString();
		if (roi != NULL)
			code += roi->toString();
		if (!useDestReg) {
			rm->releaseReg(destReg, false);
		}
		// TODO
		// release op1, op2 when necessary
	}

	return code;
}

PrimitivePatNode::PrimitivePatNode(EventEntry* ee, vector<VariableEntry*>* params,
				   ExprNode* c,
				   int line, int column, string file):
	BasePatNode(BasePatNode::PatNodeKind::PRIMITIVE, line, column, file) {
	ee_ = ee;
	params_ = params;
	// TODO we only consider cond_ here. condition_ seems useless so far.
	cond_ = c;
	condition_ = nullptr;
}

bool PrimitivePatNode::hasStar() const{
	return false;
}

bool PrimitivePatNode::hasSeqOps() const{
	return false;
}

bool PrimitivePatNode::hasNeg() const{
	return false;
}

bool PrimitivePatNode::hasAnyOrOther() const{
	// TODO what does this function mean? Any means "any" event ???
	if (ee_->name().compare(string("any")) == 0)
		return true;
	else
		return false;
}

const Type* PrimitivePatNode::typeCheck() {
	// TODO
	// Acutal parameters to an event will have the types given by the declaration for that event
	// we don't need to check it now, they will only be read in during runtime.
	// first check event paramters numbers
	EventEntry *ee = this->ee_;
	const Type *type = ee->EventEntry::type();
	const vector<const Type*>* argTypes = type->Type::argTypes();
	vector<VariableEntry*>* params = this->params();
	char msg[MAXBUF];

	if (argTypes == NULL) {
		if (params == NULL) {
			// good thing
		} else {
			// bad thing
			sprintf(msg,"Event %s require %d arguments", ee->SymTabEntry::name().c_str(), 0);
			errMsg(msg, this->line(), this->column(), this->file().c_str());
		}
	} else {
		if (params == NULL) {
			// bad thing
			sprintf(msg,"Event %s require %d arguments", ee->SymTabEntry::name().c_str(), argTypes->size());
			errMsg(msg, this->line(), this->column(), this->file().c_str());
		} else if(params->size() != argTypes->size()){
			// bad thing
			sprintf(msg,"Event %s require %d arguments", ee->SymTabEntry::name().c_str(), argTypes->size());
			errMsg(msg, this->line(), this->column(), this->file().c_str());
		}
	} 
	return NULL;
}

void PrimitivePatNode::typePrint(ostream& os, int indent) const{
	// TODO
	os << "(";
	cout << this->event()->name();
	if (this->event()->name().compare("any") != 0) {
		os << "(";
		if (this->params() != NULL) {
			auto it = this->params()->begin();
			int i, size;
			size = this->params()->size();
			for(i = 0;it != this->params()->end(); ++it, ++i) {
				(*it)->typePrint(os, 0);
				if (i < size - 1)
					os<<", ";
			}
		}
		os << ")";
	}

	if (this->cond() != NULL) {
		os << "|";
		this->cond()->typePrint(os, indent);
	}

	os << ")";

}

void PrimitivePatNode::print(ostream& os, int indent) const{
	// Add your code
	os << "(";
	cout << this->event()->name();
	if (this->event()->name().compare("any") != 0) {
		os << "(";
		if (this->params() != NULL) {
			auto it = this->params()->begin();
			int i, size;
			size = this->params()->size();
			for(i = 0;it != this->params()->end(); ++it, ++i) {
				(*it)->print(os, 0);
				if (i < size - 1)
					os<<", ";
			}
		}
		os << ")";
	}

	if (this->cond() != NULL) {
		os << "|";
		this->cond()->print(os, indent);
	}

	os << ")";

}

PatNode::PatNode(PatNodeKind pk, BasePatNode *p1, BasePatNode *p2, int line, int column, string file):
	BasePatNode(pk, line, column, file) {

	pat1_ = p1;
	pat2_ = p2;
}

bool PatNode::hasNeg() const{
	bool p1 = false, p2 = false;
	if (kind() == BasePatNode::PatNodeKind::NEG)
		return true;
	else {
		if (pat1() != nullptr)
			p1 = pat1()->hasNeg();
		if (pat2() != nullptr)
			p2 = pat2()->hasNeg();

		return p1 | p2;
	}
}

bool PatNode::hasStar() const{
	bool p1 = false, p2 = false;

	if (kind() == BasePatNode::PatNodeKind::STAR)
		return true;
	else {
		if (pat1() != nullptr)
			p1 = pat1()->hasSeqOps();
		if (pat2() != nullptr)
			p2 = pat2()->hasSeqOps();

		return p1 | p2;
	}
}

bool PatNode::hasSeqOps() const{
	bool p1 = false, p2 = false;

	if (kind() == BasePatNode::PatNodeKind::SEQ)
		return true;
	else {
		if (pat1() != nullptr)
			p1 = pat1()->hasSeqOps();
		if (pat2() != nullptr)
			p2 = pat2()->hasSeqOps();

		return p1 | p2;
	}
}

// TODO don't understand hasAnyOrOther() means? Any event and sth like STAR ?
// Only consider any here.
bool PatNode::hasAnyOrOther() const{
	bool p1 = false, p2 = false;

	if (pat1() != nullptr)
		p1 = pat1()->hasAnyOrOther();
	if (pat2() != nullptr)
		p2 = pat2()->hasAnyOrOther();

	return p1 | p2;
}

const Type* PatNode::typeCheck() {
	BasePatNode::PatNodeKind pnk = this->kind();
	if (pnk == BasePatNode::PatNodeKind::NEG) {
		this->pat1()->typeCheck();
		if (this->pat1()->hasSeqOps() || this->pat1()->hasStar())
			errMsg("Only simple patterns without `.', `*', and `!' operatorscan be negated", this->line(), this->column(), this->file().c_str());
	} else {
		if (this->pat1() != NULL)
			this->pat1()->typeCheck();
		if (this->pat2() != NULL)
			this->pat2()->typeCheck();
	}

	return NULL;
}

void PatNode::typePrint(ostream& os, int indent) const{
	// TODO
	// Add your code
	os << "(";
	switch(this->kind()) {
		case BasePatNode::PatNodeKind::NEG :
			os << "!";
			this->pat1()->typePrint(os, indent);
			break;
		case BasePatNode::PatNodeKind::STAR :
			this->pat1()->typePrint(os, indent);
			os << " **";
			break;
		case BasePatNode::PatNodeKind::OR :
			this->pat1()->typePrint(os, indent);
			os << " \\/ ";
			this->pat2()->typePrint(os, indent);
			break;
		case BasePatNode::PatNodeKind::SEQ :
			this->pat1()->typePrint(os, indent);
			os << " : ";
			this->pat2()->typePrint(os, indent);
			break;
		default:
			os << "ERROR: undefined PatNodeKind for PatNode"<<endl;
			break;
	}
	os << ")";
}

void PatNode::print(ostream& os, int indent) const{
	// Add your code
	os << "(";
	switch(this->kind()) {
		case BasePatNode::PatNodeKind::NEG :
			os << "!";
			this->pat1()->print(os, indent);
			break;
		case BasePatNode::PatNodeKind::STAR :
			this->pat1()->print(os, indent);
			os << " **";
			break;
		case BasePatNode::PatNodeKind::OR :
			this->pat1()->print(os, indent);
			os << " \\/ ";
			this->pat2()->print(os, indent);
			break;
		case BasePatNode::PatNodeKind::SEQ :
			this->pat1()->print(os, indent);
			os << " : ";
			this->pat2()->print(os, indent);
			break;
		default:
			os << "ERROR: undefined PatNodeKind for PatNode"<<endl;
			break;
	}
	os << ")";
}

const Type* ValueNode::typeCheck() {
	const Value* val = this->value();
	if (val != NULL) {
		//std::cout << "val != NULL" << endl;
		//this->print(std::cout, 0);
		//std::cout<<endl;
		return this->type();
	 } else {
		//std::cout << "val == NULL" << endl;
		return new Type::Type(Type::TypeTag::VOID);
	}
}

void ValueNode::print(ostream& os, int indent) const{
	// Add your code
	this->value()->print(os, indent);
}

void ValueNode::typePrint(ostream& os, int indent) const{
	// TODO
	const Type *ctype = this->coercedType();
	if (ctype != NULL)
		os << "(" << Type::name(ctype->tag()) << ")" <<" ";
	this->value()->typePrint(os, indent);
}

string ValueNode::codeGen(RegManager *rm) {
	string code;
	int tmpReg1;
	const Value *val;
	bool isFloat = false;
	bool bVal;
	bool error = false;
	int iVal;
	string sVal;
	double dVal;
	const Type *ctype = this->coercedType();
	Type::TypeTag tag;
	Instruction::Operand *arg1, *arg2;
	MovIns *mi;

	val = this->value();

	if (ctype == NULL)
		ctype = this->type();

	tag = ctype->tag();
	switch(tag) {
		case Type::TypeTag::BOOL:
			bVal = val->bval();
			if (bVal == true) {
				arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 1);
			} else {
				arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 0);
			}
			break;
		case Type::TypeTag::BYTE:
		case Type::TypeTag::UINT:
		case Type::TypeTag::INT:
			iVal = val->ival();
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, iVal);
			break;
		case Type::TypeTag::STRING:
			sVal = val->sval();
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::STR_CONST, sVal);
			break;
		case Type::TypeTag::DOUBLE:
			dVal = val->dval();
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_CONST, (float)dVal);
			isFloat = true;
			break;
		default:
			std::cout<<"ERROR:Unknown type for ValueNode!"<<std::endl;
			error = true;
			break;
	}

	switch(tag) {
		case Type::TypeTag::BOOL:
		case Type::TypeTag::BYTE:
		case Type::TypeTag::UINT:
		case Type::TypeTag::INT:
			// alloc a caller-save, integer reg
			tmpReg1 = rm->getReg(true, false);
			arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
			mi = new MovIns(MovIns::MovInsType::MOVI, arg1, arg2);
			break;
		case Type::TypeTag::STRING:
			// alloc a caller-save, integer reg
			tmpReg1 = rm->getReg(true, false);
			arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
			mi = new MovIns(MovIns::MovInsType::MOVS, arg1, arg2);
			break;
		case Type::TypeTag::DOUBLE:
			// alloc a caller-save, float reg
			tmpReg1 = rm->getReg(true, true);
			arg2 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, tmpReg1);
			mi = new MovIns(MovIns::MovInsType::MOVF, arg1, arg2);
			break;
		default:
			std::cout<<"ERROR:Unknown type for ValueNode!"<<std::endl;
			error = true;
			break;
	}

	if (error)
		return NULL;

	this->setTmpReg(tmpReg1);
	this->setIsRecyclable(true);
	this->setIsFloat(isFloat);

	code += mi->toString();

	return code;
}

RefExprNode::RefExprNode(string ext, const SymTabEntry* ste,
	      int line, int column, string file):
	ExprNode(ExprNode::ExprNodeType::REF_EXPR_NODE, NULL, line, column, file){
	// TODO
	ext_ = ext;
	sym_ = ste;
}

void RefExprNode::print(ostream& os, int indent) const{
	// Add your code
	os << this->ext();
}

const Type* RefExprNode::typeCheck() {
	// TODO
	return this->symTabEntry()->type();
}

void RefExprNode::typePrint(ostream& os, int indent) const{
	// TODO
	//os << this->ext();
	const Type *ctype = this->coercedType();
	if (ctype != NULL)
		os << "(" << Type::name(ctype->tag()) << ")" <<" ";
	os << Type::name(this->symTabEntry()->type()->tag());
}

// TODO don't know what kind of copy here it is, should we copy ext_???
RefExprNode::RefExprNode(const RefExprNode& ren):
	ExprNode(ExprNode::ExprNodeType::REF_EXPR_NODE){
	ext_ = ren.ext();
	sym_ = ren.symTabEntry();
}

string RefExprNode::codeGen(RegManager *rm) {
	// for local/global variable : load var into caller-save tmp reg
	// for param variable: move value from param var reg to caller saved tmp reg(in case coerced type)
	string code;
	int tmpReg1, tmpReg2, paramReg, offset;
	bool isFloat = false;
	const Type *ctype = this->coercedType();
	const Type *type = this->symTabEntry()->type();
	Instruction::Operand *arg1, *arg2;
	MovIns *mi;
	ArithIns *ai;

	// for parameter var, just set reg and isFloat, and return. param already loaded into reg when init function
 	if (((VariableEntry *)(this->symTabEntry()))->varKind() == VariableEntry::VarKind::PARAM_VAR &&
		ctype == NULL) {
		this->setIsFloat(((VariableEntry *)(this->symTabEntry()))->isFloat());
		this->setTmpReg(((VariableEntry *)(this->symTabEntry()))->getTmpReg());
		// for parameter var, the default reg is callee saved, not recyclable
		this->setIsRecyclable(false);
		return NULL;
	} else if (((VariableEntry *)(this->symTabEntry()))->varKind() == VariableEntry::VarKind::PARAM_VAR &&
		ctype != NULL) {

		// only when we need to coerce type from non-float to float, we need extra code
		if((ctype->tag() == Type::TypeTag::DOUBLE) && (type->tag() != Type::TypeTag::DOUBLE)) {
			// inst move int to float: MOVIF intreg freg
			paramReg = ((VariableEntry *)(this->symTabEntry()))->getTmpReg();
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, paramReg);
			// alloc a caller-save, float reg
			tmpReg1 = rm->getReg(true, true);
			arg2 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, tmpReg1);
			mi = new MovIns(MovIns::MovInsType::MOVIF, arg1, arg2);
			code += mi->toString();
			this->setIsFloat(true);
			this->setTmpReg(tmpReg1);
			// after move the param reg value out to float reg, it is recyclable again
			this->setIsRecyclable(true);
			return code;
		} else {
			// although coerce type is required, no need to coerce type at inst level
			this->setIsFloat(((VariableEntry *)(this->symTabEntry()))->isFloat());
			this->setTmpReg(((VariableEntry *)(this->symTabEntry()))->getTmpReg());
			// for parameter var, the default reg is callee saved, not recyclable
			this->setIsRecyclable(false);
			return NULL;
		}
	}

	// get coerced type of this exprNode
	if (ctype == NULL)
		ctype = this->symTabEntry()->type();

	if(ctype->tag() == Type::TypeTag::DOUBLE)
		isFloat = true;
	else
		isFloat = false;

	// set isFloat for this exprNode
	this->setIsFloat(isFloat);

	// inst1: MOVI offset tmpReg1
	offset = ((VariableEntry *)(this->symTabEntry()))->offSet();
	// alloc a caller-save, interger reg
	tmpReg1 = rm->getReg(true, false);
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, offset);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
	mi = new MovIns(MovIns::MovInsType::MOVI, arg1, arg2);
	code += mi->toString();

	// if variable is local variable, offset need to be recalculated
	// inst2: SUB BP tmpReg1 tmpReg1
	if (((VariableEntry *)(this->symTabEntry()))->varKind() == VariableEntry::VarKind::LOCAL_VAR) {
		arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_BP);
		arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
		ai = new ArithIns(ArithIns::ArithInsType::SUB, arg1, arg2, arg2);
		code += ai->toString();
	}

	// inst3: LDI/F tmpReg1 tmpReg2 
	if (isFloat) {
		// alloc a caller-save, float reg
		tmpReg2 = rm->getReg(true, true);
		arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
		arg2 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, tmpReg2);
		mi = new MovIns(MovIns::MovInsType::LDF, arg1, arg2);
	} else {
		// alloc a caller-save, integer reg
		tmpReg2 = rm->getReg(true, false);
		arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
		arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg2);
		mi = new MovIns(MovIns::MovInsType::LDI, arg1, arg2);
	}

	rm->releaseReg(tmpReg1, false);
	code += mi->toString();

	// set tmp reg for exprNode
	this->setTmpReg(tmpReg2);
	this->setIsRecyclable(true);

	return code;
}

InvocationNode::InvocationNode(const SymTabEntry *ste, vector<ExprNode*>* param,
		 int line, int column, string file):
	ExprNode(ExprNode::ExprNodeType::INV_NODE) {
	ste_ = ste;
	params_ = param;
}

InvocationNode::InvocationNode(const InvocationNode& in):
	ExprNode(ExprNode::ExprNodeType::INV_NODE) {
	ste_ = in.symTabEntry();
	params_ = (vector <ExprNode*> *)in.params();
}

void InvocationNode::print(ostream& os, int indent) const{
	// Add your code
	os << this->symTabEntry()->name();
	os << "(";
	if (this->params() != NULL) {
		auto it = this->params()->begin();
		for(;it != this->params()->end(); ++it) {
			(*it)->print(os, 0);
			if ((it + 1) != this->params()->end())
				os<<", ";
		}
	}
	os << ")";
}

const Type* InvocationNode::typeCheck() {
	// TODO check parametere
	const SymTabEntry *ste = this->symTabEntry();
	const Type *ftype = ste->type();
	const Type *formalType,*actualType;
	const vector<const Type*>* argTypes = ftype->argTypes();
	vector<ExprNode*>* params = this->params();
	char msg[MAXBUF];

	int i = 0, expSize = 0, actSize = 0;
	if (argTypes != NULL)
		expSize = argTypes->size();
	if (params != NULL)
		actSize = params->size();

	if (actSize != expSize) {
		sprintf(msg,"%d arguments expected for %s", expSize, ste->SymTabEntry::name().c_str());
		errMsg(msg, this->line(), this->column(), this->file().c_str());

		return ftype->retType();
	} else if (actSize == 0) {
		return ftype->retType();
	}

	auto it = argTypes->begin();
	auto pa = params->begin();
	for (; (it != argTypes->end()) && (pa != params->end()); ++it, ++pa) {
		++i;
		formalType = (*it);
		actualType = (*pa)->typeCheck();
		if (formalType == NULL || actualType == NULL) {
			sprintf(msg,"Type mismatch for argument %d to %s", i, ste->SymTabEntry::name().c_str());
			errMsg(msg, this->line(), this->column(), this->file().c_str());
		} else if (formalType->tag() == actualType->tag()) {
			// type match exactly, do nothing
			if (formalType->tag() == Type::TypeTag::CLASS) {
				const SymTabEntry *lste, *rste;
				lste = formalType->typeDesc();
				rste = actualType->typeDesc();
				if (lste != rste) {
					sprintf(msg,"Type mismatch for argument %d to %s", i, ste->SymTabEntry::name().c_str());
					errMsg(msg, this->line(), this->column(), this->file().c_str());
				}
			}
		} else {
			if (formalType->isSubType(actualType)) {
				// coerced type needed
				(*pa)->coercedType(formalType);
			} else {
				sprintf(msg,"Type mismatch for argument %d to %s", i, ste->SymTabEntry::name().c_str());
				errMsg(msg, this->line(), this->column(), this->file().c_str());
				//errMsg("type error: function parameter type not match");
			}
		}
	}

	return ftype->retType();
}

void InvocationNode::typePrint(ostream& os, int indent) const{
	// TODO
	os << this->symTabEntry()->name();
	os << "(";
	if (this->params() != NULL) {
		auto it = this->params()->begin();
		for(;it != this->params()->end(); ++it) {
			(*it)->typePrint(os, 0);
			if ((it + 1) != this->params()->end())
				os<<", ";
		}
	}
	os << ")";
}

string InvocationNode::codeGen(RegManager *rm) {
	string code;
	int tmpReg1, resReg;
	bool isFloat = false;
	const SymTabEntry *ste = this->symTabEntry();
	const Type *ftype = ste->type();
	const Type *ctype = this->coercedType();
	const Type *retType = ftype->retType();
	ArithIns *ai;
	MovIns *mi;
	JumpIns *ji;
	Instruction::Operand *arg1, *arg2, *dest;
	const vector<ExprNode *> * params = this->params();
	ExprNode *param;
	int arity = params->size();
	int i;

	// prepare parameters
	for( i = 0; i < arity; i++) {
		param = this->param(i);
		code += param->codeGen(rm);
	}

	// before function call, push caller-save register
	code +=  rm->pushCallerSaveRegs();

	// pass parameters via stack in reverse order
	for( i = arity - 1; i>=0; i--) {
		param = this->param(i);
		//inst1: STI/F tmpReg1 SP
		isFloat = param->isFloat();
		tmpReg1 = param->getTmpReg();

		if (isFloat) {
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, tmpReg1);
			arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
			mi = new MovIns(MovIns::MovInsType::STF, arg1, arg2);
		} else {
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
			arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
			mi = new MovIns(MovIns::MovInsType::STI, arg1, arg2);
		}

		code += mi->toString();

		// check whether recyclable, and recycle tmpReg1
		if(param->isRecyclable())
			rm->releaseReg(tmpReg1,isFloat);

		//inst2: SUB SP 1 SP
		arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
		arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 1);
		dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
		ai = new ArithIns(ArithIns::ArithInsType::SUB, arg1, arg2, dest);

		code += ai->toString();
	}

	// push ret_addr onto stack
	// get ret_addr label, inst1: MOVL label tmpReg1
	string label = AstNode::getLabel();

	// alloc a caller-save, int reg
	tmpReg1 = rm->getReg(true, false);
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::STR_CONST, label);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
	mi = new MovIns(MovIns::MovInsType::MOVL, arg1, arg2);
	code += mi->toString();

	// inst: STI tmpReg1 SP
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	mi = new MovIns(MovIns::MovInsType::STI, arg1, arg2);
	code += mi->toString();

	// inst: SUB SP 1 SP
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 1);
	dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	ai = new ArithIns(ArithIns::ArithInsType::SUB, arg1, arg2, dest);
	code += ai->toString();

	// it's safe to release tmpReg1 here.
	rm->releaseReg(tmpReg1, false);

	// do the function call
	// JMP func_label
	string func_label = ((FunctionEntry *)ste)->getFuncLabel();
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::STR_CONST, func_label);
	ji = new JumpIns(JumpIns::JumpInsType::JMP, NULL, arg1);
	code += ji->toString();

	// add label for next instruction
	code += label + ": ";

	// pop out func params
	if (arity > 0) {
		//inst2: SUB SP 1 SP
		arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
		arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, arity);
		dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
		ai = new ArithIns(ArithIns::ArithInsType::ADD, arg1, arg2, dest);
		code += ai->toString();
	}

	// after function call recover caller-save registes
	code +=  rm->popCallerSaveRegs();

	// if return type is not VOID keep return value
	if (retType->tag() != Type::TypeTag::VOID) {
		// need to store retVal in tmp reg
		if (retType->tag() != Type::TypeTag::DOUBLE) {
			if (ctype == NULL || ctype ->tag() != Type::TypeTag::DOUBLE) {
				// alloc a caller-save, int reg
				resReg = rm->getReg(true, false);

				// inst1: MOVI REG_RV resReg
				arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_RV);
				arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
				mi = new MovIns(MovIns::MovInsType::MOVI, arg1, arg2);
				code += mi->toString();
				isFloat = false;
			} else {
				// alloc a caller-save, float reg
				resReg = rm->getReg(true, true);

				// inst1: MOVIF REG_RV resReg
				arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_RV);
				arg2 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, tmpReg1);
				mi = new MovIns(MovIns::MovInsType::MOVIF, arg1, arg2);
				code += mi->toString();
				isFloat = true;
			}
		} else {
				// alloc a caller-save, float reg
				resReg = rm->getReg(true, true);

				// inst1: MOVF REG_RV resReg
				arg1 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, REG_RV);
				arg2 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, tmpReg1);
				mi = new MovIns(MovIns::MovInsType::MOVF, arg1, arg2);
				code += mi->toString();
				isFloat = true;
		}

		this->setTmpReg(resReg);
		this->setIsFloat(isFloat);
		this->setIsRecyclable(true);

		code += mi->toString();
	}

	return code;
}

IfNode::IfNode(ExprNode* cond, StmtNode* thenStmt, 
		 StmtNode* elseStmt, int line, int column, string file):
	StmtNode(StmtNode::StmtNodeKind::IF, line, column, file) {
	cond_ = cond;
	then_ = thenStmt;
	else_ = elseStmt;
}

const Type* IfNode::typeCheck() {
	const Type *type;
	ExprNode *en = this->cond();

	type = en->typeCheck();
	if (! (type->isBool(type->tag()))) {
		errMsg("Boolean argument expected", en->line(), en->column(), en->file().c_str());
	}

	if (then_ != NULL)
		then_->typeCheck();
	if (else_ != NULL)
		else_->typeCheck();

	// nobody care about the return type here.
	return NULL;
}

void IfNode::typePrint(ostream& os, int indent) const{
	// TODO
	prtSpace(os, indent);
	os << "if";
	os << "(";
	this->cond()->typePrint(os, indent);
	os << ")";
	if (this->thenStmt() == NULL) {
		// do nothing
	} else if ((this->thenStmt()->stmtNodeKind() != StmtNode::StmtNodeKind::COMPOUND)) { 
		prtSpace(os, indent + 2);
		this->thenStmt()->typePrint(os, indent + 2);
		os << ";";
		os << endl;
	} else {
		
		(this->thenStmt())->typePrint(os, indent + 2);
	}

	if (this->elseStmt() != NULL) {
		prtSpace(os, indent);
		os << "else "; 

		if ((this->elseStmt()->stmtNodeKind() != StmtNode::StmtNodeKind::COMPOUND)){
			prtSpace(os, indent + 2);
			this->elseStmt()->typePrint(os, indent + 2);
			os << ";";
			os << endl;
		} else {
			(this->elseStmt())->typePrint(os, indent + 2);
		}
	}
}

void IfNode::print(ostream& os, int indent) const{
	// Add your code
	prtSpace(os, indent);
	os << "if";
	os << "(";
	this->cond()->print(os, indent);
	os << ")";
	if (this->thenStmt() == NULL) {
		// do nothing
	} else if ((this->thenStmt()->stmtNodeKind() != StmtNode::StmtNodeKind::COMPOUND)) { 
		prtSpace(os, indent + 2);
		this->thenStmt()->print(os, indent + 2);
		os << ";";
		os << endl;
	} else {
		
		(this->thenStmt())->print(os, indent + 2);
/*
		os << step;
		os << "{";
		os << endl; 
		((CompoundStmtNode *)(this->thenStmt()))->printWithoutBraces(os, indent + 2);
		prtSpace(os, indent);
		os << "}";
		os << endl; 
*/	}

	if (this->elseStmt() != NULL) {
		prtSpace(os, indent);
		os << "else "; 

		if ((this->elseStmt()->stmtNodeKind() != StmtNode::StmtNodeKind::COMPOUND)){
			prtSpace(os, indent + 2);
			this->elseStmt()->print(os, indent + 2);
			os << ";";
			os << endl;
		} else {
			(this->elseStmt())->print(os, indent + 2);
		}
	}
}

string IfNode::codeGen(RegManager *rm) {
	string code;
	string elseCode;
	string thenCode;
	string thenLabel;
	string afterIfLabel;
	int tmpReg1;
	RelOpIns *roi;
	JumpIns *ji;
	Instruction::Operand *arg1, *arg2;

	// inst1: JMPC EQ tmpReg1 1 thenLabel
	//        elseCode
	//        JMP after_if/then
	// then:
	//	  thenCode
	// after_if:
	//        ...

	// cond_ should be integer reg
	tmpReg1 = cond_->getTmpReg();
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 1);
	roi = new RelOpIns(RelOpIns::RelOpInsType::EQ, arg1, arg2);

	// generate thenStmt label
	// inst1: JMPC EQ tmpReg1 1 thenLabel
	thenLabel = AstNode::getLabel();
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::STR_CONST, thenLabel);
	ji = new JumpIns(JumpIns::JumpInsType::JMPC, roi, arg1);
	code += ji->toString();

	// it's safe to release cond_->tmpReg
	if (cond_->isRecyclable())
		rm->releaseReg(tmpReg1, false);

	// elseStmtCode
	if (this->elseStmt() != NULL) {
		elseCode = this->elseStmt()->codeGen(rm);
	}

	thenCode += thenLabel + ": ";
	if (this->thenStmt() != NULL) {
		// JMP after_if
		afterIfLabel = AstNode::getLabel();
		arg1 = new Instruction::Operand(Instruction::Operand::OperandType::STR_CONST, afterIfLabel);
		ji = new JumpIns(JumpIns::JumpInsType::JMP, NULL, arg1);
		elseCode += ji->toString();
		thenCode += this->thenStmt()->codeGen(rm);
		code += elseCode + thenCode + afterIfLabel + ": ";
	} else {
		// thenStmt == NULL, so thenLabel == afterIfLabel
		arg1 = new Instruction::Operand(Instruction::Operand::OperandType::STR_CONST, thenLabel);
		ji = new JumpIns(JumpIns::JumpInsType::JMP, NULL, arg1);
		elseCode += ji->toString();
		code += elseCode + thenCode;
	}

	return code;
}

RuleNode::RuleNode(BlockEntry *re, BasePatNode* pat, StmtNode* reaction, 
	int line, int column, string file):
	AstNode(AstNode::NodeType::RULE_NODE, line, column, file) {
	rste_ = re;
	pat_ = pat;
	reaction_ = reaction;
	ruleLabel_ = AstNode::getLabel();
}

const Type* RuleNode::typeCheck() {
	// TODO we use a global variable to indicate we are current in the scope of rule
	inRuleScope = true;

	// pat check
	pat_->typeCheck();

	// statement type check
	reaction_->typeCheck();

	inRuleScope = false;

	return NULL;
}

void RuleNode::typePrint(ostream& os, int indent) const{
	// TODO
	prtSpace(os, indent);
	this->pat()->typePrint(os, indent);
	os << "-->";
	this->reaction()->typePrint(os, indent);
	prtSpace(os, indent);
	os << ";;" << endl;
}

void RuleNode::print(ostream& os, int indent) const{
	// Add your code
	prtSpace(os, indent);
	this->pat()->print(os, indent);
	os << "-->";
	this->reaction()->print(os, indent);
	prtSpace(os, indent);
	os << ";;" << endl;
}

/**
 * Every rule has a ruleLabel, control flow jump from main loop to
 * a certain ruleLabel. ruleLabel is stored in ruleNodeMap in memory.
 * Note: We only support single event, with a single as its event name.
 * After enter a rule, we should do the following:
 *	1. read in params and store them onto stack
 *	2. push bp on stack	# Note, no retaddress on stack here, retAddress store in REG_RL
 *	3. move sp to bp 
 *	4. load params into callee save registers # Note, no local vars
 *	5. execute stmt.code
 *	6. pop params
 * 	7. pop bp
 *	8. restore sp
 *	9. JMP REG_RL
 *
 */
string RuleNode::codeGen(RegManager *rm) {
	string code;
	int tmpReg1, tmpReg2;
	bool isFloat = false;
	Instruction::Operand *arg1, *arg2, *dest;
	MovIns *mi;
	ArithIns *ai;
	InputIns *ii;
	JumpIns *ji;
	EventEntry *ee;
	vector<VariableEntry*>* params;
	Type *type;
	vector<Type *> *argTypes;
	Type *argType;
	int arity, i;
	vector<int> *paramRegList = new vector<int>();
	vector<bool> *paramRegAtrList = new vector<bool>();

	// handle ruleLabel 
	string ruleLabel = this->ruleLabel();
	code += ruleLabel + ": "; 

	// 1. read in params and store them onto stack
	// TODO we only support PrimitivePatNode without condition
	ee = ((PrimitivePatNode *)(this->pat_))->event();
	params = ((PrimitivePatNode *)(this->pat_))->params();
	type = ee->type();
	arity = type->arity();
	argTypes = type->argTypes();

	// read in params and push paramters onto stack in order
	for (i = 0; i < arity; i++) {
		argType = (*argTypes)[i];
		if (argType->tag() != Type::TypeTag::DOUBLE)
			isFloat = false;
		else
			isFloat = true;

		// alloc a caller-save, int/float reg
		tmpReg1 = rm->getReg(true, isFloat);

		// inst: INI/INF REG
		if (isFloat) {
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, tmpReg1);
			ii = new InputIns(InputIns::InputInsType::INF, arg1);
		} else {
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
			ii = new InputIns(InputIns::InputInsType::INI, arg1);
		}
		code += ii->toString();


		// inst: STI/F tmpReg1 REG_SP
		if (isFloat) {
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, tmpReg1);
			arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
			mi = new MovIns(MovIns::MovInsType::STF, arg1, arg2);
		} else {
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
			arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
			mi = new MovIns(MovIns::MovInsType::STI, arg1, arg2);
		}
		code += mi->toString();

		// inst: SUB REG_SP 1 REG_SP
		arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
		arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 1);
		dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
		ai = new ArithIns(ArithIns::ArithInsType::SUB, arg1, arg2, dest);
		code += ai->toString();

		rm->releaseReg(tmpReg1, isFloat);
	}

	// 2. push bp on stack # Note, no retaddress on stack here, retAddress store in REG_RL
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_BP);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	mi = new MovIns(MovIns::MovInsType::STI, arg1, arg2);
	code += mi->toString();

	// inst2: SUB REG_SP 1 REG_SP
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 1);
	dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	ai = new ArithIns(ArithIns::ArithInsType::SUB, arg1, arg2, dest);
	code += ai->toString();

 	// 3. move sp to bp 
	// inst1: MOVI REG_SP REG_BP 
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_BP);
	mi = new MovIns(MovIns::MovInsType::MOVI, arg1, arg2);
	code += mi->toString();

 	// load params into callee save registers # Note, no local vars
	// 4. However, we should first save the original value on stack
	for (i = 0; i < arity; i++) {
		argType = (*argTypes)[i];
		if (argType->tag() != Type::TypeTag::DOUBLE)
			isFloat = false;
		else
			isFloat = true;

		// record callee-save regs alloced for param vars
		paramRegAtrList->push_back(isFloat);
		// alloc a callee-save, int/float reg
		tmpReg1 = rm->getReg(false, isFloat);
		paramRegList->push_back(tmpReg1);

		// set param var tmpReg tmpReg1
		((VariableEntry *)(*params)[i])->setTmpReg(tmpReg1);
		((VariableEntry *)(*params)[i])->setIsFloat(isFloat);

		// push allocated reg onto stack to protect their original value

		// inst1: STI tmpReg1 REG_SP
		if (isFloat) {
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, tmpReg1);
			arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
			mi = new MovIns(MovIns::MovInsType::STF, arg1, arg2);
		} else {
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
			arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
			mi = new MovIns(MovIns::MovInsType::STI, arg1, arg2);
		}
		code += mi->toString();

		// inst2: SUB REG_SP 1 REG_SP
		arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
		arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 1);
		dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
		ai = new ArithIns(ArithIns::ArithInsType::SUB, arg1, arg2, dest);
		code += ai->toString();
	}

	// 5. read param_n -> param_1 into regs

	// alloc a caller-save, int reg
	tmpReg2 = rm->getReg(true, false);

	// inst: MOVI REG_BP tmpReg2
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_BP);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg2);
	mi = new MovIns(MovIns::MovInsType::MOVI, arg1, arg2);
	code += mi->toString();

	// point tmpReg2 --> param_n
	// inst: ADD tmpReg2  tmpReg2, skip blank, n_params, BP
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg2);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, arity + 2);
	dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg2);
	ai = new ArithIns(ArithIns::ArithInsType::ADD, arg1, arg2, dest);
	code += ai->toString();

	if (paramRegList->size() > 0) {
		// load params into registers, in reverse way, cause on stack params push in increaing order
		//  param1
		//  param2
		//  param3
		//  bp
		//  p1_reg
		//  p2_reg
		//  p3_reg
		auto pit = paramRegList->rbegin();
		auto pait = paramRegAtrList->rbegin();
		while (pit != paramRegList->rend() && pait != paramRegAtrList->rend()) {
			isFloat = (*pait);
			tmpReg1 = (*pit);

			if (isFloat) {
				// inst: LDF tmpReg2 param_i
				arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg2);
				arg2 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, tmpReg1);
				mi = new MovIns(MovIns::MovInsType::LDF, arg1, arg2);
			} else {
				// inst: LDI tmpReg2 param_i
				arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg2);
				arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
				mi = new MovIns(MovIns::MovInsType::LDI, arg1, arg2);
			}
			code += mi->toString();

			// inst: ADD tmpReg2 1 tmpReg2 
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg2);
			arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 1);
			dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg2);
			ai = new ArithIns(ArithIns::ArithInsType::ADD, arg1, arg2, dest);
			code += ai->toString();

			pit++;
			pait++;
		}
	}

	rm->releaseReg(tmpReg2, false);

	// add rule stmt code
	code += this->reaction()->codeGen(rm);

	// Note: rule has no return stmt, we have to do it ourself!
	// 6. pop params
	// pop param_n -> param_1(callee save)
	if (paramRegList->size() > 0) {
		auto rpit = paramRegList->rbegin();
		auto rpait = paramRegAtrList->rbegin();
		while(rpit != paramRegList->rend() && rpait != paramRegAtrList->rend()) {
			tmpReg1 = (*rpit);
			isFloat = (*rpait);

			// pop inst: ADD REG_SP 1 REG_SP
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
			arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 1);
			dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
			ai = new ArithIns(ArithIns::ArithInsType::ADD, arg1, arg2, dest);
			code += ai->toString();

			if (isFloat) {
				// inst: LDF tmpReg2 param_i
				arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
				arg2 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, tmpReg1);
				mi = new MovIns(MovIns::MovInsType::LDF, arg1, arg2);
			} else {
				// inst: LDI tmpReg2 param_i
				arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
				arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
				mi = new MovIns(MovIns::MovInsType::LDI, arg1, arg2);
			}
			code += mi->toString();

			rpit++;
			rpait++;
		}
	}

 	// 7. pop bp
	// pop inst: ADD REG_SP 1 REG_SP
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 1);
	dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	ai = new ArithIns(ArithIns::ArithInsType::ADD, arg1, arg2, dest);
	code += ai->toString();

	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_BP);
	mi = new MovIns(MovIns::MovInsType::LDI, arg1, arg2);
	code += mi->toString();

	// 8. restore sp
	// pop out event params
	if (arity > 0) {
		//inst2: SUB SP 1 SP
		arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
		arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, arity);
		dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
		ai = new ArithIns(ArithIns::ArithInsType::ADD, arg1, arg2, dest);
		code += ai->toString();
	}

	// 9. JMP REG_RL
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_RL);
	ji = new JumpIns(JumpIns::JumpInsType::JMP, NULL, arg1);
	code += ji->toString();

	return code;
}

/**
 * rule Init code will store rule label in the memory related to event name
 * Note: we only support one letter event name, it will be used as index into
 * memory to store rule label
 * 1. get event name
 * 2. calculate label address
 * 3. store label into memory
 */
string RuleNode::ruleInitCode(RegManager *rm) {
	string code;
	EventEntry *ee;
	MovIns *mi;
	Instruction::Operand *arg1, *arg2;
	char c;
	string eventName, ruleLabel;
	int labelAddr;
	int tmpReg1, tmpReg2;

	ee = ((PrimitivePatNode *)(this->pat_))->event();
	eventName = ee->name();
	c = eventName.at(0);
	labelAddr = EVENT_MAP_START + (int)c; 
	ruleLabel = this->ruleLabel();

	// alloc a caller-save, integer reg
	tmpReg1 = rm->getReg(true, false);

	// inst: MOVI labelAddr tmpReg1 
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, labelAddr);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
	mi = new MovIns(MovIns::MovInsType::MOVI, arg1, arg2);
	code += mi->toString();

	// alloc a caller-save, integer reg
	tmpReg2 = rm->getReg(true, false);

	// inst: MOVL ruleLabel tmpReg2
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::STR_CONST, ruleLabel);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg2);
	mi = new MovIns(MovIns::MovInsType::MOVL, arg1, arg2);
	code += mi->toString();

	// inst: STI  tmpReg2 tmpReg1
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg2);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
	mi = new MovIns(MovIns::MovInsType::STI, arg1, arg2);
	code += mi->toString();
	
	rm->releaseReg(tmpReg1, false);
	rm->releaseReg(tmpReg2, false);

	return code;
}

void  CompoundStmtNode::typePrintWithoutBraces(ostream& os, int indent) const{
	// Add your code
	if (this->stmts() != NULL) {
		auto it = this->stmts()->begin();
		for (; it != this->stmts()->end(); ++it) {
			if (((*it)->stmtNodeKind() != StmtNode::StmtNodeKind::COMPOUND) ||
				((*it)->stmtNodeKind() != StmtNode::StmtNodeKind::IF)) {
				prtSpace(os, indent);
				(*it)->typePrint(os, indent);
				os << ";" << endl;
			} else
				(*it)->typePrint(os, indent);
		}
	}
}

void  CompoundStmtNode::printWithoutBraces(ostream& os, int indent) const{
	// Add your code
	if (this->stmts() != NULL) {
		auto it = this->stmts()->begin();
		for (; it != this->stmts()->end(); ++it) {
			if (((*it)->stmtNodeKind() != StmtNode::StmtNodeKind::COMPOUND) ||
				((*it)->stmtNodeKind() != StmtNode::StmtNodeKind::IF)) {
				prtSpace(os, indent);
				(*it)->print(os, indent);
				os << ";" << endl;
			} else
				(*it)->print(os, indent);
		}
	}
}

void  CompoundStmtNode::typePrint(ostream& os, int indent) const{
	// TODO
	if (this->stmts()->size() > 0) {
		prtSpace(os, indent);
		os << "{" << endl;
		auto it = this->stmts()->begin();
		for (; it != this->stmts()->end(); ++it) {
			if (((*it)->stmtNodeKind() != StmtNode::StmtNodeKind::COMPOUND) && 
				((*it)->stmtNodeKind() != StmtNode::StmtNodeKind::IF)) {
				prtSpace(os, indent + 2);
				(*it)->typePrint(os, indent + 2);
				os << ";" << endl;
			} else
				(*it)->typePrint(os, indent + 2);
		}
		prtSpace(os, indent);
		os << "};" << endl;
	} else {
		prtSpace(os, indent);
		os << "{};" << endl;
	}
}

const Type* CompoundStmtNode::typeCheck() {
	list<StmtNode*>* stmts = this->stmts();
	if (stmts == NULL)
		return NULL;

	auto it = stmts->begin();

	for (;it != stmts->end(); ++it) {
		if ((*it) != NULL)
			(*it)->typeCheck();
	}

	return NULL;
}

void  CompoundStmtNode::print(ostream& os, int indent) const{
	// Add your code
	if (this->stmts()->size() > 0) {
		prtSpace(os, indent);
		os << "{" << endl;
		auto it = this->stmts()->begin();
		for (; it != this->stmts()->end(); ++it) {
			if (((*it)->stmtNodeKind() != StmtNode::StmtNodeKind::COMPOUND) && 
				((*it)->stmtNodeKind() != StmtNode::StmtNodeKind::IF)) {
				prtSpace(os, indent + 2);
				(*it)->print(os, indent + 2);
				os << ";" << endl;
			} else
				(*it)->print(os, indent + 2);
		}
		prtSpace(os, indent);
		os << "};" << endl;
	} else {
		prtSpace(os, indent);
		os << "{};" << endl;
	}
}

string CompoundStmtNode::codeGen(RegManager *rm) {
	string code;
	list<StmtNode*>* stmts = this->stmts();
	if (stmts == NULL)
		return NULL;

	auto it = stmts->begin();

	for (;it != stmts->end(); ++it) {
		if ((*it) != NULL)
			code += (*it)->codeGen(rm);
	}

	return code;
}

const Type* ReturnStmtNode::typeCheck() {
	const Type * funType = fun_->type();
	const Type * retType = funType->retType();
	const Type * retValType;
	if (expr_ != NULL)
		retValType = expr_->typeCheck();

	if ((retType->Type::tag() == Type::TypeTag::VOID) && (expr_ != NULL)) {
		errMsg("No return value expected for a void function", this->line(), this->column(), this->file().c_str());
	} else if (! retType->isSubType(retValType)) {
		errMsg("Return value incompatible with current function's type", this->line(), this->column(), this->file().c_str());
	}

	return retType;
}

// stack frame:
// +---------+
// | p2      |
// | p1      |
// | ret addr|  REG_RA
// | rbp     |
// +---------+
/**
 * So when return, we should do the following:
 *	0. calcualte ret_val
 *	1. pop REG_RA
 *	2. pop param_1 -> param_n(callee save)
 *	3. restore rsp += #local_vars
 *	4. pop bp
 *	5. mov retval REG_RV
 *	6. jmp REG_RA
 */
string ReturnStmtNode::codeGen(RegManager *rm) {
	string code;
	int tmpReg1;
	bool isFloat = false;
	MovIns *mi;
	ArithIns *ai;
	JumpIns *ji;
	Instruction::Operand *arg1, *arg2, *dest;
	vector<int> * paramRegList = fun_->paramRegList();
	vector<bool> * paramRegAtrList = fun_->paramRegAtrList();
	int localVarNum;

 	// 0. calcualte ret_val
	// inst1: MOVI/F regTmp REG_RV
	if (expr_ != NULL) {
		code += expr_->codeGen(rm);
		tmpReg1 = expr_->getTmpReg();
		isFloat = expr_->isFloat();
		if (isFloat) {
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, tmpReg1);
			arg2 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, REG_RV);
			mi = new MovIns(MovIns::MovInsType::MOVF, arg1, arg2);
		} else {
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
			arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_RV);
			mi = new MovIns(MovIns::MovInsType::MOVI, arg1, arg2);
		}
		code += mi->toString();

		if (expr_->isRecyclable())
			rm->releaseReg(tmpReg1, isFloat);
	}

	// 2. pop param_n -> param_1(callee save)
	if (paramRegList->size() > 0) {
		auto rpit = paramRegList->rbegin();
		auto rpait = paramRegAtrList->rbegin();
		while(rpit != paramRegList->rend() && rpait != paramRegAtrList->rend()) {
			tmpReg1 = (*rpit);
			isFloat = (*rpait);

			// pop inst: ADD REG_SP 1 REG_SP
			arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
			arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 1);
			dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
			ai = new ArithIns(ArithIns::ArithInsType::ADD, arg1, arg2, dest);
			code += ai->toString();

			if (isFloat) {
				// inst: LDF tmpReg2 param_i
				arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
				arg2 = new Instruction::Operand(Instruction::Operand::OperandType::FLOAT_REG, tmpReg1);
				mi = new MovIns(MovIns::MovInsType::LDF, arg1, arg2);
			} else {
				// inst: LDI tmpReg2 param_i
				arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
				arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, tmpReg1);
				mi = new MovIns(MovIns::MovInsType::LDI, arg1, arg2);
			}
			code += mi->toString();

			rpit++;
			rpait++;
		}
	}

	// restore RSP : SP += #local_vars + 1
	localVarNum = fun_->localVarNum();
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, localVarNum);
	dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	ai = new ArithIns(ArithIns::ArithInsType::ADD, arg1, arg2, dest);
	code += ai->toString();

 	// 4. pop bp
	// pop inst: ADD REG_SP 1 REG_SP
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 1);
	dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	ai = new ArithIns(ArithIns::ArithInsType::ADD, arg1, arg2, dest);
	code += ai->toString();

	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_BP);
	mi = new MovIns(MovIns::MovInsType::LDI, arg1, arg2);
	code += mi->toString();

 	// 5. pop ret addr 
	// pop inst: ADD REG_SP 1 REG_SP
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_CONST, 1);
	dest = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	ai = new ArithIns(ArithIns::ArithInsType::ADD, arg1, arg2, dest);
	code += ai->toString();

	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_SP);
	arg2 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_RA);
	mi = new MovIns(MovIns::MovInsType::LDI, arg1, arg2);
	code += mi->toString();

	// jmp to REG_RA
	arg1 = new Instruction::Operand(Instruction::Operand::OperandType::INT_REG, REG_RA);
	ji = new JumpIns(JumpIns::JumpInsType::JMP, NULL, arg1);
	code += ji->toString();

	return code;
}
