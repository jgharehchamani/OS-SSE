#ifndef AVLTREE_H
#define AVLTREE_H
#include "ORAM.hpp"
#include <functional>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <array>
#include <memory>
#include <type_traits>
#include <iomanip>
#include "Bid.h"
#include <random>
#include <algorithm>
#include <stdlib.h>
#include <functional>
#include <utility>
using namespace std;

using updateFn = std::function<string(string)>;
using updateArrayFn = std::function<std::array< byte_t, VALUE_SIZE>(std::array< byte_t, VALUE_SIZE>, bool)>;

class AVLTree {
private:
    int height(Bid N, int& leaf);
    int max(int a, int b);
    Node* newNode(Bid key, string value);
    Node* newNode(Bid key, std::array< byte_t, VALUE_SIZE> value);
    Node* rightRotate(Node* y);
    Node* leftRotate(Node* x);
    int getBalance(Node* N);
    int RandomPath();
    int maxOfRandom;
    int sortedArrayToBST(int start, int end, int& pos, Bid& node);
    int setupProgress = 0;
    vector<Node*> setupNodes;

public:
    ORAM *oram;
    AVLTree(int maxSize, bytes<Key> key);
    virtual ~AVLTree();
    Bid insert(Bid rootKey, int& pos, Bid key, string value);
    string incrementSrcCnt(Node* head, Bid key);
    string incrementAwCnt(Node* head, Bid key);
    Node* search(Node* head, Bid key);
    void batchSearch(Node* head, vector<Bid> keys, vector<Node*>* results);
    void printTree(Node* root, int indent);
    void startOperation(bool batchWrite = false);
    void finishOperation(bool find, Bid& rootKey, int& rootPos);
    void setupInsert(Bid& rootKey, int& rootPos, map<Bid, string> pairs);
    Bid insert(Bid rootKey, int& pos, Bid key, std::array< byte_t, VALUE_SIZE> value);
    string getSet(Node* head, Bid key, updateFn updateValue);
    std::array< byte_t, VALUE_SIZE> getSet(Node* head, Bid key, updateArrayFn updateValue, bool& found);
    void setupInsert(Bid& rootKey, int& rootPos, map<Bid, std::array< byte_t, VALUE_SIZE> > pairs);
};

#endif /* AVLTREE_H */




