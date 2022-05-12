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
    bool isLeaf;
    prf_type value;
    int leftChild;
    int rightChild;
    int acc;

    void set(int index, bool isLeaf, prf_type value, int acc) {
        this->acc = acc;
        this->index = index;
        this->isLeaf = isLeaf;
        this->value = value;
    }
};

class Result {
public:
    vector<ResultRow> results;

    ResultRow insert(int index, bool isLeaf, prf_type value, int acc) {
        ResultRow row;
        row.set(index, isLeaf, value, acc);
        results.push_back(row);
        return row;
    }

    void insert(ResultRow row) {
        results.push_back(row);
    }
};

class Server {
private:
    int N, height, firstLeaf;

    unordered_map<prf_type, prf_type, PRFHasher> KV;
    unordered_map<prf_type, unordered_map<int, ResultRow>, PRFHasher> oldKV;
    unordered_map<prf_type, unordered_map<int, int>, PRFHasher> ACCs;
    void recursiveSearch(prf_type Ikey, prf_type Dkey, prf_type tk_0, int index, Result* result, vector<int>* coverSet, bool indexIsCoverSet);
    prf_type findMaxAcc(prf_type Ikey, prf_type Dkey, prf_type tk_0, int index, prf_type keyForAcc0, int& maxAcc, bool& oldRecord);
    prf_type get(prf_type key);
    prf_type remove(prf_type key, bool incrementCounter = true);
    void put(prf_type key, prf_type value);
    bool contain(prf_type key, bool incrementCounter = true);
    bool subtreeContainCoverSet(int index, vector<int>* coverSet, bool& indexIsCoverSet);
    int getLeftBound(int index, prf_type tk_0, bool& found);
    int smallest_power_of_two_greater_than(int n);
    void exponentialSearch(prf_type Ikey, prf_type Dkey, prf_type tk_0, int index, int& leftBound, int& rightBound, int& maxAcc, prf_type& lastKey);


public:
    int mapAccess = 0;
    int existingEntries = 0;
    void reset();
    virtual ~Server();
    Server(int N);
    void insert(prf_type key, prf_type value);
    Result search(prf_type keyI, prf_type KeyD, prf_type tk_0, int a_w);
    void insert(vector<prf_type> keys, vector<prf_type> values);
};

#endif /* SERVER_H */

