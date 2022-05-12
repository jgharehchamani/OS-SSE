#include "RAMStore.hpp"
#include <iostream>
#include "ORAM.hpp"
using namespace std;

RAMStore::RAMStore(size_t count, size_t size)
: store(count), size(size) {
}

RAMStore::~RAMStore() {
}

block RAMStore::Read(int pos) {
    return store[pos];
}

void RAMStore::Write(int pos, block b) {
    store[pos] = b;
}
