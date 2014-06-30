//  Created by Nils Becker on 11/03/14.
//  Copyright (c) 2014 Nils Becker. All rights reserved.

#include <iostream>
#include <list>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace boost;

enum clause_type { PRE, POST, INV };

static const boost::regex nameExpression("^\\s*([a-z]+[a-zA-z0-9_]*):");
static const boost::regex predicateExpression("^\\s*([a-z]+[a-zA-z0-9_]*):\\s([a-zA-Z0-9].*[a-zA-Z0-9])$");
static const boost::regex collectionExpression("\\(A\\s*([a-z]+):\\s*([a-z0-9A-Z])+\\s*(<=|<){1}\\s*[a-z]+(.*),(.*)\\)$");

class Contract;

class Clause {
private:
    string identifier;
    clause_type type;

public:
    Clause(string clauseDescription, clause_type type) {
        boost::smatch matchResult;

        boost::regex_search(clauseDescription, matchResult, nameExpression);
        string identifier = matchResult[1];

        // trim all whitespace
        identifier.erase(remove(identifier.begin(), identifier.end(),' '), identifier.end());

        this->identifier = identifier;
        this->type = type;
    }

    virtual string getIdentifier() { return this->identifier; }
    virtual clause_type getType() { return this->type; }
    virtual string stringRepresentation() { return "---"; }
};

class PredicateClause : public Clause {
private:
    string predicate;

public:
    PredicateClause(string clauseDescription, clause_type type) : Clause(clauseDescription, type) {
        boost::smatch matchResult;
        boost::regex_search(clauseDescription, matchResult, predicateExpression);

        string predicate = matchResult[2];

        this->predicate = predicate;
    }

    string getPredicate() { return this->predicate; }
    virtual string stringRepresentation() { return "bool " + this->getIdentifier() + " =  " + this->getPredicate() +";\n"; }
};

class CollectionClause : public Clause {
private:
    string predicate;
    string runningIndex;
    string lowerBound;
    string upperBound;

public:
    CollectionClause(string clauseDescription, clause_type type) : Clause(clauseDescription, type) {
        boost::smatch matchResult;
        regex_search(clauseDescription, matchResult, collectionExpression);

        this->runningIndex = matchResult[1];
        this->lowerBound = matchResult[2];
        this->upperBound = matchResult[4];
        this->predicate = matchResult[5];

        string lowerBoundExpression = matchResult[3];
        string lessThan = "<";

        if (lessThan.compare(lowerBoundExpression) == 0) {
            this->lowerBound += " + 1";
        }
    }

    string getPredicate() { return this->predicate; }
    virtual string stringRepresentation() { return "\n\n bool " + this->getIdentifier() + " = false;\nfor (int " + runningIndex + " = " + lowerBound + "; " + runningIndex + upperBound + "; " + runningIndex + "++) { \n  " + this->getIdentifier() + " = " + this->getPredicate() + ";\n}\n"; }
};


class Contract {

private:
    list<Clause*> clauses;
    list<Contract*> subcontracts;
    string identifier;

public:
    Contract() {
        this->identifier = "---";
    }

    Contract(string identifier) {
        this->identifier = identifier;
    }

    string getIdentifier() {
        return this->identifier;
    }

    void addClause(Clause* clause) {
        this->clauses.push_back(clause);
    }

    list<Clause*> getAllClauses() { return this->clauses; }

    list<Clause*> getAllClausesWithType(clause_type typeToFilter) {
        list<Clause*> allFilteredClauses;
        for (list<Clause*>::iterator i = this->clauses.begin(); i != this->clauses.end(); i++) {
            Clause *currentClause = *i;
            if (currentClause->getType() == typeToFilter) allFilteredClauses.push_back(currentClause);
        }
        return allFilteredClauses;
    }

    void addSubcontract(Contract* contract) {
        this->subcontracts.push_back(contract);
    }

    list<Contract*> getSubcontracts() {
        return this->subcontracts;
    }

    bool hasSubcontracts() {
        return !this->subcontracts.empty();
    }
};

class ClauseFactory {

public:
    static Clause* clauseWithDescriptionAndType(string clauseDescription, clause_type type) {
        bool isACollectionClause = boost::regex_search(clauseDescription, collectionExpression);
        Clause *newClause;

        if (isACollectionClause) {
            newClause = new CollectionClause(clauseDescription, type);
        } else {
            newClause = new PredicateClause(clauseDescription, type);
        }

        return newClause;
    }
};
