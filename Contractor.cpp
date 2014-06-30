//  Created by Nils Becker on 11/03/14.
//  Copyright (c) 2014 Nils Becker. All rights reserved.

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/AST/Comment.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "Contract.cpp"

#include <iostream>
#include <string>
#include <list>
#include <boost/regex.hpp>

using namespace std;
using namespace clang;
using namespace clang::comments;
using namespace llvm;

Rewriter rewriter;

class  RecursiveVisitor : public RecursiveASTVisitor<RecursiveVisitor> {
private:
    ASTContext *astContext;
    FunctionDecl *currentDecl;

    list<string> classInvariantsForDecl(Decl *dcl) {
      string methodClassName = ((CXXMethodDecl*)dcl)->getParent()->getName().str();

      map<string, list<string> >::iterator result = this->classInvariants.find(methodClassName);
      if (result != this->classInvariants.end()) {
        return result->second;
      } else {
        list<string> emptyList;
        return emptyList;
      }
    }

    void addConditionWithTypeForFunction(string cond, clause_type type, NamedDecl *declaration) {
      string funcName = declaration->getName().str();
      map<string, list<string> > lookupMap;

      // needs better source for unique id
      unsigned globalIDForFunction = declaration->getLocStart().getRawEncoding();
      // cout << "fname: " << funcName << " id: " << globalIDForFunction << "cond: " << cond << endl;
      map< unsigned, Contract >::iterator existingContract = this->contracts.find(globalIDForFunction);
      bool contractAllreadyExists = existingContract != this->contracts.end();
      Contract contractForDeclaration = Contract(funcName);

      if (contractAllreadyExists) {
        contractForDeclaration = existingContract->second;
      }

      Clause *newClauseForDeclaration = ClauseFactory::clauseWithDescriptionAndType(cond, type);
      contractForDeclaration.addClause(newClauseForDeclaration);

      this->contracts[globalIDForFunction] = contractForDeclaration;

      switch (type) {
        case PRE: lookupMap = this->preconditions; break;
        case POST: lookupMap = this->postconditions; break;
        case INV: lookupMap = this->invariants; break;
      }

      map<string, list<string> >::iterator result = lookupMap.find(funcName);
      list<string> list;

      if (result != lookupMap.end()) {
        list = result->second;
        list.push_back(cond);
      } else {
        list.push_back(cond);
      }

      switch (type) {
        case PRE: this->preconditions[funcName] = list; break;
        case POST: this->postconditions[funcName] = list; break;
        case INV: this->invariants[funcName] = list; break;
      }

    }

    string getConditionFromBlockCommandComment(BlockCommandComment *bcc) {
      Comment::child_iterator c = bcc->child_begin();
      ParagraphComment *paragraphComment = (ParagraphComment *)*c;
      string fullConditionText;

      for (Comment::child_iterator i = paragraphComment->child_begin(); i != paragraphComment->child_end(); i++) {
        TextComment *textComment = (TextComment *)*i;
        fullConditionText += textComment->getText().str();
      }

      return fullConditionText;
    }

    list<Clause*> getClausesForDeclaration(Decl *declaration) {
      list<Clause*> stub;
      return stub;
    }

    void addClauseGroupsToBuffer(list <list<Clause*>> clauseGroups, list<string>& buffer) {
      bool hasInheritedClauses = clauseGroups.size() > 1;
      bool hasOnlyItsOwnClauses = clauseGroups.size() == 1;

      if (hasInheritedClauses) {
        string assertionString = "\nassert(";

        for (list < list<Clause*> >::reverse_iterator it = clauseGroups.rbegin(); it != clauseGroups.rend(); it++) {
          list<Clause*> preconditions = *it;
          assertionString += " ( ";

          for (list<Clause*>::reverse_iterator i = preconditions.rbegin(); i != preconditions.rend(); i++) {
            Clause* clause = *i;
            assertionString += clause->getIdentifier() + " && ";
            buffer.push_back(clause->stringRepresentation());
          }

          assertionString = assertionString.substr(0, assertionString.length() - 4);
          assertionString += " ) ";

          if (it != clauseGroups.rend()) assertionString += "||";
      }

      assertionString = assertionString.substr(0, assertionString.length() - 3);
      assertionString += ");\n\n";
      buffer.push_back(assertionString);

      } else if (hasOnlyItsOwnClauses) {
        string assertionString = "\nassert( ";

        for (list < list<Clause*> >::reverse_iterator it = clauseGroups.rbegin(); it != clauseGroups.rend(); it++) {
          list<Clause*> preconditions = *it;
          for (list<Clause*>::reverse_iterator i = preconditions.rbegin(); i != preconditions.rend(); i++) {
            Clause* clause = *i;
            assertionString += clause->getIdentifier() + " && ";
            buffer.push_back(clause->stringRepresentation());
          }
        }

        assertionString = assertionString.substr(0, assertionString.length() - 3);
        assertionString += ");\n\n";
        buffer.push_back(assertionString);
      }
  }

public:
    map<string, list<string> > preconditions;
    map<string, list<string> > postconditions;
    map<string, list<string> > invariants;
    map<string, list<string> > classInvariants;

    map<unsigned, Contract> contracts;

    set<string> traversedFunctions;
    set<string> traversedClasses;

    explicit RecursiveVisitor(CompilerInstance *CI)
      : astContext(&(CI->getASTContext()))
    {
        rewriter.setSourceMgr(astContext->getSourceManager(), astContext->getLangOpts());
    }

    // collect all invariants from class declaration
    virtual bool VisitCXXRecordDecl(CXXRecordDecl *cls)
    {
      Comment *comment = astContext->getCommentForDecl(cls, NULL);
      bool classHasComment = comment != NULL;

      if (classHasComment) {
        list<string> classInvariants;
        for (FullComment::child_iterator c = comment->child_begin(), e = comment->child_end(); c != e; ++c) {
          Comment *fullCommentChild = *c;
          bool thisIsABlockCommandComment = strncmp(fullCommentChild->getCommentKindName(), "BlockCommandComment", 2) == 0;

          if (thisIsABlockCommandComment) {
            BlockCommandComment *bcc = (BlockCommandComment *)fullCommentChild;
            string commandName = bcc->getCommandName(astContext->getCommentCommandTraits()).str();
            bool thisIsAInvariantClause = commandName.compare("invariant") == 0;

            if (thisIsAInvariantClause) {
              Comment::child_iterator c = bcc->child_begin();
              ParagraphComment *paragraphComment = (ParagraphComment *)*c;

              Comment::child_iterator t = paragraphComment->child_begin();
              TextComment *textComment = (TextComment *)*t;
              string invariantString = textComment->getText().str();

              this->addConditionWithTypeForFunction(invariantString, INV, cls);

              classInvariants.push_back(invariantString);
            }
          }
        }
        string className = cls->getName().str();
        this->traversedClasses.insert(className);
        this->classInvariants[className] = classInvariants;
      }

      return true;
    }

    // insert pre- and postconditions for functions
    virtual bool VisitFunctionDecl(FunctionDecl *func) {
      currentDecl = func;
      string funcName = func->getNameInfo().getName().getAsString();
      Comment *comment = astContext->getCommentForDecl(func, NULL);
      bool isVoidFunction = func->getResultType()->isVoidType();
      bool isFunctionWithComment = comment != NULL;

      list <string> preconditionBuffer;
      list <string> postconditionBuffer;
      list <Clause*> preconditionClauses;
      list <Clause*> postconditionClauses;

      if (isFunctionWithComment) {
        for (FullComment::child_iterator c = comment->child_begin(), e = comment->child_end(); c != e; ++c) {
          Comment *fullCommentChild = *c;
          if (strncmp(fullCommentChild->getCommentKindName(), "BlockCommandComment", 2) == 0) {
            BlockCommandComment *bcc = (BlockCommandComment *)fullCommentChild;
            string commandName = bcc->getCommandName(astContext->getCommentCommandTraits()).str();
            string condition = this->getConditionFromBlockCommandComment(bcc);

            if (commandName.compare("pre") == 0) {
              this->traversedFunctions.insert(funcName);
              addConditionWithTypeForFunction(condition, PRE, func);
              Clause *preconditionClause = ClauseFactory::clauseWithDescriptionAndType(condition, PRE);
              preconditionClauses.push_back(preconditionClause);
              preconditionBuffer.push_back(preconditionClause->stringRepresentation());
              // cout << preconditionClause->stringRepresentation() << endl;
            } else if (commandName.compare("post") == 0 && isVoidFunction) {
              addConditionWithTypeForFunction(condition, POST, func);
              this->traversedFunctions.insert(funcName);
              Clause *postconditionClause = ClauseFactory::clauseWithDescriptionAndType(condition, POST);
              postconditionClauses.push_back(postconditionClause);
              postconditionBuffer.push_back(postconditionClause->stringRepresentation());
            }
          }
        }

        bool functionHasPreconditions = preconditionClauses.size() > 0;
        bool functionHasPostconditions = postconditionClauses.size() > 0;
        bool isNotAMethodd = !dyn_cast<CXXMethodDecl>(func);

        if (functionHasPreconditions) {
          string assertionString = "\nassert(";

          for (list <Clause*>::reverse_iterator it = preconditionClauses.rbegin(); it != preconditionClauses.rend(); it++) {
            Clause* precondition = *it;
            assertionString += precondition->getIdentifier() + " && ";
          }

          assertionString = assertionString.substr(0, assertionString.length() - 3);
          assertionString += ");\n\n";
          preconditionBuffer.push_front(assertionString);
        }

        if (functionHasPostconditions) {
          string assertionString = "\nassert(";

          for (list <Clause*>::reverse_iterator it = postconditionClauses.rbegin(); it != postconditionClauses.rend(); it++) {
            Clause* postcondition = *it;
            assertionString += postcondition->getIdentifier() + " && ";
          }

          assertionString = assertionString.substr(0, assertionString.length() - 3);
          assertionString += ");\n\n";
          postconditionBuffer.push_front(assertionString);
        }

        // if this is a function and not a method, write out its pre and postconditions
        if (isNotAMethodd) {
          if (preconditionBuffer.size() > 0) { preconditionBuffer.push_back("\n/* auto generated function precondition assertions */\n"); }

          // write out all preconditions
          for (list<string>::reverse_iterator i = preconditionBuffer.rbegin(); i != preconditionBuffer.rend(); i++) {
            string currentStringFromBuffer = *i;
            rewriter.InsertTextAfterToken(func->getBody()->getSourceRange().getBegin(), currentStringFromBuffer);
          }

          if (postconditionBuffer.size() > 0 && isVoidFunction) { postconditionBuffer.push_back("\n/* auto generated function postcondition assertions */\n"); }

          // write out all postconditions
          for (list<string>::iterator i = postconditionBuffer.begin(); i != postconditionBuffer.end(); i++) {
            string currentStringFromBuffer = *i;
            rewriter.InsertText(func->getBody()->getSourceRange().getEnd(), currentStringFromBuffer, false, false);
          }

          if (rewriter.overwriteChangedFiles()) { cerr << "could not write out changes" << endl; }
        }
      }

      return true;
    }

    // insert class invariants, preconditions and postconditions for methods
    virtual bool VisitCXXMethodDecl(CXXMethodDecl *mdcl) {
      Comment *comment = astContext->getCommentForDecl(mdcl, NULL);
      string resultType = ((FunctionDecl*)mdcl)->getResultType().getAsString();

      list< string > endWriteBuffer;
      list< string > startWriteBuffer;

      bool isVoidFunction = mdcl->getResultType()->isVoidType();
      bool functionHasComment = comment != NULL;

      // collect postconditions for function
      if (functionHasComment) {
        for (FullComment::child_iterator c = comment->child_begin(), e = comment->child_end(); c != e; ++c) {
          Comment *fullCommentChild = *c;
          bool thisIsABlockCommandComment = strncmp(fullCommentChild->getCommentKindName(), "BlockCommandComment", 2) == 0;

          if (thisIsABlockCommandComment) {
            BlockCommandComment *bcc = (BlockCommandComment *)fullCommentChild;
            string commandName = bcc->getCommandName(astContext->getCommentCommandTraits()).str();
            string condition = this->getConditionFromBlockCommandComment(bcc);
            bool thisIsAPostCondition = commandName.compare("post") == 0;

            if (thisIsAPostCondition) {
              addConditionWithTypeForFunction(condition, POST, mdcl);
            }
          }
        }
      }

      // if this is a void function, collect all of its preconditions, postconditions and invariants
      // and write them out - this includes all inherited clauses from its superclasses
      if (isVoidFunction) {
        const CXXRecordDecl* parentClassDeclaration = (CXXRecordDecl*)mdcl->getParent();

        map<unsigned, Contract >::iterator foundContractResult = this->contracts.find(parentClassDeclaration->getLocStart().getRawEncoding());
        bool classForMethodHasContract = foundContractResult != this->contracts.end();

        map<unsigned, Contract >::iterator foundMethodContractResult = this->contracts.find(mdcl->getLocStart().getRawEncoding());
        bool methodHasContract = foundMethodContractResult != this->contracts.end();

        list< list<Clause*> > invariantGroups;
        list< list<Clause*> > postconditionGroups;
        list< list<Clause*> > preconditionGroups;

        // if the method has a contract, collect all of its pre- and postconditions
        if (methodHasContract) {
          Contract contractForCurrentMethod = foundMethodContractResult->second;
          list<Clause*> preconditions = contractForCurrentMethod.getAllClausesWithType(PRE);
          list<Clause*> postconditions = contractForCurrentMethod.getAllClausesWithType(POST);

          if (preconditions.size() > 0) {
            preconditionGroups.push_back(preconditions);
          }

          if (postconditions.size() > 0) {
            postconditionGroups.push_back(postconditions);
          }
        }

        // if this methods class has invariants, proceed and collect them
        // iterate over all overridden methods and collect their pre- and postconditions
        // iterate over all superclasses and collect their invariants
        if (classForMethodHasContract) {
          Contract contractForCurrentMethod = foundContractResult->second;
          list<Clause*> invariants = contractForCurrentMethod.getAllClausesWithType(INV);

          if (invariants.size() > 0) {
            invariantGroups.push_back(invariants);
          }

          // collect conditions from overriden methods
          for (CXXMethodDecl::method_iterator i = mdcl->begin_overridden_methods(); i != mdcl->end_overridden_methods(); i++) {
            const CXXMethodDecl *overriddenMethodDecl = (const CXXMethodDecl*)*i;
            const CXXRecordDecl* inheritedParentClassDeclaration = (const CXXRecordDecl*)overriddenMethodDecl->getParent();

            map<unsigned, Contract >::iterator foundContractResult = this->contracts.find(inheritedParentClassDeclaration->getLocStart().getRawEncoding());
            bool classForMethodHasContract = foundContractResult != this->contracts.end();

            if (classForMethodHasContract) {
              Contract contractForCurrentMethod = foundContractResult->second;
              list<Clause*> inheritedInvariants = contractForCurrentMethod.getAllClausesWithType(INV);
              if (inheritedInvariants.size() > 0) {
                invariantGroups.push_back(inheritedInvariants);
              }
            }

            map<unsigned, Contract >::iterator foundContractForMethodResult = this->contracts.find(overriddenMethodDecl->getLocStart().getRawEncoding());
            bool methodHasContract = foundContractForMethodResult != this->contracts.end();

            if (methodHasContract) {
              Contract contractForCurrentMethod = foundContractForMethodResult->second;
              list<Clause*> inheritedPreconditions = contractForCurrentMethod.getAllClausesWithType(PRE);
              list<Clause*> inheritedPostconditions = contractForCurrentMethod.getAllClausesWithType(POST);

              if (inheritedPreconditions.size() > 0) {
                preconditionGroups.push_back(inheritedPreconditions);
              }

              if (inheritedPostconditions.size() > 0) {
                postconditionGroups.push_back(inheritedPostconditions);
              }
            }
          }

          // insert comment to signal the start of the methods preconditions
          if (preconditionGroups.size() > 0) { startWriteBuffer.push_back("\n/* auto generated precondition assertions */\n "); }

          addClauseGroupsToBuffer(preconditionGroups, startWriteBuffer);

          // insert comment to signal the start of the methods postconditions
          if (postconditionGroups.size() > 0) { endWriteBuffer.push_back("\n/* auto generated postcondition assertions */\n "); }

          addClauseGroupsToBuffer(postconditionGroups, endWriteBuffer);

          // insert comment to signal the start of the methods invariants
          if (invariantGroups.size() > 0) { endWriteBuffer.push_back("\n/* auto generated invariant assertions */\n "); }

          addClauseGroupsToBuffer(invariantGroups, endWriteBuffer);

          // write out buffer at the start of the functionscope
          for (list<string>::iterator it = startWriteBuffer.begin(); it != startWriteBuffer.end(); it++) {
            string currentStringFromBuffer = *it;
            rewriter.InsertTextAfterToken(mdcl->getBody()->getSourceRange().getBegin(), currentStringFromBuffer);
          }

          // write out buffer at the end of the functionscope
          for (list<string>::reverse_iterator it = endWriteBuffer.rbegin(); it != endWriteBuffer.rend(); it++) {
            string currentStringFromBuffer = *it;
            rewriter.InsertText(mdcl->getBody()->getSourceRange().getEnd(), currentStringFromBuffer, false, true);
          }

          if (rewriter.overwriteChangedFiles()) cerr << "could not insert text" << endl;
        }

      }

      return true;
    }

    void insertAssertsBeforeReturn(ReturnStmt *ret) {
      Comment *comment = astContext->getCommentForDecl(this->currentDecl, NULL);
      string returnStmt = rewriter.ConvertToString(ret).substr(7);
      string resultType = ((FunctionDecl*)this->currentDecl)->getResultType().getAsString();

      list< string > endWriteBuffer;
      list< string > startWriteBuffer;

      bool shouldRescueReturnValue = false;
      bool functionHasComment = comment != NULL;

      if (functionHasComment) {
        for (FullComment::child_iterator c = comment->child_begin(), e = comment->child_end(); c != e; ++c) {
          Comment *fullCommentChild = *c;
          bool thisIsABlockCommandComment = strncmp(fullCommentChild->getCommentKindName(), "BlockCommandComment", 2) == 0;

          if (thisIsABlockCommandComment) {
            BlockCommandComment *bcc = (BlockCommandComment *)fullCommentChild;
            string commandName = bcc->getCommandName(astContext->getCommentCommandTraits()).str();
            string condition = this->getConditionFromBlockCommandComment(bcc);
            bool thisIsAPostCondition = commandName.compare("post") == 0;

            if (thisIsAPostCondition) {
              shouldRescueReturnValue = true;
              // addConditionWithTypeForFunction(condition, POST, this->currentDecl);
            }
          }
        }
      }

      // if this is a method, proceed and collect all invariants, pre- and postconditions
      // also iterate over all overridden methods and collect their pre- and postconditions
      // also iterate over all superclasses and collect their invariants
      if (CXXMethodDecl* methodDecl = dyn_cast<CXXMethodDecl>(this->currentDecl)) {
        const CXXRecordDecl* parentClassDeclaration = (CXXRecordDecl*)methodDecl->getParent();

        map<unsigned, Contract >::iterator foundContractResult = this->contracts.find(parentClassDeclaration->getLocStart().getRawEncoding());
        bool classForMethodHasContract = foundContractResult != this->contracts.end();

        map<unsigned, Contract >::iterator foundMethodContractResult = this->contracts.find(this->currentDecl->getLocStart().getRawEncoding());
        bool methodHasContract = foundMethodContractResult != this->contracts.end();

        list< list<Clause*> > invariantGroups;
        list< list<Clause*> > postconditionGroups;
        list< list<Clause*> > preconditionGroups;

        if (methodHasContract) {
          Contract contractForCurrentMethod = foundMethodContractResult->second;
          list<Clause*> preconditions = contractForCurrentMethod.getAllClausesWithType(PRE);
          list<Clause*> postconditions = contractForCurrentMethod.getAllClausesWithType(POST);

          if (preconditions.size() > 0) {
            preconditionGroups.push_back(preconditions);
          }

          if (postconditions.size() > 0) {
            postconditionGroups.push_back(postconditions);
          }
        }


        if (classForMethodHasContract) {
          Contract contractForCurrentMethod = foundContractResult->second;
          list<Clause*> invariants = contractForCurrentMethod.getAllClausesWithType(INV);

          if (invariants.size() > 0) {
            invariantGroups.push_back(invariants);
          }

          // traverse conditions from overriden methods
          for (CXXMethodDecl::method_iterator i = methodDecl->begin_overridden_methods(); i != methodDecl->end_overridden_methods(); i++) {
            const CXXMethodDecl *overriddenMethodDecl = (const CXXMethodDecl*)*i;
            const CXXRecordDecl* inheritedParentClassDeclaration = (const CXXRecordDecl*)overriddenMethodDecl->getParent();

            map<unsigned, Contract >::iterator foundContractResult = this->contracts.find(inheritedParentClassDeclaration->getLocStart().getRawEncoding());
            bool classForMethodHasContract = foundContractResult != this->contracts.end();

            if (classForMethodHasContract) {
              Contract contractForCurrentMethod = foundContractResult->second;
              list<Clause*> inheritedInvariants = contractForCurrentMethod.getAllClausesWithType(INV);
              if (inheritedInvariants.size() > 0) {
                invariantGroups.push_back(inheritedInvariants);
              }
            }

            map<unsigned, Contract >::iterator foundContractForMethodResult = this->contracts.find(overriddenMethodDecl->getLocStart().getRawEncoding());
            bool methodHasContract = foundContractForMethodResult != this->contracts.end();

            if (methodHasContract) {
              Contract contractForCurrentMethod = foundContractForMethodResult->second;
              list<Clause*> inheritedPreconditions = contractForCurrentMethod.getAllClausesWithType(PRE);
              list<Clause*> inheritedPostconditions = contractForCurrentMethod.getAllClausesWithType(POST);

              if (inheritedPreconditions.size() > 0) {
                preconditionGroups.push_back(inheritedPreconditions);
              }

              if (inheritedPostconditions.size() > 0) {
                postconditionGroups.push_back(inheritedPostconditions);
              }
            }
          }

          // insert comment to signal the start of the methods preconditions
          if (preconditionGroups.size() > 0) { startWriteBuffer.push_back("\n/* auto generated precondition assertions */\n "); }

          addClauseGroupsToBuffer(preconditionGroups, startWriteBuffer);

          // insert comment to signal the start of the methods postconditions and rescue return value
          if (postconditionGroups.size() > 0) {
            shouldRescueReturnValue = true;
            endWriteBuffer.push_back("\n/* auto generated postcondition assertions */\n ");
            endWriteBuffer.push_back(resultType + " rescuedReturn = " + returnStmt);
          }

          addClauseGroupsToBuffer(postconditionGroups, endWriteBuffer);

          // insert comment to signal the start of the methods invariants and rescue return value if not allready rescued
          if (invariantGroups.size() > 0) {
            shouldRescueReturnValue = true;
            endWriteBuffer.push_back("\n/* auto generated invariant assertions */\n ");
            bool didNotInsertAnyPostconditions = postconditionGroups.size() == 0;

            if (didNotInsertAnyPostconditions) {
              endWriteBuffer.push_back(resultType + " rescuedReturn = " + returnStmt);
            }
          }

          addClauseGroupsToBuffer(invariantGroups, endWriteBuffer);

          // rescue return if needed
          if (shouldRescueReturnValue) {
            rewriter.RemoveText(ret->getSourceRange().getBegin(), rewriter.ConvertToString(ret).size());
            endWriteBuffer.push_back("\n\nreturn rescuedReturn;\n");
          }

          // write out buffer at the start of the functionscope
          for (list<string>::iterator it = startWriteBuffer.begin(); it != startWriteBuffer.end(); it++) {
            string currentStringFromBuffer = *it;
            rewriter.InsertTextAfterToken(methodDecl->getBody()->getSourceRange().getBegin(), currentStringFromBuffer);
          }

          // write out buffer at the end of the functionscope
          for (list<string>::reverse_iterator it = endWriteBuffer.rbegin(); it != endWriteBuffer.rend(); it++) {
            string currentStringFromBuffer = *it;
            rewriter.InsertText(methodDecl->getBody()->getSourceRange().getEnd(), currentStringFromBuffer, false, true);
          }

          if (rewriter.overwriteChangedFiles()) cerr << "could not insert text" << endl;
        }
      }
      // in this case we are looking at a function and don't have to deal with inherited contracts
      // the functions preconditions are allready inserted, so we only look at its postconditions
      else {
        map<unsigned, Contract >::iterator foundFunctionContractResult = this->contracts.find(this->currentDecl->getLocStart().getRawEncoding());
        bool functionHasContract = foundFunctionContractResult != this->contracts.end();

        if (functionHasContract) {
          Contract contractForFunction = foundFunctionContractResult->second;
          list<Clause*> postconditions = contractForFunction.getAllClausesWithType(POST);

          if (postconditions.size() > 0) {
            endWriteBuffer.push_back("\n/* auto generated postcondition assertions */\n ");
            endWriteBuffer.push_back(resultType + " rescuedReturn = " + returnStmt);
          }

          for (list<Clause*>::reverse_iterator i = postconditions.rbegin(); i != postconditions.rend(); i++) {
            Clause *clause = *i;
            endWriteBuffer.push_back(clause->stringRepresentation());
          }

          string assertionString = "assert(";
          for (list<Clause*>::reverse_iterator i = postconditions.rbegin(); i != postconditions.rend(); i++) {
            Clause *clause = *i;
            assertionString += clause->getIdentifier() + " && ";
          }

          assertionString = assertionString.substr(0, assertionString.length() - 4);
          assertionString += ")";
          endWriteBuffer.push_back(assertionString);

          if (postconditions.size() > 0) {
            rewriter.RemoveText(ret->getSourceRange().getBegin(), rewriter.ConvertToString(ret).size());
            endWriteBuffer.push_back("\n\nreturn rescuedReturn;\n");
          }

          // write out buffer at the end of the functionscope
          for (list<string>::reverse_iterator it = endWriteBuffer.rbegin(); it != endWriteBuffer.rend(); it++) {
            string currentStringFromBuffer = *it;
            rewriter.InsertText(this->currentDecl->getBody()->getSourceRange().getEnd(), currentStringFromBuffer, false, true);
          }

        }
      }

    }

    virtual bool VisitReturnStmt(ReturnStmt *ret)
    {
      insertAssertsBeforeReturn(ret);
      return true;
    }

    // Handle Loops
    virtual bool VisitForStmt(ForStmt *forstmt){return true;}
    virtual bool VisitWhileStmt(WhileStmt *whileStmt){return true;}
    virtual bool VisitForStmt(DoStmt *doStmt){return true;}
};

class Consumer : public ASTConsumer {
private:
     RecursiveVisitor *visitor;

public:
    explicit Consumer(CompilerInstance *CI)
        : visitor(new RecursiveVisitor(CI))
    { }

    virtual void HandleTranslationUnit(ASTContext &Context) {
        visitor->TraverseDecl(Context.getTranslationUnitDecl());

        map<unsigned, Contract> contracts = visitor->contracts;
        for(map<unsigned, Contract>::iterator it = contracts.begin(); it != contracts.end(); ++it) {
          Contract currentContract = it->second;
          string nameOfContract = currentContract.getIdentifier();
          cout << "\033[1;33m" + nameOfContract + "\033[0m" << endl;

          list<Clause*> invariants = currentContract.getAllClausesWithType(INV);
          list<Clause*> preconditions = currentContract.getAllClausesWithType(PRE);
          list<Clause*> postconditions = currentContract.getAllClausesWithType(POST);

          bool contractHasInvariants = invariants.begin() != invariants.end();
          bool contractHasPreconditions = preconditions.begin() != preconditions.end();
          bool contractHasPostconditions = postconditions.begin() != postconditions.end();

          if (contractHasPreconditions) {
            cout << "\033[1;36m  -Preconditions\033[0m" << endl;
            for (list<Clause*>::iterator it = preconditions.begin(); it != preconditions.end(); it++) {
              // Clause *currentClause = *it;
              // cout << "\033[1;36m    " << currentClause->getIdentifier() << ": [" << currentClause->getPredicate() << "]\033[0m" << endl;
            }
          }

          if (contractHasPostconditions) {
            cout << "\033[1;36m  -Postconditions\033[0m" << endl;
            for (list<Clause*>::iterator it = postconditions.begin(); it != postconditions.end(); it++) {
              // Clause *currentClause = *it;
              // cout << "\033[1;36m    " << currentClause->getIdentifier() << ": [" << currentClause->getPredicate() << "]\033[0m" << endl;
            }
          }

          if (contractHasInvariants) {
            cout << "\033[1;36m  -Invariants\033[0m" << endl;
            for (list<Clause*>::iterator it = invariants.begin(); it != invariants.end(); it++) {
              // Clause *currentClause = *it;
              // cout << "\033[1;36m    " << currentClause->getIdentifier() << ": [" << currentClause->getPredicate() << "]\033[0m" << endl;
            }
          }

        }
    }
};

class ContractorAction : public PluginASTAction {

protected:

  ASTConsumer *CreateASTConsumer(CompilerInstance &CI, llvm::StringRef) {
    return new Consumer(&CI);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string>& args) {
    for (unsigned i = 0, e = args.size(); i != e; ++i) {
      llvm::errs() << "Contractor arg = " << args[i] << "\n";

      // Example error handling.
      if (args[i] == "-an-error") {
        DiagnosticsEngine &D = CI.getDiagnostics();
        unsigned DiagID = D.getCustomDiagID(
          DiagnosticsEngine::Error, "invalid argument '" + args[i] + "'");
        D.Report(DiagID);
        return false;
      }
    }
    if (args.size() && args[0] == "help")
      PrintHelp(llvm::errs());

    return true;
  }
  void PrintHelp(llvm::raw_ostream& ros) {
    ros << "Help for Contractor plugin goes here\n";
  }

};

static FrontendPluginRegistry::Add<ContractorAction>
X("contractor", "inserts asserts for dbc-contract-specifications");
