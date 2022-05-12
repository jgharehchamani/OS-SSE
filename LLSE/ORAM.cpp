#include "ORAM.hpp"
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <random>
#include <cmath>
#include <cassert>
#include <cstring>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <stdlib.h>
#include <vector>

ORAM::ORAM(int maxSize, bytes<Key> oram_key)
: key(oram_key), dis(0, (int) pow(2, ceil(log2(maxSize))) - 1) {
    maxOfRandom = (int) pow(2, ceil(log2(maxSize))) - 1;
    AESUtility::Setup();
    depth = (int) (log2(maxOfRandom + 1)) + 1;
    bucketCount = (int) pow(2, depth) - 1;
    blockSize = sizeof (Node); // B
    size_t storeBucketkSize = (size_t) AESUtility::GetCiphertextLength((int) (Z * (blockSize)));
    clen_size = storeBucketkSize;
    plaintext_size = (blockSize) * Z;
    store = new RAMStore(bucketCount, storeBucketkSize);
    for (int i = 0; i < bucketCount; i++) {
        Bucket bucket;
        for (int z = 0; z < Z; z++) {
            bucket[z].id = 0;
            bucket[z].data.resize(blockSize, 0);
        }
        WriteBucket((int) i, bucket);
    }
}

ORAM::~ORAM() {
    AESUtility::Cleanup();
}

int ORAM::GetNodeOnPath(int leaf, int curDepth) {
    leaf += bucketCount / 2;
    for (int d = depth - 2; d >= curDepth; d--) {
        leaf = (leaf + 1) / 2 - 1;
    }

    return leaf;
}

// Write bucket to a single block

block ORAM::SerialiseBucket(Bucket bucket) {
    block buffer;

    for (int z = 0; z < Z; z++) {
        Block b = bucket[z];

        // Write block data
        buffer.insert(buffer.end(), b.data.begin(), b.data.end());
    }

    assert(buffer.size() == Z * (blockSize));

    return buffer;
}

Bucket ORAM::DeserialiseBucket(block buffer) {
    assert(buffer.size() == Z * (blockSize));

    Bucket bucket;

    for (int z = 0; z < Z; z++) {
        Block &curBlock = bucket[z];

        curBlock.data.assign(buffer.begin(), buffer.begin() + blockSize);
        Node* node = convertBlockToNode(curBlock.data);
        curBlock.id = node->key;
        delete node;
        buffer.erase(buffer.begin(), buffer.begin() + blockSize);
    }

    return bucket;
}

Bucket ORAM::ReadBucket(int index) {
    totalRead++;
    block ciphertext = store->Read(index);
    block buffer = AESUtility::Decrypt(key, ciphertext, clen_size);
    Bucket bucket = DeserialiseBucket(buffer);
    return bucket;
}

void ORAM::WriteBucket(int index, Bucket bucket) {
    totalWrite++;
    block b = SerialiseBucket(bucket);
    block ciphertext = AESUtility::Encrypt(key, b, clen_size, plaintext_size);
    store->Write(index, ciphertext);
}

// Fetches blocks along a path, adding them to the cache

void ORAM::FetchPath(int leaf) {
    readCnt++;
    for (int d = 0; d < depth; d++) {
        int node = GetNodeOnPath(leaf, d);

        if (find(readviewmap.begin(), readviewmap.end(), node) != readviewmap.end()) {
            continue;
        } else {
            readviewmap.push_back(node);
        }

        Bucket bucket = ReadBucket(node);

        for (int z = 0; z < Z; z++) {
            Block &curBlock = bucket[z];

            if (curBlock.id != 0) { // It isn't a dummy block   
                Node* n = convertBlockToNode(curBlock.data);
                if (cache.count(curBlock.id) == 0) {
                    cache.insert(make_pair(curBlock.id, n));
                } else {
                    delete n;
                }
            }
        }
    }
}

// Gets a list of blocks on the cache which can be placed at a specific point

std::vector<Bid> ORAM::GetIntersectingBlocks(int x, int curDepth) {
    std::vector<Bid> validBlocks;

    int node = GetNodeOnPath(x, curDepth);
    for (auto b : cache) {
        Bid bid = b.first;
        if (b.second != NULL && GetNodeOnPath(b.second->pos, curDepth) == node) {
            validBlocks.push_back(bid);
            if (validBlocks.size() >= Z) {
                return validBlocks;
            }
        }
    }
    return validBlocks;
}

// Greedily writes blocks from the cache to the tree, pushing blocks as deep into the tree as possible

void ORAM::WritePath(int leaf, int d) {
    // Find blocks that can be on this bucket
    int node = GetNodeOnPath(leaf, d);
    if (find(writeviewmap.begin(), writeviewmap.end(), node) == writeviewmap.end()) {

        auto validBlocks = GetIntersectingBlocks(leaf, d);
        // Write blocks to tree
        Bucket bucket;
        for (int z = 0; z < std::min((int) validBlocks.size(), Z); z++) {
            Block &curBlock = bucket[z];
            curBlock.id = validBlocks[z];
            Node* curnode = cache[curBlock.id];
            curBlock.data = convertNodeToBlock(curnode);
            delete curnode;
            cache.erase(curBlock.id);
        }
        // Fill any empty spaces with dummy blocks
        for (long unsigned int z = validBlocks.size(); z < Z; z++) {
            Block &curBlock = bucket[z];
            curBlock.id = 0;
            curBlock.data.resize(blockSize, 0);
        }

        // Write bucket to tree
        writeviewmap.push_back(node);
        WriteBucket(node, bucket);
    }
}

// Gets the data of a block in the cache

Node* ORAM::ReadData(Bid bid) {
    if (cache.find(bid) == cache.end()) {
        return NULL;
    }
    return cache[bid];
}

// Updates the data of a block in the cache

void ORAM::WriteData(Bid bid, Node* node) {
    cache[bid] = node;
}

// Fetches a block, allowing you to read and write in a block

void ORAM::Access(Bid bid, Node*& node, int lastLeaf, int newLeaf) {
    FetchPath(lastLeaf);
    node = ReadData(bid);
    if (node != NULL) {
        node->pos = newLeaf;
        if (cache.count(bid) != 0) {
            cache.erase(bid);
        }
        cache[bid] = node;
        if (find(leafList.begin(), leafList.end(), lastLeaf) == leafList.end()) {
            leafList.push_back(lastLeaf);
        }
    }
}

void ORAM::Access(Bid bid, Node*& node) {
    if (!batchWrite) {
        FetchPath(node->pos);
    }
    WriteData(bid, node);
    if (find(leafList.begin(), leafList.end(), node->pos) == leafList.end()) {
        leafList.push_back(node->pos);
    }
}

Node* ORAM::ReadNodeFromCache(Bid bid) {
    if (bid == 0) {
        throw runtime_error("Node id is not set");
    }
    if (cache.count(bid) == 0) {
        throw runtime_error("Node not found in the cache");
    } else {
        Node* node = cache[bid];
        return node;
    }
}

Node* ORAM::ReadNode(Bid bid, int lastLeaf, int newLeaf) {
    if (bid == 0) {
        return NULL;
    }
    if (cache.count(bid) == 0 || find(leafList.begin(), leafList.end(), lastLeaf) == leafList.end()) {
        Node* node;
        Access(bid, node, lastLeaf, newLeaf);
        if (node != NULL) {
            modified.insert(bid);
        }
        return node;
    } else {
        modified.insert(bid);
        Node* node = cache[bid];
        node->pos = newLeaf;
        return node;
    }
}

int ORAM::WriteNode(Bid bid, Node* node) {
    if (bid == 0) {
        throw runtime_error("Node id is not set");
    }
    if (cache.count(bid) == 0) {
        modified.insert(bid);
        Access(bid, node);
        return node->pos;
    } else {
        modified.insert(bid);
        return node->pos;
    }
}

Node* ORAM::convertBlockToNode(block b) {
    Node* node = new Node();
    std::array<byte_t, sizeof (Node) > arr;
    std::copy(b.begin(), b.begin() + sizeof (Node), arr.begin());
    from_bytes(arr, *node);
    return node;
}

block ORAM::convertNodeToBlock(Node* node) {
    std::array<byte_t, sizeof (Node) > data = to_bytes(*node);
    block b(data.begin(), data.end());
    return b;
}

void ORAM::finilize(bool find, Bid& rootKey, int& rootPos) {
    //fake read for padding     
    //    Utilities::startTimer(66);
    if (!batchWrite) {
        if (find) {
            for (unsigned int i = readCnt; i < depth * 1.45; i++) {
                int rnd = RandomPath();
                if (std::find(leafList.begin(), leafList.end(), rnd) == leafList.end()) {
                    leafList.push_back(rnd);
                }
                FetchPath(rnd);
            }
        } else {
            for (int i = readCnt; i < 2 * 1.45 * depth; i++) {
                int rnd = RandomPath();
                if (std::find(leafList.begin(), leafList.end(), rnd) == leafList.end()) {
                    leafList.push_back(rnd);
                }
                FetchPath(rnd);
            }
        }
    }

    for (int i = 1; i <= depth; i++) {
        for (auto t : cache) {
            if (t.second != NULL && t.second->height == i) {
                Node* tmp = t.second;
                if (modified.count(tmp->key)) {
                    tmp->pos = RandomPath();
                }
                if (tmp->leftID != 0 && cache.count(tmp->leftID) > 0) {
                    tmp->leftPos = cache[tmp->leftID]->pos;
                }
                if (tmp->rightID != 0 && cache.count(tmp->rightID) > 0) {
                    tmp->rightPos = cache[tmp->rightID]->pos;
                }
            }
        }
    }

    if (cache[rootKey] != NULL)
        rootPos = cache[rootKey]->pos;
    int cnt = 0;
    for (int d = depth - 1; d >= 0; d--) {
        for (unsigned int i = 0; i < leafList.size(); i++) {
            cnt++;
            WritePath(leafList[i], d);
            if (cnt % 1000 == 0 && batchWrite) {
                cout << "OMAP:" << cnt << "/" << (depth + 1) * leafList.size() << " inserted" << endl;
            }
        }
    }
    leafList.clear();
    modified.clear();
}

void ORAM::start(bool isBatchWrite) {
    this->batchWrite = isBatchWrite;
    writeviewmap.clear();
    readviewmap.clear();
    readCnt = 0;
}

int ORAM::RandomPath() {
    int val = dis(mt);
    return val;
}

void ORAM::setupInsert(vector<Node*> nodes) {
    sort(nodes.begin(), nodes.end(), [ ](const Node* lhs, const Node * rhs) {
        return lhs->pos < rhs->pos;
    });
    int curPos = 0;
    if (nodes.size() > 0) {
        curPos = nodes[0]->pos;
    }
    map<int, Bucket> buckets;
    map<int, int> bucketsCnt;
    unsigned int i = 0;
    bool cannotInsert = false;
    int cnt = 0;
    while (i < nodes.size()) {
        cnt++;
        if (cnt % 1000 == 0) {
            cout << "i:" << i << "/" << nodes.size() << endl;
        }
        for (int d = depth - 1; d >= 0 && i < nodes.size() && curPos == nodes[i]->pos; d--) {
            int nodeIndex = GetNodeOnPath(curPos, d);
            Bucket bucket;
            if (bucketsCnt.count(nodeIndex) == 0) {
                bucketsCnt[nodeIndex] = 0;
                for (int z = 0; z < Z; z++) {
                    if (i < nodes.size() && nodes[i]->pos == curPos) {
                        Block &curBlock = bucket[z];
                        curBlock.id = nodes[i]->key;
                        curBlock.data = convertNodeToBlock(nodes[i]);
                        delete nodes[i];
                        bucketsCnt[nodeIndex]++;
                        i++;
                    }
                }
                buckets[nodeIndex] = bucket;
            } else {
                if (bucketsCnt[nodeIndex] < Z) {
                    bucket = buckets[nodeIndex];
                    for (int z = bucketsCnt[nodeIndex]; z < Z; z++) {
                        if (i < nodes.size() && nodes[i]->pos == curPos) {
                            Block &curBlock = bucket[z];
                            curBlock.id = nodes[i]->key;
                            curBlock.data = convertNodeToBlock(nodes[i]);
                            delete nodes[i];
                            bucketsCnt[nodeIndex]++;
                            i++;
                        }
                    }
                    buckets[nodeIndex] = bucket;
                } else {
                    cannotInsert = true;
                }
            }

        }

        if (i < nodes.size()) {
            if (cannotInsert) {
                cache[nodes[i]->key] = nodes[i];
                i++;
                cannotInsert = false;
            }
            if (i < nodes.size()) {
                curPos = nodes[i]->pos;
            }
        }
    }


    for (auto buk : buckets) {
        if (bucketsCnt[buk.first] == Z) {
            WriteBucket(buk.first, buk.second);
        } else {
            for (long unsigned int z = bucketsCnt[buk.first]; z < Z; z++) {
                Block &curBlock = buk.second[z];
                curBlock.id = 0;
                curBlock.data.resize(blockSize, 0);
            }
            WriteBucket(buk.first, buk.second);
        }
    }

    for (; i < nodes.size(); i++) {
        cache[nodes[i]->key] = nodes[i];
    }
}
