#include "blob.h"

Blob Blob::parse(const std::string& storeBytes) {
    return Blob(stripHeader(storeBytes, "blob"));
}
