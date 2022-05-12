#ifndef ORAM_H
#define ORAM_H

#include "AESUtility.hpp"
#include <random>
#include <vector>
#include <unordered_map>
#include <string>
#include <iostream>
#include <map>
#include <set>
#include "RAMStore.hpp"
#include <bits/stdc++.h>
#include "Utilities.h"
#include "Bid.h"

using namespace std;

class Node {
public:

    Node() {
    }

    ~Node() {
    }
    Bid key;
    std::array< byte_t, 60> value;
    int pos;
    Bid leftID;
    int leftPos;
    Bid rightID;
    int rightPos;
    int evictionNode;
    int height;
};

struct Block {
    Bid id;
    block data;
};

using Bucket = std::array<Block, Z>;

struct BidHasher {

    std::size_t operator()(const Bid &key) const {
        std::hash<byte_t> hasher;
        size_t result = 0; // I would still seed this.
        for (size_t i = 0; i < ID_SIZE; ++i) {
            result = (result << 1) ^ hasher(key.id[i]); // ??
        }
        return result;
    }
};

class ORAM {
private:
    int depth;
    size_t blockSize;
    unordered_map<Bid, Node*, BidHasher> cache;
    vector<int> leafList;
    vector<int> readviewmap;
    vector<int> writeviewmap;
    set<Bid> modified;
    int readCnt = 0;
    bytes<Key> key;
    size_t plaintext_size;
    int bucketCount;

    bool batchWrite = false;
    int maxOfRandom;
    int maxHeightOfAVLTree;
    RAMStore* store;

    // Randomness
    std::random_device rd;
    std::mt19937 mt;
    std::uniform_int_distribution<int> dis;

    int RandomPath();
    int GetNodeOnPath(int leaf, int depth);
    vector<Bid> GetIntersectingBlocks(int x, int depth);

    void FetchPath(int leaf);
    void WritePath(int leaf, int level);

    Node* ReadData(Bid bid);
    void WriteData(Bid bid, Node* b);

    block SerialiseBucket(Bucket bucket);
    Bucket DeserialiseBucket(block buffer);

    Bucket ReadBucket(int pos);
    void WriteBucket(int pos, Bucket bucket);
    void Access(Bid bid, Node*& node, int lastLeaf, int newLeaf);
    void Access(Bid bid, Node*& node);



    bool WasSerialised();
    Node* convertBlockToNode(block b);
    block convertNodeToBlock(Node* node);

public:
    int totalRead = 0, totalWrite = 0;
    size_t clen_size;
    ORAM(int maxSize, bytes<Key> key);
    ~ORAM();

    Node* ReadNode(Bid bid, int lastLeaf, int newLeaf);
    Node* ReadNodeFromCache(Bid bid);
    int WriteNode(Bid bid, Node* n);
    void start(bool batchWrite);
    void finilize(bool find, Bid& rootKey, int& rootPos);
    void setupInsert(vector<Node*> nodes);

};

#endif
