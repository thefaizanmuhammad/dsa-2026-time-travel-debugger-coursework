#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>


// Helper.h should contain the structs, functions, constants, etc. that are used more than one time in the project. For example, the structs that are used in both server and client, the constants that are used in all three files. 

struct FuncEntry
{
    std::string funcName;
    int64_t byteOffsetInResolveBin; // where this function's FUNC header record sits
};
struct PendingPatch
{
    int64_t byteOffsetOfOffsetField; // where in resolve.bin to seek back and overwrite
    std::string targetFuncName;
};
inline uint32_t updateChecksum(uint32_t sum, const unsigned char* data, size_t count)
{
    for (size_t i = 0; i < count; i++)
        sum += data[i];

    return sum;
}
inline void writeChecksum(uint32_t sum, unsigned char trailer[4])
{
    for (int i = 3; i >= 0; i--)
    {
        trailer[i] = static_cast<unsigned char>(sum & 255);
        sum >>= 8;
    }
}

class TrailerChecker
{
    unsigned char trailer[4] = {};
    int count = 0;
    uint32_t sum = 0;
    bool receivedStatus = false;

public:
    bool feed(unsigned char byte, unsigned char& released)
    {
        if (count < 4)
        {
            trailer[count++] = byte;
            return false;
        }

        released = trailer[0];
        for (int i = 1; i < 4; i++)
            trailer[i - 1] = trailer[i];

        trailer[3] = byte;
        sum = updateChecksum(sum, &released, 1);
        receivedStatus = true;
        return true;
    }

    bool valid() const
    {
        if (!receivedStatus)
            return false;

        unsigned char expected[4];
        writeChecksum(sum, expected);

        for (int i = 0; i < 4; i++)
        {
            if (trailer[i] != expected[i])
                return false;
        }
        return true;
    }
};
