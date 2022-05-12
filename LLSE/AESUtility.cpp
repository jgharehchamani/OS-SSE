#include "AESUtility.hpp"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/rand.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

void AESUtility::Setup() {
    // Initialise OpenSSL
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
    OPENSSL_config(NULL);
}

void AESUtility::Cleanup() {
    EVP_cleanup();
    ERR_free_strings();
}

static void error(const char *msg) {
    ERR_print_errors_fp(stderr);
}

int AESUtility::EncryptBytes(bytes<Key> key, bytes<IV> iv, byte_t *plaintext, int plen, byte_t *ciphertext) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if (!ctx) {
        error("Failed to create new cipher");
    }

    // Initialise the encryption operation
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key.data(), iv.data()) != 1) {
        error("Failed to initialise encryption");
    }

    // Encrypt
    int len;
    if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plen) != 1) {
        error("Failed to complete EncryptUpdate");
    }

    int clen = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1) {
        error("Failed to complete EncryptFinal");
    }
    clen += len;

    EVP_CIPHER_CTX_free(ctx);

    return clen;
}

int AESUtility::DecryptBytes(bytes<Key> key, bytes<IV> iv, byte_t *ciphertext, int clen, byte_t *plaintext) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if (!ctx) {
        error("Failed to create new cipher");
    }

    // Initialise the decryption operation
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key.data(), iv.data()) != 1) {
        error("Failed to initialise decryption");
    }

    // Dencrypt
    int len;
    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, clen) != 1) {
        error("Failed to complete DecryptUpdate");
    }

    int plen = len;

    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1) {
        error("Failed to complete DecryptFinal");
    }
    plen += len;

    EVP_CIPHER_CTX_free(ctx);

    return plen;
}

block AESUtility::EncryptBlock(bytes<Key> key, bytes<IV> iv, block plaintext, size_t clen_size, size_t plaintext_size) {
    block ciphertext(clen_size);
    EncryptBytes(key, iv, plaintext.data(), plaintext_size, ciphertext.data());
    return ciphertext;
}

block AESUtility::DecryptBlock(bytes<Key> key, bytes<IV> iv, block ciphertext, size_t clen_size) {
    block plaintext(clen_size);
    int plen = DecryptBytes(key, iv, ciphertext.data(), clen_size, plaintext.data());

    // Trim plaintext to actual size
    plaintext.resize(plen);

    return plaintext;
}

block AESUtility::Encrypt(bytes<Key> key, block plaintext, size_t clen_size, size_t plaintext_size) {
    block ciphertext;
    bytes<IV> iv = AESUtility::GenerateIV();

    ciphertext = EncryptBlock(key, iv, plaintext, clen_size, plaintext_size);

    // Put randomised IV at the front of the ciphertext
    ciphertext.insert(ciphertext.end(), iv.begin(), iv.end());
    return ciphertext;
}

block AESUtility::Decrypt(bytes<Key> key, block ciphertext, size_t clen_size) {
    // Extract the IV
    bytes<IV> iv;
    std::copy(ciphertext.end() - IV, ciphertext.end(), iv.begin());

    // Perform the decryption
    block plaintext = DecryptBlock(key, iv, ciphertext, clen_size);

    return plaintext;
}

int AESUtility::GetCiphertextLength(int plen) {
    // Round up to the next 16 bytes (due to padding)
    return (plen / 16 + 1) * 16;
}

bytes<IV> AESUtility::GenerateIV() {
    bytes<IV> iv;

    if (RAND_bytes(iv.data(), iv.size()) != 1) {
        // Bytes generated aren't cryptographically strong
        error("Needs more entropy");
    }

    return iv;
}

void AESUtility::derive(const uint8_t* data, const uint8_t* k, const size_t len, unsigned char* out) {
    const uint32_t offset = 0;
    if (k == NULL) {
        throw std::invalid_argument("PRG input key is NULL");
    }
    if (len == 0) {
        throw std::invalid_argument("The minimum number of bytes to encrypt is 1.");
    }

    uint32_t extra_len = (offset % AES_BLOCK_SIZE);
    uint32_t block_offset = offset / AES_BLOCK_SIZE;
    size_t max_block_index = (len + offset) / AES_BLOCK_SIZE;

    if ((len + offset) % AES_BLOCK_SIZE != 0) {
        max_block_index++;
    }

    size_t block_len = max_block_index - block_offset;

    AES_KEY aes_enc_key;

    if (AES_set_encrypt_key(k, 128, &aes_enc_key) != 0) {
        // throw an exception
        throw std::runtime_error("Unable to init AESUtility subkeys");
    }

    unsigned char *in = new unsigned char[block_len * AES_BLOCK_SIZE];
    memset(in, 0x00, block_len * AES_BLOCK_SIZE);

    unsigned char *tmp = new unsigned char[block_len * AES_BLOCK_SIZE];

    for (size_t i = block_offset; i < max_block_index; i++) {
        ((size_t*) in)[2 * (i - block_offset)] = i;
    }
    memcpy(in, data, len);

    memset(out, 0, len);

    for (size_t i = 0; i < block_len; i++) {
        AES_encrypt(in + i*AES_BLOCK_SIZE, tmp + i*AES_BLOCK_SIZE, &aes_enc_key);
    }

    memcpy(out, tmp + extra_len, len);
    memset(tmp, 0x00, block_len * AES_BLOCK_SIZE);
    memset(&aes_enc_key, 0x00, sizeof (AES_KEY));


    delete [] tmp;
    delete [] in;
}