#include "utils.h"
#include <vector>
#include <zlib.h>
#include <string>
#include <stdexcept>
#include <openssl/sha.h>
#include <sstream>
#include <filesystem>

using namespace std;

string decompress(const string &compressed) {
    uLongf decompressedSize = compressed.size()*4 + 64;
    vector <Bytef> buffer;
    int result;
    do{
        buffer.resize(decompressedSize);
        result = uncompress(
            buffer.data(),
            &decompressedSize,
            reinterpret_cast<const Bytef *>(compressed.data()),
            compressed.size()
        );
        if(result == Z_BUF_ERROR){
            decompressedSize = decompressedSize * 2;
        }
    }while(result == Z_BUF_ERROR);
    if(result != Z_OK){
        throw runtime_error("zlib decompression error: " + to_string(result));
    }

    return string(reinterpret_cast<char *>(buffer.data()) , decompressedSize);
}


string read_file(const string &file_path) {
    ifstream file(file_path, ios::binary);
    if(!file.is_open()){
        throw runtime_error("failed to open file: " + file_path);
    }
    return string(istreambuf_iterator<char>(file), istreambuf_iterator<char>());
}

string sha1_hex(const string& data) {
    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest);

    ostringstream oss;
    for (unsigned char byte : digest) {
        oss << hex << setw(2) << setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}

string compress(const string& data) {
    uLongf bound = compressBound(data.size());
    vector<unsigned char> buffer(bound);

    int result = compress(buffer.data(), &bound,
                           reinterpret_cast<const unsigned char*>(data.data()),
                           data.size());
    if (result != Z_OK) {
        throw runtime_error("zlib compression failed: " + to_string(result));
    }
    return string(reinterpret_cast<char*>(buffer.data()), bound);
}

void write_object_file(const string& hash, const string& compressed) {
    string dir  = ".git/objects/" + hash.substr(0, 2);
    string path = dir + "/" + hash.substr(2);

    filesystem::create_directories(dir);

    ofstream out(path, ios::binary);
    if (!out) {
        throw runtime_error("Failed to write object file: " + path);
    }
    out.write(compressed.data(), static_cast<streamsize>(compressed.size()));
}
