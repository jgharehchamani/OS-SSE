#include "Server.h"
#include "OMAP.h"
#include "Utilities.h"

Server::~Server() {
}

Server::Server(int N) {
    this->N = N;
    this->height = (int) ceil(log2(this->N)) + 1; //root is level 1
    firstLeaf = (pow(2, height - 1) - 1);
    KV.reserve(2 * N);
}

void Server::insert(prf_type key, prf_type value) {
    put(key, value);
}

void Server::insert(vector<prf_type> keys, vector<prf_type> values) {
    for (unsigned int i = 0; i < keys.size(); i++) {
        put(keys[i], values[i]);
    }
}

Result Server::search(prf_type keywordToken, prf_type Ikey, prf_type Dkey, int a_w, int acc, int coverSetStatus, int coverSetOne) {
    mapAccess = 0;
    unordered_map<prf_type, unordered_map<int, ResultRow>, PRFHasher> tmpResult = previousResult;
    Result result(&tmpResult);
    vector<int> t_i = Utilities::findCoveringSet(a_w, height);
    bool check = true;
    for (int i = 0; i < t_i.size() - 1; i++) {
        if (coverSetStatus & (1 << (i))) {
            check = false;
        }
    }
    if (t_i[t_i.size() - 1] >= firstLeaf && check && (coverSetStatus & (1 << (t_i.size() - 1)))) {
        prf_type tmpKey;
        int index = t_i[t_i.size() - 1];
        memset(tmpKey.data(), 0, AES_KEY_SIZE);
        *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 5])) = index;
        *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 9])) = 0;
        prf_type fkey = Utilities::generatePRF(tmpKey.data(), Ikey.data());
        if (previousResult.count(keywordToken) != 0 && previousResult[keywordToken].count(index) != 0) {
            result.insert(previousResult[keywordToken][index], keywordToken);
        } else {
            if (KV.count(fkey) != 0) {
                result.insert(index, true, remove(fkey, true), 0, 0, 0, 0, 0, Ikey, keywordToken);
            } else {
                printf("not found\n");
            }
        }
        return result;
    }

    int nextPowerOf2 = Utilities::smallest_power_of_two_greater_than(a_w + 2);
    int rootOfSubtree = Utilities::getNodeOnPath(0, height - log2(nextPowerOf2), height);
    int newRoot;
    newRoot = (rootOfSubtree + 1) / 2 - 1;

    if (newRoot == -1) {
        prf_type tmpKey;
        memset(tmpKey.data(), 0, AES_KEY_SIZE);
        *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 5])) = newRoot;
        *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 9])) = acc;
        prf_type fkey = Utilities::generatePRF(tmpKey.data(), Ikey.data());
        int leftIndex = -1, rightIndex = -1, leftAcc = -1, rightAcc = -1;
        if (!contain(fkey, true)) {
            if (acc > 0) {
                if (previousResult.count(keywordToken) != 0 && previousResult[keywordToken].count(-1) != 0) {
                    result.insert(previousResult[keywordToken][-1], keywordToken);
                    bool isCover = false;
                    bool shouldDig = subtreeContainCoverSet(previousResult[keywordToken][-1].rightChild, &t_i, isCover);
                    if (shouldDig) {
                        recursiveSearch(keywordToken, Ikey, Dkey, previousResult[keywordToken][-1].rightChild, previousResult[keywordToken][-1].rightAcc, &result, &t_i, isCover, a_w);
                    }
                }
            } else {
                recursiveSearch(keywordToken, Ikey, Dkey, 0, 0, &result, &t_i, false, a_w);
            }
        } else {
            prf_type curRes = remove(fkey, false);
            prf_type mapValue;
            Utilities::decode(curRes, mapValue, Dkey.data());
            leftIndex = *(int*) (&(mapValue.data()[AES_KEY_SIZE - 5]));
            rightIndex = *(int*) (&(mapValue.data()[AES_KEY_SIZE - 12]));
            *(int*) (&(mapValue.data()[AES_KEY_SIZE - 5])) = 0;
            *(int*) (&(mapValue.data()[AES_KEY_SIZE - 12])) = 0;
            leftAcc = *(int*) (&(mapValue.data()[AES_KEY_SIZE - 8]));
            rightAcc = *(int*) (&(mapValue.data()[AES_KEY_SIZE - 15]));

            result.insert(newRoot, false, curRes, acc, leftIndex, leftAcc, rightIndex, rightAcc, Ikey, keywordToken);
            bool isCover = false;
            bool shouldDig = subtreeContainCoverSet(rightIndex, &t_i, isCover);
            if (shouldDig) {
                recursiveSearch(keywordToken, Ikey, Dkey, rightIndex, rightAcc, &result, &t_i, isCover, a_w);
            }
        }
    } else {
        recursiveSearch(keywordToken, Ikey, Dkey, newRoot, acc, &result, &t_i, false, a_w);
    }

    previousResultAw[keywordToken] = a_w;
    previousResult = tmpResult;
    return result;
}

void Server::recursiveSearch(prf_type keywordToken, prf_type Ikey, prf_type Dkey, int index, int acc, Result* result, vector<int>* coverSet, bool indexIsCoverSet, int a_w) {
    prf_type tmpKey;
    memset(tmpKey.data(), 0, AES_KEY_SIZE);
    *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 5])) = index;
    *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 9])) = (index >= firstLeaf ? 0 : acc);
    prf_type fkey = Utilities::generatePRF(tmpKey.data(), Ikey.data());
    if (index >= firstLeaf) {
        if (previousResult.count(keywordToken) != 0 && previousResult[keywordToken].count(index) != 0) {
            result->insert(previousResult[keywordToken][index], keywordToken);
        } else {
            result->insert(index, true, remove(fkey, true), 0, 0, 0, 0, 0, Ikey, keywordToken);
        }
    } else {
        int leftIndex = -1, rightIndex = -1, leftAcc = -1, rightAcc = -1;
        if (!contain(fkey, true)) {
            if (previousResult.count(keywordToken) == 0 || previousResult[keywordToken].count(index) == 0 || previousResult[keywordToken][index].acc != acc) {
                if (indexIsCoverSet) {
                    addSubtreeLeaves(Ikey, keywordToken, result, index);
                    return;
                } else {
                    leftIndex = index * 2 + 1;
                    leftAcc = 0;
                    rightIndex = index * 2 + 2;
                    rightAcc = 0;
                }
            } else {
                findPreviousSearchResult(Ikey, keywordToken, index, result, a_w);
                return;
            }
        } else {
            prf_type curRes = remove(fkey, false);
            prf_type mapValue;
            Utilities::decode(curRes, mapValue, Dkey.data());
            leftIndex = *(int*) (&(mapValue.data()[AES_KEY_SIZE - 5]));
            rightIndex = *(int*) (&(mapValue.data()[AES_KEY_SIZE - 12]));
            *(int*) (&(mapValue.data()[AES_KEY_SIZE - 5])) = 0;
            *(int*) (&(mapValue.data()[AES_KEY_SIZE - 12])) = 0;
            leftAcc = *(int*) (&(mapValue.data()[AES_KEY_SIZE - 8]));
            rightAcc = *(int*) (&(mapValue.data()[AES_KEY_SIZE - 15]));

            result->insert(index, false, curRes, acc, leftIndex, leftAcc, rightIndex, rightAcc, Ikey, keywordToken);
        }
        if (!indexIsCoverSet) {
            bool isLCover = false;
            int checkIndex = leftIndex;
            if (leftIndex == -1) {
                checkIndex = 2 * index + 1;
            }
            bool shouldDig = subtreeContainCoverSet(checkIndex, coverSet, isLCover);
            if (shouldDig && leftIndex != -1) {
                if (previousResult.count(keywordToken) == 0 || previousResult[keywordToken].count(leftIndex) == 0 || previousResult[keywordToken][leftIndex].acc != leftAcc) {
                    recursiveSearch(keywordToken, Ikey, Dkey, leftIndex, leftAcc, result, coverSet, isLCover, a_w);
                } else {
                    findPreviousSearchResult(Ikey, keywordToken, leftIndex, result, a_w);
                }
            } else if (shouldDig && leftIndex == -1) {
                if (previousResult.count(keywordToken) == 0 || previousResult[keywordToken].count(index * 2 + 1) == 0 || previousResult[keywordToken][index * 2 + 1].acc != leftAcc) {
                    recursiveSearch(keywordToken, Ikey, Dkey, index * 2 + 1, leftAcc, result, coverSet, isLCover, a_w);
                } else {
                    findPreviousSearchResult(Ikey, keywordToken, index * 2 + 1, result, a_w);
                }
            }
            bool isRCover = false;
            checkIndex = rightIndex;
            if (rightIndex == -1) {
                checkIndex = 2 * index + 2;
            }
            shouldDig = subtreeContainCoverSet(checkIndex, coverSet, isRCover);
            if (shouldDig && rightIndex != -1) {
                if (previousResult.count(keywordToken) == 0 || previousResult[keywordToken].count(rightIndex) == 0 || previousResult[keywordToken][rightIndex].acc != rightAcc) {
                    recursiveSearch(keywordToken, Ikey, Dkey, rightIndex, rightAcc, result, coverSet, isRCover, a_w);
                } else {
                    findPreviousSearchResult(Ikey, keywordToken, rightIndex, result, a_w);
                }
            } else if (shouldDig && rightIndex == -1) {
                if (previousResult.count(keywordToken) == 0 || previousResult[keywordToken].count(index * 2 + 2) == 0 || previousResult[keywordToken][index * 2 + 2].acc != rightAcc) {
                    recursiveSearch(keywordToken, Ikey, Dkey, index * 2 + 2, rightAcc, result, coverSet, isRCover, a_w);
                } else {
                    findPreviousSearchResult(Ikey, keywordToken, index * 2 + 2, result, a_w);
                }
            }
        } else {
            bool IndexExist = previousResult[keywordToken].count(index);
            if (leftIndex != -1) {
                if (previousResult.count(keywordToken) == 0 || previousResult[keywordToken].count(leftIndex) == 0 || previousResult[keywordToken][leftIndex].acc != leftAcc) {
                    recursiveSearch(keywordToken, Ikey, Dkey, leftIndex, leftAcc, result, coverSet, true, a_w);
                } else {
                    findPreviousSearchResult(Ikey, keywordToken, leftIndex, result, a_w);
                }
            } else if (leftIndex == -1 && previousResult.count(keywordToken) != 0 && IndexExist) {
                findPreviousSearchResult(Ikey, keywordToken, index * 2 + 1, result, a_w);
            } else {
                addSubtreeLeaves(Ikey, keywordToken, result, index * 2 + 1);
            }
            if (rightIndex != -1) {
                if (previousResult.count(keywordToken) == 0 || previousResult[keywordToken].count(rightIndex) == 0 || previousResult[keywordToken][rightIndex].acc != rightAcc) {
                    recursiveSearch(keywordToken, Ikey, Dkey, rightIndex, rightAcc, result, coverSet, true, a_w);
                } else {
                    findPreviousSearchResult(Ikey, keywordToken, rightIndex, result, a_w);
                }
            } else if (rightIndex == -1 && previousResult.count(keywordToken) != 0 && IndexExist) {
                findPreviousSearchResult(Ikey, keywordToken, index * 2 + 2, result, a_w);
            } else {
                addSubtreeLeaves(Ikey, keywordToken, result, index * 2 + 2);
            }
        }
    }
}

void Server::addSubtreeLeaves(prf_type Ikey, prf_type keywordToken, Result* result, int root) {
    int leftLeaf, rightLeaf;
    int tmp = root;
    while (tmp < firstLeaf) {
        tmp = tmp * 2 + 1;
    }
    leftLeaf = tmp;
    tmp = root;
    while (tmp < firstLeaf) {
        tmp = tmp * 2 + 2;
    }
    rightLeaf = tmp;
    for (int i = leftLeaf; i <= rightLeaf; i++) {
        prf_type tmpKey;
        memset(tmpKey.data(), 0, AES_KEY_SIZE);
        *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 5])) = i;
        *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 9])) = 0;
        prf_type fkey = Utilities::generatePRF(tmpKey.data(), Ikey.data());
        if (previousResult.count(keywordToken) != 0 && previousResult[keywordToken].count(i) != 0) {
            result->insert(previousResult[keywordToken][i], keywordToken);
        } else {
            result->insert(i, true, remove(fkey, true), 0, 0, 0, 0, 0, Ikey, keywordToken);
        }
    }
}

void Server::addCacheSubtreeLeaves(prf_type Ikey, prf_type keywordToken, Result* result, int root, int a_w) {
    int leftLeaf, rightLeaf;
    int tmp = root;
    int lastExistedLeafInResult = previousResultAw[keywordToken] + firstLeaf;
    int lastRealLeafInResult = a_w + firstLeaf;
    while (tmp < firstLeaf) {
        tmp = tmp * 2 + 1;
    }
    leftLeaf = tmp;
    tmp = root;
    while (tmp < firstLeaf) {
        tmp = tmp * 2 + 2;
    }
    rightLeaf = tmp;
    int i = leftLeaf;
    for (; i <= rightLeaf && i <= lastExistedLeafInResult; i++) {
        result->insert(previousResult[keywordToken][i], keywordToken);
    }
    if (rightLeaf > lastExistedLeafInResult) {
        for (; i <= lastRealLeafInResult; i++) {
            prf_type tmpKey;
            memset(tmpKey.data(), 0, AES_KEY_SIZE);
            *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 5])) = i;
            *(int*) (&(tmpKey.data()[AES_KEY_SIZE - 9])) = 0;
            prf_type fkey = Utilities::generatePRF(tmpKey.data(), Ikey.data());
            result->insert(i, true, remove(fkey, true), 0, 0, 0, 0, 0, Ikey, keywordToken);
        }
    }
}

void Server::findPreviousSearchResult(prf_type Ikey, prf_type keywordToken, int index, Result* result, int a_w) {
    bool resultRowExist = false;
    ResultRow res;
    if (previousResult.count(keywordToken) != 0 && previousResult[keywordToken].count(index) != 0) {
        result->insert(previousResult[keywordToken][index], keywordToken);
        resultRowExist = true;
        res = previousResult[keywordToken][index];
    }
    if (index >= firstLeaf) {
        return;
    } else {
        if (resultRowExist) {
            if (res.leftChild != -1) {
                findPreviousSearchResult(Ikey, keywordToken, res.leftChild, result, a_w);
            } else {
                addCacheSubtreeLeaves(Ikey, keywordToken, result, index * 2 + 1, a_w);
            }
            if (res.rightChild != -1) {
                findPreviousSearchResult(Ikey, keywordToken, res.rightChild, result, a_w);
            } else {
                addCacheSubtreeLeaves(Ikey, keywordToken, result, index * 2 + 2, a_w);
            }
        } else {
            addCacheSubtreeLeaves(Ikey, keywordToken, result, index * 2 + 1, a_w);
            addCacheSubtreeLeaves(Ikey, keywordToken, result, index * 2 + 2, a_w);
        }
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
    previousResult.clear();
    previousResultAw.clear();
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

