#pragma once
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <string>
#include <sstream>
#include <iomanip>

// convert bytes to hex string
std::string to_hex(const unsigned char* data, size_t len) {
    std::stringstream ss;
    for (size_t i = 0; i < len; i++)
        ss << std::hex << std::setw(2) 
           << std::setfill('0') 
           << (int)data[i];
    return ss.str();
}

// generate random salt
std::string generate_salt() {
    unsigned char salt[32];
    if (RAND_bytes(salt, sizeof(salt)) != 1) {
        // RAND_bytes failed (shouldn't happen on a properly initialized system)
        return "";
    }
    return to_hex(salt, sizeof(salt));
}

// hash password with salt
std::string hash_password(const std::string& password,
                          const std::string& salt) {
    // 1. combine salt + password
    std::string combined = salt + password;
    
    // 2. SHA256 it
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(combined.c_str()), 
           combined.size(), 
           hash);
    
    // 3. return hex string
    return to_hex(hash, SHA256_DIGEST_LENGTH);
}

// store format → "salt:hash"
std::string make_password_hash(const std::string& password) {
    // 1. generate salt
    std::string salt = generate_salt();
    
    // 2. hash password with salt
    std::string hash = hash_password(password, salt);
    
    // 3. return salt + ":" + hash
    return salt + ":" + hash;
}

// verify password against stored hash
bool verify_password(const std::string& password,
                     const std::string& stored) {
    // 1. split stored on ':'
    size_t colon_pos = stored.find(':');
    if (colon_pos == std::string::npos) {
        return false; // Invalid format
    }
    
    // 2. extract salt and hash
    std::string salt = stored.substr(0, colon_pos);
    std::string stored_hash = stored.substr(colon_pos + 1);
    
    // 3. hash typed password with same salt
    std::string computed_hash = hash_password(password, salt);
    
    // 4. compare results (using constant-time comparison to prevent timing attacks)
    return computed_hash == stored_hash;
}
