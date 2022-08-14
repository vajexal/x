#include "ast.h"
#include "ast_printer.h"
#include "codegen.h"

namespace X {
    std::ostream &operator<<(std::ostream &out, const OpType type) {
        switch (type) {
            case OpType::POST_INC: return out << "++";
            case OpType::POST_DEC: return out << "--";
            case OpType::OR: return out << "||";
            case OpType::AND: return out << "&&";
            case OpType::PLUS: return out << "+";
            case OpType::MINUS: return out << "-";
            case OpType::MUL: return out << "*";
            case OpType::DIV: return out << "/";
            case OpType::NOT: return out << "!";
            case OpType::POW: return out << "**";
            case OpType::EQUAL: return out << "==";
            case OpType::NOT_EQUAL: return out << "!=";
            case OpType::SMALLER: return out << "<";
            case OpType::SMALLER_OR_EQUAL: return out << "<=";
            case OpType::GREATER: return out << ">";
            case OpType::GREATER_OR_EQUAL: return out << ">=";
        }

        return out;
    }

    std::ostream &operator<<(std::ostream &out, const Type type) {
        switch (type) {
            case Type::INT: return out << "int";
            case Type::FLOAT: return out << "float";
            case Type::BOOL: return out << "bool";
            case Type::VOID: return out << "void";
        }

        return out;
    }

    void ExprNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<ExprNode>(this, level); }
    void ScalarNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<ScalarNode>(this, level); }
    void StatementListNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<StatementListNode>(this, level); }
    void UnaryNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<UnaryNode>(this, level); }
    void BinaryNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<BinaryNode>(this, level); }
    void DeclareNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<DeclareNode>(this, level); }
    void AssignNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<AssignNode>(this, level); }
    void VarNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<VarNode>(this, level); }
    void IfNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<IfNode>(this, level); }
    void WhileNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<WhileNode>(this, level); }
    void ArgNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<ArgNode>(this, level); }
    void FnNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<FnNode>(this, level); }
    void FnCallNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<FnCallNode>(this, level); }
    void ReturnNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<ReturnNode>(this, level); }
    void BreakNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<BreakNode>(this, level); }
    void ContinueNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<ContinueNode>(this, level); }
    void CommentNode::print(AstPrinter &astPrinter, int level) { astPrinter.print<CommentNode>(this, level); }

    llvm::Value *ExprNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *ScalarNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *StatementListNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *UnaryNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *BinaryNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *DeclareNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *AssignNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *VarNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *IfNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *WhileNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *ArgNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *FnNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *FnCallNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *ReturnNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *BreakNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *ContinueNode::gen(Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *CommentNode::gen(Codegen &codegen) { return codegen.gen(this); }
}

