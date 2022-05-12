#ifndef SERVER_H
#define SERVER_H
#include <string>
#include <map>
#include <vector>
#include <array>
#include <iostream>
#include <unistd.h>
#include <sstream>
#include <vector>
#include "OMAP.h"
#include "Utilities.h"
using namespace std;

struct PRFHasher {

    std::size_t operator()(const prf_type &key) const {
        std::hash<uint8_t> hasher;
        size_t result = 0;
        for (size_t i = 0; i < AES_KEY_SIZE; ++i) {
            result = (result << 1) ^ hasher(key[i]);
        }
        return result;
    }
};

class ResultRow {
public:
    int index;
    int leftChild;
    int rightChild;
    int leftAcc;
    int rightAcc;
    bool isLeaf;
    prf_type value;
    int acc;
    bool previousSearch;
    prf_type searchToken;

    void set(int index, bool isLeaf, prf_type value, int acc, int leftChild, int leftAcc, int rightChild, int rightAcc, bool previousSearch, prf_type searchToken) {
        this->acc = acc;
        this->index = index;
        this->isLeaf = isLeaf;
        this->value = value;
        this->previousSearch = previousSearch;
        this->leftChild = leftChild;
        this->leftAcc = leftAcc;
        this->rightChild = rightChild;
        this->rightAcc = rightAcc;
        this->searchToken = searchToken;
    }
};

class Result {
public:
    vector<ResultRow> results;
    unordered_map<prf_type, unordered_map<int, ResultRow>, PRFHasher>* serverResults;

    Result(unordered_map<prf_type, unordered_map<int, ResultRow>, PRFHasher>* re) {
        serverResults = re;
    }

    void insert(int index, bool isLeaf, prf_type value, int acc, int leftChild, int leftAcc, int rightChild, int rightAcc, prf_type tk_i, prf_type keywordToken) {
        ResultRow row;
        row.set(index, isLeaf, value, acc, leftChild, leftAcc, rightChild, rightAcc, false, tk_i);
        if (isLeaf)
            results.push_back(row);
        if ((*serverResults).count(keywordToken) == 0) {
            (*serverResults)[keywordToken] = unordered_map<int, ResultRow>();
        }
        (*serverResults)[keywordToken][index] = row;
    }

    void insert(ResultRow row, prf_type keywordToken) {
        row.previousSearch = true;
        if (row.isLeaf)
            results.push_back(row);
        if ((*serverResults).count(keywordToken) == 0) {
            (*serverResults)[keywordToken] = unordered_map<int, ResultRow>();
        }
        (*serverResults)[keywordToken][row.index] = row;
    }


};

class Server {
private:
    int N, height, firstLeaf;

    unordered_map<prf_type, prf_type, PRFHasher> KV;
    unordered_map<prf_type, unordered_map<int, ResultRow>, PRFHasher> previousResult;
    unordered_map<prf_type, int, PRFHasher> previousResultAw;
    void recursiveSearch(prf_type keywordToken, prf_type Ikey, prf_type Dkey, int index, int acc, Result* result, vector<int>* coverSet, bool indexIsCoverSet, int a_w);
    prf_type get(prf_type key);
    prf_type remove(prf_type key, bool incrementCounter = true);
    void put(prf_type key, prf_type value);
    bool contain(prf_type key, bool incrementCounter = true);
    bool subtreeContainCoverSet(int index, vector<int>* coverSet, bool& indexIsCoverSet);
    void findPreviousSearchResult(prf_type Ikey, prf_type keywordToken, int index, Result* result, int a_w);
    void addSubtreeLeaves(prf_type Ikey, prf_type keywordToken, Result* result, int root);
    void addCacheSubtreeLeaves(prf_type Ikey, prf_type keywordToken, Result* result, int root, int a_w);



public:
    int mapAccess = 0;
    void reset();
    virtual ~Server();
    Server(int N);
    void insert(prf_type key, prf_type value);
    Result search(prf_type keywordToken, prf_type keyI, prf_type KeyD, int a_w, int acc, int coverSetStatus, int coverSetOne);
    void insert(vector<prf_type> keys, vector<prf_type> values);
};

#endif /* SERVER_H */

