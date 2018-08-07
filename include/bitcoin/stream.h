#ifndef __BITCOIN_STREAM_H__
#define __BITCOIN_STREAM_H__

#include <stdint.h>
#include <string.h>
#include <vector>
#include <bitcoin/serialize.h>
#include <iostream>

/* Minimal stream for overwriting and/or appending to an existing byte vector
 *
 * The referenced vector will grow as necessary
 */
class CVectorWriter
{
 public:

/*
 * @param[in]  nTypeIn Serialization Type
 * @param[in]  nVersionIn Serialization Version (including any flags)
 * @param[in]  vchDataIn  Referenced byte vector to overwrite/append
 * @param[in]  nPosIn Starting position. Vector index where writes should start. The vector will initially
 *                    grow as necessary to max(nPosIn, vec.size()). So to append, use vec.size().
*/
    CVectorWriter(bool bHaveTimeIn, int nTypeIn, uint32_t nVersionIn, std::vector<unsigned char>& vchDataIn, size_t nPosIn) : 
        bHaveTime(bHaveTimeIn), nType(nTypeIn),  nVersion(nVersionIn), vchData(vchDataIn), nPos(nPosIn)
    {
        if(nPos > vchData.size())
            vchData.resize(nPos);
    }
/*
 * (other params same as above)
 * @param[in]  args  A list of items to serialize starting at nPosIn.
*/
    template <typename... Args>
    CVectorWriter(bool bHaveTimeIn, int nTypeIn, uint32_t nVersionIn, std::vector<unsigned char>& vchDataIn, size_t nPosIn, Args&&... args) : 
        CVectorWriter(bHaveTimeIn, nTypeIn, nVersionIn, vchDataIn, nPosIn)
    {
        ::SerializeMany(*this, std::forward<Args>(args)...);
    }
    void write(const char* pch, size_t nSize)
    {
        assert(nPos <= vchData.size());
        size_t nOverwrite = std::min(nSize, vchData.size() - nPos);
        if (nOverwrite) {
            memcpy(vchData.data() + nPos, reinterpret_cast<const unsigned char*>(pch), nOverwrite);
        }
        if (nOverwrite < nSize) {
            vchData.insert(vchData.end(), reinterpret_cast<const unsigned char*>(pch) + nOverwrite, reinterpret_cast<const unsigned char*>(pch) + nSize);
        }
        nPos += nSize;
    }
    template<typename T>
    CVectorWriter& operator<<(const T& obj)
    {
        // Serialize to this stream
        ::Serialize(*this, obj);
        return (*this);
    }
    int GetVersion() const
    {
        return nVersion;
    }
    int GetType() const
    {
        return nType;
    }
    int HaveTime() const
    {
        return bHaveTime;
    }
    void seek(size_t nSize)
    {
        nPos += nSize;
        if(nPos > vchData.size())
            vchData.resize(nPos);
    }
    void reset(int nTypeIn, int nVersionIn) {
        nType = nTypeIn;
        nVersion = nVersionIn;
        nPos = 0;
        vchData.clear();
    }
private:
    int nType;
    uint32_t nVersion;
    std::vector<unsigned char>& vchData;
    size_t nPos;
    bool bHaveTime;
};


class CVectorReader
{
public:
    CVectorReader(bool bHaveTimeIn, int nTypeIn, int nVersionIn, const std::vector<unsigned char>& vchDataIn, size_t pos) : 
        bHaveTime(bHaveTimeIn), nType(nTypeIn), nVersion(nVersionIn), vchData(vchDataIn), readPos(pos) {}

    void read(const char* pch, size_t nSize)
    {
        // printf("read %zu bytes, readPos=%zu, bufferSize=%zu\n", nSize, readPos, vchData.size());
        // for (auto i = 0; i < nSize; i++) {
        //     printf("%02x ", vchData[readPos+i]);
        // }
        // printf("\n");
        assert(readPos <= vchData.size());
        if (nSize == 0) {
            return;
        }
        if (readPos + nSize > vchData.size()) {
            memset((void *)pch, 0, nSize);
            readPos = vchData.size();
        } else {
            memcpy((void *)pch, &vchData[readPos], nSize);
            readPos += nSize;
        }
    }

    template<typename T>
    CVectorReader& operator>>(T& obj)
    {
        // Serialize to this stream
        ::Unserialize(*this, obj);
        return (*this);
    }
    void skip(size_t span) {
        readPos += span;
        assert(readPos <= vchData.size());
    }
    int GetVersion() const
    {
        return nVersion;
    }
    int GetType() const
    {
        return nType;
    }
    bool HaveTime() const
    {
        return bHaveTime;
    }
    size_t GetReadPos() const
    {
        return readPos;
    }
private:
    int nType;
    int nVersion;
    const std::vector<unsigned char>& vchData;
    size_t readPos;
    bool bHaveTime;
};



#endif
