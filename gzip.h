#pragma once
#include <zlib.h>
#include <string>

std::string gzip_compress(const std::string& data) {
    z_stream zs{};
    deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 
                  15 + 16, 8, Z_DEFAULT_STRATEGY);

    zs.next_in  = (Bytef*)data.data();
    zs.avail_in = data.size();

    std::string output;
    char buffer[4096];

    int ret;
    do {
        zs.next_out  = (Bytef*)buffer;
        zs.avail_out = sizeof(buffer);

        ret = deflate(&zs, Z_FINISH);

        output.append(buffer, sizeof(buffer) - zs.avail_out);
    } while (ret != Z_STREAM_END);

    deflateEnd(&zs);
    return output;
}
