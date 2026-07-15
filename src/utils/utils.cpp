#include "utils.h"
#include <vector>
#include <zlib.h>
#include <string>
#include <stdexcept>

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
