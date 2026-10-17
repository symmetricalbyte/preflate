#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <string.h>
#include <vector>

#include "preflate_decoder.h"
#include "preflate_reencoder.h"
#include "support/stream.h"
#include "support/task_pool.h"

class SpanInputStream : public InputStream {
public:
    SpanInputStream(const unsigned char* data, size_t size) : _data(data), _size(size), _pos(0) {}

    bool eof() const override {
        return _pos >= _size;
    }

    size_t read(unsigned char* buffer, const size_t size) override {
        size_t toCopy = std::min(size, _size - _pos);
        if (toCopy > 0) {
            memcpy(buffer, _data + _pos, toCopy);
            _pos += toCopy;
        }
        return toCopy;
    }

    size_t pos() const {
        return _pos;
    }
    size_t remaining() const {
        return _size - _pos;
    }

private:
    const unsigned char* _data;
    const size_t _size;
    size_t _pos;
};

class SpanOutputStream : public OutputStream {
public:
    SpanOutputStream(unsigned char* buffer, size_t capacity) : _buffer(buffer), _capacity(capacity), _size(0), _overflow(false) {}

    size_t write(const unsigned char* buffer, const size_t size) override {
        if (_size + size > _capacity) {
            _overflow = true;
            return 0;
        }
        memcpy(_buffer + _size, buffer, size);
        _size += size;
        return size;
    }

    size_t size() const { return _size; }
    bool overflow() const { return _overflow; }

private:
    unsigned char* _buffer;
    const size_t _capacity;
    size_t _size;
    bool _overflow;
};

struct ThreadBufferPool {
    std::vector<unsigned char> diff_buffer;
    std::vector<unsigned char> reencode_diff_buffer;

    ThreadBufferPool() {
        diff_buffer.reserve(65536);
        reencode_diff_buffer.reserve(65536);
    }
};

static thread_local ThreadBufferPool tls_buffers;

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        globalTaskPool.setThreadLimit(0);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) bool decode(
    const unsigned char* src,
    size_t srcSize,
    unsigned char* dst1,
    size_t* dst1Capacity,
    unsigned char* dst2,
    size_t* dst2Capacity) {

    if (!src || srcSize == 0 || !dst1 || !dst1Capacity || *dst1Capacity == 0 || !dst2 || !dst2Capacity || *dst2Capacity == 0) { return false; }

    SpanInputStream inStream(src, srcSize);
    SpanOutputStream outStream(dst1, *dst1Capacity);

    auto& diff = tls_buffers.diff_buffer;
    diff.clear();

    uint64_t deflateSize = 0;
    bool ok = preflate_decode(outStream, diff, deflateSize, inStream, [] {}, 0, INT32_MAX);

    if (!ok || outStream.overflow() || deflateSize != srcSize) { return false; }

    if (diff.size() > *dst2Capacity) { return false; }

    *dst1Capacity = outStream.size();
    *dst2Capacity = diff.size();
    memcpy(dst2, diff.data(), diff.size());
    return true;
}

extern "C" __declspec(dllexport) bool reencode(
    const unsigned char* src1,
    size_t src1Size,
    const unsigned char* src2,
    size_t src2Size,
    unsigned char* dst,
    size_t* dstCapacity) {

    if (!src1 || !src2 || src2Size == 0 || !dst || !dstCapacity || *dstCapacity == 0) { return false; }

    SpanInputStream uncompressedIn(src1, src1Size);
    SpanOutputStream deflateOut(dst, *dstCapacity);

    auto& diff = tls_buffers.reencode_diff_buffer;
    diff.resize(src2Size);
    memcpy(diff.data(), src2, src2Size);

    bool ok = preflate_reencode(deflateOut, diff, uncompressedIn, src1Size, [] {});

    if (!ok || deflateOut.overflow()) { return false; }

    *dstCapacity = deflateOut.size();
    return true;
}
