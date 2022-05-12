#include "Server.h"
#include "OMAP.h"
#include "Utilities.h"

Server::~Server() {
}

Server::Server(int N) {
    this->N = N;
    this->height = (int) ceil(log2(this->N)) + 1; //root is level 1
    firstLeaf = (pow(2, height - 1) - 1);
}

void Server::insert(prf_type key, prf_type value) {
    put(key, value);
    existingEntries++;
}

void Server::insert(vector<prf_type> keys, vector<prf_type> values) {
    for (unsigned int i = 0; i < keys.size(); i++) {
        put(keys[i], values[i]);
        existingEntries++;
    }
}

int Server::smallest_power_of_two_greater_than(int n) {
    int k = 1;
    while (k > 0 && k < n) {
        k = k << 1;
    }
    return k;
}

Result Server::search(prf_type Ikey, prf_type Dkey, prf_type tk_0, int a_w) {
    Result result;
    if (oldKV.count(tk_0) == 0) {
        oldKV[tk_0] = unordered_map<int, ResultRow>();
        ACCs[tk_0] = unordered_map<int, int>();
    }
    vector<int> t_i = Utilities::findCoveringSet(a_w, height);
    int nextPowerOf2 = smallest_power_of_two_greater_than(a_w + 2);
    int rootOfSubtree = Utilities::getNodeOnPath(0, height - log2(nextPowerOf2), height);
    int newRoot;
    newRoot = (rootOfSubtree + 1) / 2 - 1;

    recursiveSearch(Ikey, Dkey, tk_0, newRoot, &result, &t_i, false);
    existingEntries = 0;
    return result;
}

void Server::recursiveSearch(prf_type Ikey, prf_type Dkey, prf_type tk_0, int index, Result* result, vector<int>* coverSet, bool indexIsCoverSet) {
    prf_type tmpKey;
    bool tmp;
    int acc = (index >= firstLeaf) ? 0 : getLeftBound(index, tk_0, tmp);
    memset(tmpKey.data(), 0, AES_KEY_SIZE);
    *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 5])) = index;
    *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 9])) = acc;
    prf_type fkey = Utilities::generatePRF(tmpKey.data(), Ikey.data());
    if (index >= firstLeaf) {
        if (KV.count(fkey) == 0) {
            result->insert(oldKV[tk_0][index]);
        } else {
            prf_type leafData = remove(fkey, true);
            ResultRow row = result->insert(index, true, leafData, 0);
            oldKV[tk_0][index] = row;
        }
    } else {
        int leftIndex = -1, rightIndex = -1;
        if (oldKV[tk_0].count(index) == 0 && !contain(fkey, true)) {
            leftIndex = index * 2 + 1;
            rightIndex = index * 2 + 2;
        } else {
            int maxAcc;
            bool oldRecord = false;
            prf_type curRes = findMaxAcc(Ikey, Dkey, tk_0, index, fkey, maxAcc, oldRecord);

            if (oldRecord) {
                leftIndex = oldKV[tk_0][index].leftChild;
                rightIndex = oldKV[tk_0][index].rightChild;
            } else {
                prf_type mapValue;
                Utilities::decode(curRes, mapValue, Dkey.data());
                ResultRow row;
                row.set(index, false, curRes, maxAcc);
                leftIndex = *(int*) (&(mapValue.data()[AES_KEY_SIZE - 5]));
                rightIndex = *(int*) (&(mapValue.data()[AES_KEY_SIZE - 9]));
                row.leftChild = leftIndex;
                row.rightChild = rightIndex;
                oldKV[tk_0][index] = row;
            }


        }
        if (!indexIsCoverSet) {
            bool isLCover = false;
            bool shouldDig = subtreeContainCoverSet(leftIndex, coverSet, isLCover);
            if (shouldDig && leftIndex != -1) {
                recursiveSearch(Ikey, Dkey, tk_0, leftIndex, result, coverSet, isLCover);
            }
            bool isRCover = false;
            shouldDig = subtreeContainCoverSet(rightIndex, coverSet, isRCover);
            if (shouldDig && rightIndex != -1) {
                recursiveSearch(Ikey, Dkey, tk_0, rightIndex, result, coverSet, isRCover);
            }
        } else {
            if (leftIndex != -1)
                recursiveSearch(Ikey, Dkey, tk_0, leftIndex, result, coverSet, true);
            if (rightIndex != -1)
                recursiveSearch(Ikey, Dkey, tk_0, rightIndex, result, coverSet, true);
        }
    }
}

prf_type Server::findMaxAcc(prf_type Ikey, prf_type Dkey, prf_type tk_0, int index, prf_type keyForAcc0, int& maxAcc, bool& oldRecord) {
    prf_type result;
    int h = (index == -1) ? (height + 1) : (height - (floor(log2(index + 1))));
    bool recordExist = false;
    int originalLeftBound = getLeftBound(index, tk_0, recordExist);
    int leftBound = originalLeftBound + 1;

    int rightBound = 2 * h - 1 + leftBound;
    exponentialSearch(Ikey, Dkey, tk_0, index, leftBound, rightBound, maxAcc, keyForAcc0);
    prf_type lastKey;
    if (rightBound != -1) {
        while (leftBound < rightBound) {
            int currentAcc = (leftBound + rightBound) / 2;

            prf_type tmpKey;
            memset(tmpKey.data(), 0, AES_KEY_SIZE);
            *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 5])) = index;
            *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 9])) = currentAcc;
            prf_type fkey = Utilities::generatePRF(tmpKey.data(), Ikey.data());
            if (contain(fkey, false)) {
                lastKey = fkey;
                leftBound = currentAcc + 1;
            } else {
                rightBound = currentAcc;
            }
        }
        prf_type tmpKey;
        memset(tmpKey.data(), 0, AES_KEY_SIZE);
        *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 5])) = index;
        *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 9])) = leftBound;
        prf_type fkey = Utilities::generatePRF(tmpKey.data(), Ikey.data());

        if (contain(fkey, false)) {
            result = remove(fkey, false);
            maxAcc = leftBound;
        } else {
            result = remove(lastKey, false);
            maxAcc = leftBound - 1;
        }
    } else {
        result = remove(keyForAcc0, false);
    }
    if (recordExist && maxAcc <= originalLeftBound && oldKV[tk_0].count(index) != 0) {
        oldRecord = true;
    } else {
        ACCs[tk_0][index] = maxAcc;
    }
    return result;
}

void Server::exponentialSearch(prf_type Ikey, prf_type Dkey, prf_type tk_0, int index, int& leftBound, int& rightBound, int& maxAcc, prf_type& lastKey) {
    int currentAcc = leftBound;

    prf_type tmpKey;
    memset(tmpKey.data(), 0, AES_KEY_SIZE);
    *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 5])) = index;
    *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 9])) = currentAcc;
    prf_type fkey2 = Utilities::generatePRF(tmpKey.data(), Ikey.data());
    if (contain(fkey2, false)) {
        currentAcc = leftBound + 2;
        memset(tmpKey.data(), 0, AES_KEY_SIZE);
        *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 5])) = index;
        *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 9])) = currentAcc;
        prf_type fkey = Utilities::generatePRF(tmpKey.data(), Ikey.data());
        if (contain(fkey, false)) {
            int i = 4;
            do {
                currentAcc = leftBound + i;
                memset(tmpKey.data(), 0, AES_KEY_SIZE);
                *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 5])) = index;
                *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 9])) = currentAcc;
                prf_type fkey = Utilities::generatePRF(tmpKey.data(), Ikey.data());
                if (contain(fkey, false)) {
                    leftBound = i;
                    i = i * 2;
                } else {
                    rightBound = currentAcc;
                    break;
                }
            } while (currentAcc < rightBound);
        } else {
            currentAcc = leftBound + 1;
            memset(tmpKey.data(), 0, AES_KEY_SIZE);
            *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 5])) = index;
            *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 9])) = currentAcc;
            fkey = Utilities::generatePRF(tmpKey.data(), Ikey.data());
            if (contain(fkey, false)) {
                maxAcc = leftBound + 1;
                rightBound = -1;
                lastKey = fkey;
            } else {
                maxAcc = leftBound;
                rightBound = -1;
                lastKey = fkey2;
            }
        }

    } else {
        maxAcc = leftBound - 1;
        rightBound = -1;
    }
}

prf_type Server::get(prf_type key) {
    mapAccess++;
    return KV[key];
}

prf_type Server::remove(prf_type key, bool incrementCounter) {
    if (incrementCounter) {
        mapAccess++;
    }
    prf_type result = KV[key];
    KV.erase(key);
    return result;
}

void Server::put(prf_type key, prf_type value) {
    KV[key] = value;
}

bool Server::contain(prf_type key, bool incrementCounter) {
    if (incrementCounter) {
        mapAccess++;
    }
    if (KV.count(key) == 0) {
        return false;
    } else {
        return true;
    }
}

void Server::reset() {
    KV.clear();
    oldKV.clear();
    ACCs.clear();
    mapAccess = 0;
}

bool Server::subtreeContainCoverSet(int index, vector<int>* coverSet, bool& indexIsCoverSet) {
    bool contain = false;
    indexIsCoverSet = false;
    for (int root : (*coverSet)) {
        int tmpIndex = index;
        if (root == tmpIndex) {
            indexIsCoverSet = true;
        }
        if (root > tmpIndex) {
            while (root > tmpIndex) {
                root = (root + 1) / 2 - 1;
            }
            if (root == tmpIndex) {
                contain = true;
            }
        } else {
            while (root < tmpIndex) {
                tmpIndex = (tmpIndex + 1) / 2 - 1;
            }
            if (root == tmpIndex) {
                contain = true;
            }
        }
    }
    return contain;
}

int Server::getLeftBound(int index, prf_type tk_0, bool& found) {
    if (ACCs[tk_0].count(index) == 0) {
        found = false;
        return 0;
    } else {
        int acc = ACCs[tk_0][index];
        found = true;
        return acc;
    }
}
