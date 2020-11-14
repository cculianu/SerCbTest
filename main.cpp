#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <arpa/inet.h>


template <typename T>
static T SwapBytes(T val)
{
    constexpr size_t n = sizeof(T);
    T ret{val};
    if (n) {
        std::byte *p = reinterpret_cast<std::byte *>(&ret);
        for (size_t lo = 0, hi = n-1; hi > lo; lo++, hi--)
        {
            const std::byte tmp = p[lo];
            p[lo] = p[hi];
            p[hi] = tmp;
        }
    }
    return ret;
}

static uint16_t htole16(uint16_t val)
{
    static constexpr int ival = 15;
    if (*(char *)&ival == 15) // little endian
        return val;
    return SwapBytes(val);
}

static uint32_t htole32(uint32_t val)
{
    static constexpr int ival = 15;
    if (*(char *)&ival == 15) // little endian
        return val;
    return SwapBytes(val);
}

static inline void WriteLE16(uint8_t *ptr, uint16_t x) {
    uint16_t v = htole16(x);
    std::memcpy(ptr, (char *)&v, 2);
}

static inline void WriteLE32(uint8_t *ptr, uint32_t x) {
    uint32_t v = htole32(x);
    std::memcpy(ptr, (char *)&v, 4);
}

using uchar = uint8_t;

/* For encoding nHeight into coinbase, return how many bytes were used */
std::vector<uchar> asicseer_ser_cbheight(int32_t val)
{
    static const auto ser = [](uchar *outp, int32_t val) -> unsigned {
        uchar *s = (uchar *)outp;
        int32_t *i32 = (int32_t *)&s[1];
        int len;

        if (val < 128)
            len = 1;
        else if (val < 16512)
            len = 2;
        else if (val < 2113664)
            len = 3;
        else
            len = 4;
        const int32_t tmp = htole32(val);
        memcpy(i32, &tmp, len);
        s[0] = len++;
        return len;
    };
    std::vector<uchar> ret(16);
    ret.resize( ser(&ret[0], val) );
    return ret;
}

std::vector<uchar> BCHN_ser_cbheight(int64_t n) {
    std::vector<uchar> v;
    enum opcodetype {
        // push value
        OP_0 = 0x00,
        OP_1 = 0x51,
        OP_PUSHDATA1 = 0x4c,
        OP_PUSHDATA2 = 0x4d,
        OP_PUSHDATA4 = 0x4e,
    };
    if (n == -1 || (n >= 1 && n <= 16)) {
        v.push_back(n + (OP_1 - 1));
    } else if (n == 0) {
        v.push_back(OP_0);
    } else {
        // standard way to push a vector in bitcoin
        static const auto pushVec = [] (std::vector<uchar> &v, const std::vector<uchar> &b) {
             if (b.size() < OP_PUSHDATA1) {
                 v.insert(v.end(), uint8_t(b.size()));
             } else if (b.size() <= 0xff) {
                 v.insert(v.end(), OP_PUSHDATA1);
                 v.insert(v.end(), uint8_t(b.size()));
             } else if (b.size() <= 0xffff) {
                 v.insert(v.end(), OP_PUSHDATA2);
                 uint8_t _data[2];
                 WriteLE16(_data, b.size());
                 v.insert(v.end(), _data, _data + sizeof(_data));
             } else {
                 v.insert(v.end(), OP_PUSHDATA4);
                 uint8_t _data[4];
                 WriteLE32(_data, b.size());
                 v.insert(v.end(), _data, _data + sizeof(_data));
             }
             v.insert(v.end(), b.begin(), b.end());
        };
        static const auto serialize = [](const int64_t &value) -> std::vector<uint8_t> {
            if (value == 0) {
                return {};
            }

            std::vector<uint8_t> result;
            const bool neg = value < 0;
            uint64_t absvalue = neg ? -value : value;

            while (absvalue) {
                result.push_back(absvalue & 0xff);
                absvalue >>= 8;
            }

            // - If the most significant byte is >= 0x80 and the value is positive,
            // push a new zero-byte to make the significant byte < 0x80 again.
            // - If the most significant byte is >= 0x80 and the value is negative,
            // push a new 0x80 byte that will be popped off when converting to an
            // integral.
            // - If the most significant byte is < 0x80 and the value is negative,
            // add 0x80 to it, since it will be subtracted and interpreted as a
            // negative when converting to an integral.
            if (result.back() & 0x80) {
                result.push_back(neg ? 0x80 : 0);
            } else if (neg) {
                result.back() |= 0x80;
            }

            return result;
        };

        const auto bytes = serialize(n); // serialize compact
        pushVec(v, bytes);
    }
    return v;
}


std::string ToHex(const void *mem, unsigned len)
{
    std::string ret;
    ret.reserve(len * 2);
    const uchar *ptr = static_cast<const uchar *>(mem);
    while (len--) {
        const uchar c = *ptr++;
        const uchar nibLo = c & 0x0f, nibHi = (c >> 4u) & 0x0f;
        for (auto nib : {nibHi, nibLo}) {
            if (nib < 10) ret +=  char('0' + nib);
            else if (nib < 16) ret += char('a' + (nib-10));
            else throw std::runtime_error("Bug!");
        }
    }
    return ret;
}

template <typename T>
std::string ToHex(const std::vector<T> &v) { return ToHex(&v[0], v.size()); }

int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cerr << "Please pass a height to serialize as hex\n";
        return EXIT_FAILURE;
    }
    const int32_t val = std::stoi(argv[1]);
    std::vector<uchar> encoded;

    encoded = asicseer_ser_cbheight(val);
    std::cout << "AsicSeer - Value: " << val << " Hex: " << ToHex(encoded) << "\n";

    encoded = BCHN_ser_cbheight(val);
    std::cout << "BCHN - Value: " << val << " Hex: " << ToHex(encoded) << "\n";

    return EXIT_SUCCESS;
}
