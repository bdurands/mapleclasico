#pragma once
#include "ztl/ztl.h"


class CInPacket {
protected:
    int m_bLoopback;
    int m_nState;
    ZArray<unsigned char> m_aRecvBuff;
    unsigned short m_uLength;
    unsigned short m_uRawSeq;
    unsigned short m_uDataLen;
    size_t m_uOffset;

public:
    size_t GetOffset() const { return m_uOffset; }
    void SetOffset(size_t off) { m_uOffset = off; }
    bool CanRead(size_t n) const { return m_uOffset + n <= m_uLength; }
    const unsigned char* CurrentPublic() const { return &m_aRecvBuff[m_uOffset]; }

    unsigned char Decode1() {
        if (!CanRead(1)) return 0;
        unsigned char v = m_aRecvBuff[m_uOffset];
        m_uOffset += 1;
        return v;
    }
    unsigned short Decode2() {
        if (!CanRead(2)) return 0;
        unsigned short v = *reinterpret_cast<unsigned short*>(&m_aRecvBuff[m_uOffset]);
        m_uOffset += 2;
        return v;
    }
    unsigned int Decode4() {
        if (!CanRead(4)) return 0;
        unsigned int v = *reinterpret_cast<unsigned int*>(&m_aRecvBuff[m_uOffset]);
        m_uOffset += 4;
        return v;
    }
    void DecodeBuffer(void* buf, size_t size) {
        if (!CanRead(size)) return;
        memcpy(buf, &m_aRecvBuff[m_uOffset], size);
        m_uOffset += size;
    }
    template<typename T>
    T Decode() {
        if constexpr (sizeof(T) == 1) {
            return static_cast<T>(Decode1());
        } else if constexpr (sizeof(T) == 2) {
            return static_cast<T>(Decode2());
        } else if constexpr (sizeof(T) == 4) {
            return static_cast<T>(Decode4());
        }
        return T();
    }
    std::string DecodeStr() {
        unsigned short len = Decode2();
        if (!CanRead(len)) return "";
        std::string s(reinterpret_cast<char*>(&m_aRecvBuff[m_uOffset]), len);
        m_uOffset += len;
        return s;
    }
};

static_assert(sizeof(CInPacket) == 0x18);


class COutPacket {
protected:
    int m_bLoopback;
    ZArray<unsigned char> m_aSendBuff;
    unsigned int m_uOffset;
    int m_bIsEncryptedByShanda;

public:
    explicit COutPacket(int nType) : m_aSendBuff(0x100) {
        Init(nType, 0, 0);
    }
    void Encode1(unsigned char n) {
        EncodeBuffer(&n, 1);
    }
    void Encode2(unsigned short n) {
        EncodeBuffer(&n, 2);
    }
    void Encode4(unsigned int n) {
        EncodeBuffer(&n, 4);
    }
    void EncodeStr(ZXString<char> s) {
        int n = s.GetLength();
        Encode2(n);
        EncodeBuffer(s, n);
    }
    void EncodeBuffer(const void* p, size_t uSize) {
        EnlargeBuffer(uSize);
        memcpy(&m_aSendBuff[m_uOffset], p, uSize);
        m_uOffset += uSize;
    }
    void Init(int nType, int bLoopback, int bTypeHeader1Byte) {
        m_bLoopback = bLoopback;
        m_uOffset = 0;
        if (nType != 0x7FFFFFFF) {
            if (bTypeHeader1Byte) {
                Encode1(nType);
            } else {
                Encode2(nType);
            }
        }
        m_bIsEncryptedByShanda = 0;
    }

protected:
    void EnlargeBuffer(size_t uSize) {
        size_t uCur = m_aSendBuff.GetCount();
        size_t uReq = m_uOffset + uSize;
        if (uCur < uReq) {
            do {
                uCur *= 2;
            } while (uCur < uReq);
            m_aSendBuff.Realloc(uCur, 0);
        }
    }
};

static_assert(sizeof(COutPacket) == 0x10);