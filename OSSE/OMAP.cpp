#include "OMAP.h"
using namespace std;

OMAP::OMAP(int maxSize, bytes<Key> key) {
    treeHandler = new AVLTree(maxSize, key);
    rootKey = 0;
}

OMAP::~OMAP() {

}

string OMAP::find(Bid key) {
    if (rootKey == 0) {
        return "";
    }
    treeHandler->startOperation();
    Node* node = new Node();
    node->key = rootKey;
    node->pos = rootPos;
    auto resNode = treeHandler->search(node, key);
    string res = "";
    if (resNode != NULL) {
        res.assign(resNode->value.begin(), resNode->value.end());
        res = res.c_str();
    }
    treeHandler->finishOperation(true, rootKey, rootPos);
    return res;
}

std::array< byte_t, VALUE_SIZE> OMAP::find(Bid key, bool& found) {
    if (rootKey == 0) {
        found = false;
        return std::array< byte_t, VALUE_SIZE>();
    }
    treeHandler->startOperation();
    Node* node = new Node();
    node->key = rootKey;
    node->pos = rootPos;
    auto resNode = treeHandler->search(node, key);
    std::array< byte_t, VALUE_SIZE> res;
    if (resNode != NULL) {
        res = resNode->value;
        found = true;
    } else {
        res.fill(0);
        found = false;
    }
    treeHandler->finishOperation(true, rootKey, rootPos);
    return res;
}

void OMAP::insert(Bid key, string value) {
    treeHandler->startOperation();

    if (rootKey == 0) {
        rootKey = treeHandler->insert(0, rootPos, key, value);
    } else {
        rootKey = treeHandler->insert(rootKey, rootPos, key, value);
    }
    treeHandler->finishOperation(false, rootKey, rootPos);
}

void OMAP::insert(Bid key, std::array< byte_t, VALUE_SIZE> value) {
    treeHandler->startOperation();

    if (rootKey == 0) {
        rootKey = treeHandler->insert(0, rootPos, key, value);
    } else {
        rootKey = treeHandler->insert(rootKey, rootPos, key, value);
    }
    treeHandler->finishOperation(false, rootKey, rootPos);
}

void OMAP::printTree() {
    treeHandler->startOperation();
    Node* node = new Node();
    node->key = rootKey;
    node->pos = rootPos;
    treeHandler->printTree(node, 0);
    delete node;
    treeHandler->finishOperation(true, rootKey, rootPos);
}

string OMAP::incrementSrcCnt(Bid key) {
    string res = "";
    treeHandler->startOperation();
    Node* node = new Node();
    node->key = rootKey;
    node->pos = rootPos;
    res = treeHandler->incrementSrcCnt(node, key);
    treeHandler->finishOperation(true, rootKey, rootPos);
    return res;
}

string OMAP::getSet(Bid key, updateFn updateValue) {
    string res = "";
    treeHandler->startOperation();
    Node* node = new Node();
    node->key = rootKey;
    node->pos = rootPos;
    res = treeHandler->getSet(node, key, updateValue);
    if (res == "") {
        rootKey = treeHandler->insert(rootKey, rootPos, key, updateValue(""));
        res = "";
    }
    treeHandler->finishOperation(false, rootKey, rootPos);
    return res;
}

void OMAP::getSet(Bid key, updateArrayFn updateValue) {
    treeHandler->startOperation();
    Node* node = new Node();
    node->key = rootKey;
    node->pos = rootPos;
    bool found;
    std::array< byte_t, VALUE_SIZE> res = treeHandler->getSet(node, key, updateValue, found);
    if (!found) {
        rootKey = treeHandler->insert(rootKey, rootPos, key, updateValue(res, false));
    }
    treeHandler->finishOperation(false, rootKey, rootPos);
}

string OMAP::incrementAwCnt(Bid key) {
    string res = "";
    treeHandler->startOperation();
    Node* node = new Node();
    node->key = rootKey;
    node->pos = rootPos;
    res = treeHandler->incrementAwCnt(node, key);
    if (res == "") {
        rootKey = treeHandler->insert(rootKey, rootPos, key, "0#0");
        res = "";
    }
    treeHandler->finishOperation(false, rootKey, rootPos);
    return res;
}

/**
 * This function is used for batch insert which is used at the end of setup phase.
 */
void OMAP::batchInsert(map<Bid, string> pairs) {
    treeHandler->startOperation(true);
    int cnt = 0;
    cout << "before insert" << endl;
    for (auto pair : pairs) {
        cnt++;
        if (rootKey == 0) {
            rootKey = treeHandler->insert(0, rootPos, pair.first, pair.second);
        } else {
            rootKey = treeHandler->insert(rootKey, rootPos, pair.first, pair.second);
        }
    }
    cout << "after insert" << endl;
    treeHandler->finishOperation(false, rootKey, rootPos);
}

/**
 * This function is used for batch search which is used in the real search procedure
 */
vector<string> OMAP::batchSearch(vector<Bid> keys) {
    vector<string> result;
    treeHandler->startOperation(false);
    Node* node = new Node();
    node->key = rootKey;
    node->pos = rootPos;

    vector<Node*> resNodes;
    treeHandler->batchSearch(node, keys, &resNodes);
    for (Node* n : resNodes) {
        string res;
        if (n != NULL) {
            res.assign(n->value.begin(), n->value.end());
            result.push_back(res);
        } else {
            result.push_back("");
        }
    }
    treeHandler->finishOperation(true, rootKey, rootPos);
    return result;
}

void OMAP::setupInsert(map<Bid, string> pairs) {
    treeHandler->setupInsert(rootKey, rootPos, pairs);
}

void OMAP::setupInsert(map<Bid, std::array< byte_t, VALUE_SIZE> > pairs) {
    treeHandler->setupInsert(rootKey, rootPos, pairs);
}
