#include "AVLTree.h"
#include <vector>
#include <algorithm>
#include "Utilities.h"

AVLTree::AVLTree(int maxSize, bytes<Key> key) {
    oram = new ORAM(maxSize, key);
    maxOfRandom = (int) pow(2, ceil(log2(maxSize))) - 1;
}

AVLTree::~AVLTree() {
    delete oram;
}

// A utility function to get height of the tree

int AVLTree::height(Bid key, int& leaf) {
    if (key == 0)
        return 0;
    Node* node = oram->ReadNode(key, leaf, leaf);
    return node->height;
}

// A utility function to get maximum of two integers

int AVLTree::max(int a, int b) {
    return (a > b) ? a : b;
}

/* Helper function that allocates a new node with the given key and
   NULL left and right pointers. */
Node* AVLTree::newNode(Bid key, string value) {
    Node* node = new Node();
    node->key = key;
    std::fill(node->value.begin(), node->value.end(), 0);
    std::copy(value.begin(), value.end(), node->value.begin());
    node->leftID = 0;
    node->leftPos = -1;
    node->rightPos = -1;
    node->rightID = 0;
    node->pos = RandomPath();
    node->height = 1; // new node is initially added at leaf
    return node;
}

Node* AVLTree::newNode(Bid key, std::array< byte_t, VALUE_SIZE> value) {
    Node* node = new Node();
    node->key = key;
    std::copy(value.begin(), value.end(), node->value.begin());
    node->leftID = 0;
    node->leftPos = -1;
    node->rightPos = -1;
    node->rightID = 0;
    node->pos = RandomPath();
    node->height = 1; // new node is initially added at leaf
    return node;
}

// A utility function to right rotate subtree rooted with y
// See the diagram given above.

Node* AVLTree::rightRotate(Node* y) {
    Node* x = oram->ReadNodeFromCache(y->leftID);
    Node* T2;
    if (x->rightID == 0) {
        T2 = newNode(0, "");
    } else {
        T2 = oram->ReadNodeFromCache(x->rightID);
    }

    // Perform rotation
    x->rightID = y->key;
    x->rightPos = y->pos;
    y->leftID = T2->key;
    y->leftPos = T2->pos;

    // Update heights
    y->height = max(height(y->leftID, y->leftPos), height(y->rightID, y->rightPos)) + 1;
    oram->WriteNode(y->key, y);
    x->height = max(height(x->leftID, x->leftPos), height(x->rightID, x->rightPos)) + 1;
    oram->WriteNode(x->key, x);
    // Return new root

    return x;
}

// A utility function to left rotate subtree rooted with x
// See the diagram given above.

Node* AVLTree::leftRotate(Node* x) {
    Node* y = oram->ReadNodeFromCache(x->rightID);
    Node* T2;
    if (y->leftID == 0) {
        T2 = newNode(0, "");
    } else {
        T2 = oram->ReadNodeFromCache(y->leftID);
    }


    // Perform rotation
    y->leftID = x->key;
    y->leftPos = x->pos;
    x->rightID = T2->key;
    x->rightPos = T2->pos;

    // Update heights
    x->height = max(height(x->leftID, x->leftPos), height(x->rightID, x->rightPos)) + 1;
    oram->WriteNode(x->key, x);
    y->height = max(height(y->leftID, y->leftPos), height(y->rightID, y->rightPos)) + 1;
    oram->WriteNode(y->key, y);
    // Return new root
    return y;
}

// Get Balance factor of node N

int AVLTree::getBalance(Node* N) {
    if (N == NULL)
        return 0;
    return height(N->leftID, N->leftPos) - height(N->rightID, N->rightPos);
}

Bid AVLTree::insert(Bid rootKey, int& pos, Bid key, std::array< byte_t, VALUE_SIZE> value) {
    /* 1. Perform the normal BST rotation */
    if (rootKey == 0) {
        Node* nnode = newNode(key, value);
        pos = oram->WriteNode(key, nnode);
        return nnode->key;
    }
    Node* node = oram->ReadNode(rootKey, pos, pos);
    if (key < node->key) {
        node->leftID = insert(node->leftID, node->leftPos, key, value);
    } else if (key > node->key) {
        node->rightID = insert(node->rightID, node->rightPos, key, value);
    } else {
        std::fill(node->value.begin(), node->value.end(), 0);
        std::copy(value.begin(), value.end(), node->value.begin());
        oram->WriteNode(rootKey, node);
        return node->key;
    }

    /* 2. Update height of this ancestor node */
    node->height = max(height(node->leftID, node->leftPos), height(node->rightID, node->rightPos)) + 1;

    /* 3. Get the balance factor of this ancestor node to check whether
       this node became unbalanced */
    int balance = getBalance(node);

    // If this node becomes unbalanced, then there are 4 cases

    // Left Left Case
    if (balance > 1 && key < oram->ReadNodeFromCache(node->leftID)->key) {
        Node* res = rightRotate(node);
        pos = res->pos;
        return res->key;
    }

    // Right Right Case
    if (balance < -1 && key > oram->ReadNodeFromCache(node->rightID)->key) {
        Node* res = leftRotate(node);
        pos = res->pos;
        return res->key;
    }

    // Left Right Case
    if (balance > 1 && key > oram->ReadNodeFromCache(node->leftID)->key) {
        Node* res = leftRotate(oram->ReadNodeFromCache(node->leftID));
        node->leftID = res->key;
        node->leftPos = res->pos;
        oram->WriteNode(node->key, node);
        Node* res2 = rightRotate(node);
        pos = res2->pos;
        return res2->key;
    }

    // Right Left Case
    if (balance < -1 && key < oram->ReadNodeFromCache(node->rightID)->key) {
        auto res = rightRotate(oram->ReadNodeFromCache(node->rightID));
        node->rightID = res->key;
        node->rightPos = res->pos;
        oram->WriteNode(node->key, node);
        auto res2 = leftRotate(node);
        pos = res2->pos;
        return res2->key;
    }

    /* return the (unchanged) node pointer */
    oram->WriteNode(node->key, node);
    return node->key;
}

Bid AVLTree::insert(Bid rootKey, int& pos, Bid key, string value) {
    /* 1. Perform the normal BST rotation */
    if (rootKey == 0) {
        Node* nnode = newNode(key, value);
        pos = oram->WriteNode(key, nnode);
        return nnode->key;
    }
    Node* node = oram->ReadNode(rootKey, pos, pos);
    if (key < node->key) {
        node->leftID = insert(node->leftID, node->leftPos, key, value);
    } else if (key > node->key) {
        node->rightID = insert(node->rightID, node->rightPos, key, value);
    } else {
        std::fill(node->value.begin(), node->value.end(), 0);
        std::copy(value.begin(), value.end(), node->value.begin());
        oram->WriteNode(rootKey, node);
        return node->key;
    }

    /* 2. Update height of this ancestor node */
    node->height = max(height(node->leftID, node->leftPos), height(node->rightID, node->rightPos)) + 1;

    /* 3. Get the balance factor of this ancestor node to check whether
       this node became unbalanced */
    int balance = getBalance(node);

    // If this node becomes unbalanced, then there are 4 cases

    // Left Left Case
    if (balance > 1 && key < oram->ReadNodeFromCache(node->leftID)->key) {
        Node* res = rightRotate(node);
        pos = res->pos;
        return res->key;
    }

    // Right Right Case
    if (balance < -1 && key > oram->ReadNodeFromCache(node->rightID)->key) {
        Node* res = leftRotate(node);
        pos = res->pos;
        return res->key;
    }

    // Left Right Case
    if (balance > 1 && key > oram->ReadNodeFromCache(node->leftID)->key) {
        Node* res = leftRotate(oram->ReadNodeFromCache(node->leftID));
        node->leftID = res->key;
        node->leftPos = res->pos;
        oram->WriteNode(node->key, node);
        Node* res2 = rightRotate(node);
        pos = res2->pos;
        return res2->key;
    }

    // Right Left Case
    if (balance < -1 && key < oram->ReadNodeFromCache(node->rightID)->key) {
        auto res = rightRotate(oram->ReadNodeFromCache(node->rightID));
        node->rightID = res->key;
        node->rightPos = res->pos;
        oram->WriteNode(node->key, node);
        auto res2 = leftRotate(node);
        pos = res2->pos;
        return res2->key;
    }

    /* return the (unchanged) node pointer */
    oram->WriteNode(node->key, node);
    return node->key;
}

/**
 * a recursive search function which traverse binary tree to find the target node
 */
Node* AVLTree::search(Node* head, Bid key) {
    if (head == NULL || head->key == 0)
        return head;
    head = oram->ReadNode(head->key, head->pos, head->pos);
    if (head->key > key) {
        return search(oram->ReadNode(head->leftID, head->leftPos, head->leftPos), key);
    } else if (head->key < key) {
        return search(oram->ReadNode(head->rightID, head->rightPos, head->rightPos), key);
    } else
        return head;
}

/**
 * a recursive search function which traverse binary tree to find the target node
 */
void AVLTree::batchSearch(Node* head, vector<Bid> keys, vector<Node*>* results) {
    if (head == NULL || head->key == 0) {
        return;
    }
    head = oram->ReadNode(head->key, head->pos, head->pos);
    bool getLeft = false, getRight = false;
    vector<Bid> leftkeys, rightkeys;
    for (Bid bid : keys) {
        if (head->key > bid) {
            getLeft = true;
            leftkeys.push_back(bid);
        }
        if (head->key < bid) {
            getRight = true;
            rightkeys.push_back(bid);
        }
        if (head->key == bid) {
            results->push_back(head);
        }
    }
    if (getLeft) {
        batchSearch(oram->ReadNode(head->leftID, head->leftPos, head->leftPos), leftkeys, results);
    }
    if (getRight) {
        batchSearch(oram->ReadNode(head->rightID, head->rightPos, head->rightPos), rightkeys, results);
    }
}

void AVLTree::printTree(Node* root, int indent) {
    if (root != 0 && root->key != 0) {
        root = oram->ReadNode(root->key, root->pos, root->pos);
        if (root->leftID != 0)
            printTree(oram->ReadNode(root->leftID, root->leftPos, root->leftPos), indent + 4);
        if (indent > 0) {
            for (int i = 0; i < indent; i++) {
                printf(" ");
            }
        }

        string value;
        value.assign(root->value.begin(), root->value.end());
        string key;
        Bid tmpKey = root->key;
        int id = *(int*) (&(tmpKey.id.data()[AES_KEY_SIZE - 4]));
        for (int i = 0; i < tmpKey.id.size(); i++) {
            if (tmpKey.id[i] == 0) {
                tmpKey.id[i] = '+';
            }
        }
        key.assign(tmpKey.id.begin(), tmpKey.id.end());
        key = key + "+" + to_string(id);
        printf("%s:%d:%s:%d:%d:%d:%d:%d\n", key.c_str(), root->height, value.c_str(), root->pos, root->leftID.getValue(), root->leftPos, root->rightID.getValue(), root->rightPos);
        if (root->rightID != 0)
            printTree(oram->ReadNode(root->rightID, root->rightPos, root->rightPos), indent + 4);

    }
}

/*
 * before executing each operation, this function should be called with proper arguments
 */
void AVLTree::startOperation(bool batchWrite) {
    oram->start(batchWrite);
}

/*
 * after executing each operation, this function should be called with proper arguments
 */
void AVLTree::finishOperation(bool find, Bid& rootKey, int& rootPos) {
    oram->finilize(find, rootKey, rootPos);
}

int AVLTree::RandomPath() {
    return rand() % (maxOfRandom + 1);
}

void AVLTree::setupInsert(Bid& rootKey, int& rootPos, map<Bid, string> pairs) {
    for (auto pair : pairs) {
        Node* node = newNode(pair.first, pair.second);
        setupNodes.push_back(node);
    }
    cout << "Creating BST" << endl;
    sortedArrayToBST(0, setupNodes.size() - 1, rootPos, rootKey);
    cout << "Inserting in ORAM" << endl;
    oram->setupInsert(setupNodes);
}

void AVLTree::setupInsert(Bid& rootKey, int& rootPos, map<Bid, std::array< byte_t, VALUE_SIZE> > pairs) {
    for (auto pair : pairs) {
        Node* node = newNode(pair.first, pair.second);
        setupNodes.push_back(node);
    }
    cout << "Creating BST" << endl;
    sortedArrayToBST(0, setupNodes.size() - 1, rootPos, rootKey);
    cout << "Inserting in ORAM" << endl;
    oram->setupInsert(setupNodes);
}

int AVLTree::sortedArrayToBST(int start, int end, int& pos, Bid& node) {
    setupProgress++;
    if (setupProgress % 100000 == 0) {
        cout << setupProgress << "/" << setupNodes.size()*2 << " of AVL tree constructed" << endl;
    }
    if (start > end) {
        pos = -1;
        node = 0;
        return 0;
    }

    int mid = (start + end) / 2;
    Node *root = setupNodes[mid];

    int leftHeight = sortedArrayToBST(start, mid - 1, root->leftPos, root->leftID);

    int rightHeight = sortedArrayToBST(mid + 1, end, root->rightPos, root->rightID);
    root->pos = RandomPath();
    root->height = max(leftHeight, rightHeight) + 1;
    pos = root->pos;
    node = root->key;
    return root->height;
}

string AVLTree::getSet(Node* head, Bid key, updateFn updateValue) {
    if (head == NULL || head->key == 0)
        return "";
    head = oram->ReadNode(head->key, head->pos, head->pos);
    if (head->key > key) {
        return getSet(oram->ReadNode(head->leftID, head->leftPos, head->leftPos), key, updateValue);
    } else if (head->key < key) {
        return getSet(oram->ReadNode(head->rightID, head->rightPos, head->rightPos), key, updateValue);
    } else {
        string res(head->value.begin(), head->value.end());
        string newval = updateValue(res);
        std::fill(head->value.begin(), head->value.end(), 0);
        std::copy(newval.begin(), newval.end(), head->value.begin());
        oram->WriteNode(key, head);
        return res;
    }
}

std::array< byte_t, VALUE_SIZE> AVLTree::getSet(Node* head, Bid key, updateArrayFn updateValue, bool& found) {
    if (head == NULL || head->key == 0) {
        found = false;
        return std::array< byte_t, VALUE_SIZE>();
    }
    head = oram->ReadNode(head->key, head->pos, head->pos);
    if (head->key > key) {
        return getSet(oram->ReadNode(head->leftID, head->leftPos, head->leftPos), key, updateValue, found);
    } else if (head->key < key) {
        return getSet(oram->ReadNode(head->rightID, head->rightPos, head->rightPos), key, updateValue, found);
    } else {
        std::array< byte_t, VALUE_SIZE> res = head->value;
        std::array< byte_t, VALUE_SIZE> newval = updateValue(res, true);
        head->value = newval;
        oram->WriteNode(key, head);
        found = true;
        return res;
    }
}

string AVLTree::incrementSrcCnt(Node* head, Bid key) {
    if (head == NULL || head->key == 0)
        return "";
    head = oram->ReadNode(head->key, head->pos, head->pos);
    if (head->key > key) {
        return incrementSrcCnt(oram->ReadNode(head->leftID, head->leftPos, head->leftPos), key);
    } else if (head->key < key) {
        return incrementSrcCnt(oram->ReadNode(head->rightID, head->rightPos, head->rightPos), key);
    } else {
        string res(head->value.begin(), head->value.end());
        vector<string> parts = Utilities::splitData(res, "#");
        int src_cnt = stoi(parts[0]);
        string newval = to_string(src_cnt + 1) + "#" + parts[1];
        std::fill(head->value.begin(), head->value.end(), 0);
        std::copy(newval.begin(), newval.end(), head->value.begin());
        oram->WriteNode(key, head);
        return res;
    }
}

string AVLTree::incrementAwCnt(Node* head, Bid key) {
    if (head == NULL || head->key == 0)
        return "";
    head = oram->ReadNode(head->key, head->pos, head->pos);
    if (head->key > key) {
        return incrementAwCnt(oram->ReadNode(head->leftID, head->leftPos, head->leftPos), key);
    } else if (head->key < key) {
        return incrementAwCnt(oram->ReadNode(head->rightID, head->rightPos, head->rightPos), key);
    } else {
        string res(head->value.begin(), head->value.end());
        vector<string> parts = Utilities::splitData(res, "#");
        int aw_cnt = stoi(parts[1]);
        string newval = parts[0] + "#" + to_string(aw_cnt + 1);
        std::fill(head->value.begin(), head->value.end(), 0);
        std::copy(newval.begin(), newval.end(), head->value.begin());
        oram->WriteNode(key, head);
        return res;
    }
}
