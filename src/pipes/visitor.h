#pragma once

#include <concepts>
#include <ranges>

#include "ast.h"

namespace X::Pipes {
    template<std::derived_from<Node> K>
    class Visitor {
    public:
        template<std::derived_from<Node> T>
        T *visit(T *node, const std::function<Node *(K *)> &handler) {
            switch (node->getKind()) {
                case Node::NodeKind::Scalar:
                    break;
                case Node::NodeKind::StatementList: {
                    auto statementListNode = llvm::cast<StatementListNode>(node);

                    std::ranges::transform(
                            statementListNode->children.begin(),
                            statementListNode->children.end(),
                            statementListNode->children.begin(),
                            [&](Node *child) {
                                return visit(child, handler);
                            }
                    );

                    break;
                }
                case Node::NodeKind::Unary: {
                    auto unaryNode = llvm::cast<UnaryNode>(node);

                    unaryNode->expr = visit(unaryNode->expr, handler);

                    break;
                }
                case Node::NodeKind::Binary: {
                    auto binaryNode = llvm::cast<BinaryNode>(node);

                    binaryNode->lhs = visit(binaryNode->lhs, handler);
                    binaryNode->rhs = visit(binaryNode->rhs, handler);

                    break;
                }
                case Node::NodeKind::Decl: {
                    auto declNode = llvm::cast<DeclNode>(node);

                    if (declNode->expr) {
                        declNode->expr = visit(declNode->expr, handler);
                    }

                    break;
                }
                case Node::NodeKind::Assign: {
                    auto assignNode = llvm::cast<AssignNode>(node);

                    assignNode->expr = visit(assignNode->expr, handler);

                    break;
                }
                case Node::NodeKind::Var:
                    break;
                case Node::NodeKind::If: {
                    auto ifNode = llvm::cast<IfNode>(node);

                    ifNode->cond = visit(ifNode->cond, handler);
                    ifNode->thenNode = visit(ifNode->thenNode, handler);

                    if (ifNode->elseNode) {
                        ifNode->elseNode = visit(ifNode->elseNode, handler);
                    }

                    break;
                }
                case Node::NodeKind::While: {
                    auto whileNode = llvm::cast<WhileNode>(node);

                    whileNode->cond = visit(whileNode->cond, handler);
                    whileNode->body = visit(whileNode->body, handler);

                    break;
                }
                case Node::NodeKind::For: {
                    auto forNode = llvm::cast<ForNode>(node);

                    forNode->expr = visit(forNode->expr, handler);
                    forNode->body = visit(forNode->body, handler);

                    break;
                }
                case Node::NodeKind::Range: {
                    auto rangeNode = llvm::cast<RangeNode>(node);

                    if (rangeNode->start) {
                        rangeNode->start = visit(rangeNode->start, handler);
                    }

                    rangeNode->stop = visit(rangeNode->stop, handler);

                    if (rangeNode->step) {
                        rangeNode->step = visit(rangeNode->step, handler);
                    }

                    break;
                }
                case Node::NodeKind::Arg:
                    break;
                case Node::NodeKind::FnDecl:
                    break;
                case Node::NodeKind::FnDef: {
                    auto fnDefNode = llvm::cast<FnDefNode>(node);

                    fnDefNode->body = visit(fnDefNode->body, handler);

                    break;
                }
                case Node::NodeKind::FnCall: {
                    auto fnCallNode = llvm::cast<FnCallNode>(node);

                    std::ranges::transform(fnCallNode->args.begin(), fnCallNode->args.end(), fnCallNode->args.begin(), [&](ExprNode *arg) {
                        return visit(arg, handler);
                    });

                    break;
                }
                case Node::NodeKind::Return: {
                    auto returnNode = llvm::cast<ReturnNode>(node);

                    returnNode->val = visit(returnNode->val, handler);
                    returnNode->val = visit(returnNode->val, handler);

                    break;
                }
                case Node::NodeKind::Println: {
                    auto printlnNode = llvm::cast<PrintlnNode>(node);

                    printlnNode->val = visit(printlnNode->val, handler);

                    break;
                }
                case Node::NodeKind::Break:
                    break;
                case Node::NodeKind::Continue:
                    break;
                case Node::NodeKind::Comment:
                    break;
                case Node::NodeKind::PropDecl: {
                    auto propDeclNode = llvm::cast<PropDeclNode>(node);

                    propDeclNode->decl = visit(propDeclNode->decl, handler);

                    break;
                }
                case Node::NodeKind::MethodDecl:
                    break;
                case Node::NodeKind::MethodDef: {
                    auto methodDefNode = llvm::cast<MethodDefNode>(node);

                    methodDefNode->fnDef = visit(methodDefNode->fnDef, handler);

                    break;
                }
                case Node::NodeKind::Class: {
                    auto classNode = llvm::cast<ClassNode>(node);

                    classNode->body = visit(classNode->body, handler);

                    break;
                }
                case Node::NodeKind::FetchProp: {
                    auto fetchPropNode = llvm::cast<FetchPropNode>(node);

                    fetchPropNode->obj = visit(fetchPropNode->obj, handler);

                    break;
                }
                case Node::NodeKind::FetchStaticProp:
                    break;
                case Node::NodeKind::MethodCall: {
                    auto methodCallNode = llvm::cast<MethodCallNode>(node);

                    methodCallNode->obj = visit(methodCallNode->obj, handler);

                    std::ranges::transform(methodCallNode->args.begin(), methodCallNode->args.end(), methodCallNode->args.begin(), [&](ExprNode *arg) {
                        return visit(arg, handler);
                    });

                    break;
                }
                case Node::NodeKind::StaticMethodCall: {
                    auto staticMethodCallNode = llvm::cast<StaticMethodCallNode>(node);

                    std::ranges::transform(
                            staticMethodCallNode->args.begin(),
                            staticMethodCallNode->args.end(),
                            staticMethodCallNode->args.begin(),
                            [&](ExprNode *arg) {
                                return visit(arg, handler);
                            }
                    );

                    break;
                }
                case Node::NodeKind::AssignProp: {
                    auto assignPropNode = llvm::cast<AssignPropNode>(node);

                    assignPropNode->obj = visit(assignPropNode->obj, handler);
                    assignPropNode->expr = visit(assignPropNode->expr, handler);

                    break;
                }
                case Node::NodeKind::AssignStaticProp: {
                    auto assignStaticPropNode = llvm::cast<AssignStaticPropNode>(node);

                    assignStaticPropNode->expr = visit(assignStaticPropNode->expr, handler);

                    break;
                }
                case Node::NodeKind::New: {
                    auto newNode = llvm::cast<NewNode>(node);

                    std::ranges::transform(newNode->args.begin(), newNode->args.end(), newNode->args.begin(), [&](ExprNode *arg) {
                        return visit(arg, handler);
                    });

                    break;
                }
                case Node::NodeKind::Interface:
                    break;
                case Node::NodeKind::FetchArr: {
                    auto fetchArrNode = llvm::cast<FetchArrNode>(node);

                    fetchArrNode->arr = visit(fetchArrNode->arr, handler);
                    fetchArrNode->idx = visit(fetchArrNode->idx, handler);

                    break;
                }
                case Node::NodeKind::AssignArr: {
                    auto assignArrNode = llvm::cast<AssignArrNode>(node);

                    assignArrNode->arr = visit(assignArrNode->arr, handler);
                    assignArrNode->idx = visit(assignArrNode->idx, handler);
                    assignArrNode->expr = visit(assignArrNode->expr, handler);

                    break;
                }
                case Node::NodeKind::AppendArr: {
                    auto appendArrNode = llvm::cast<AppendArrNode>(node);

                    appendArrNode->arr = visit(appendArrNode->arr, handler);
                    appendArrNode->expr = visit(appendArrNode->expr, handler);

                    break;
                }
            }

            if (llvm::isa<K>(node)) {
                return reinterpret_cast<T *>(handler(llvm::cast<K>(node)));
            }

            return node;

        }
    };
}
