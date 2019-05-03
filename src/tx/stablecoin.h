// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2019- The WaykiChain Core Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php

#ifndef STABLE_COIN_H
#define STABLE_COIN_H

#include "tx/tx.h"

class CCdpOpenTx : public CBaseTx {
public:
    mutable CUserID userId;     // CDP owner's regid or pubkey
    uint64_t bcoins;            // CDP collateral base coins amount
    uint64_t scoins;            // minted stable coins
    vector_unsigned_char signature;

public:
    CCdpOpenTx(): CBaseTx(CDP_OPEN_TX) {}

    CCdpOpenTx(const CBaseTx *pBaseTx): CBaseTx(CDP_OPEN_TX) {
        assert(CDP_OPEN_TX == pBaseTx->nTxType);
        *this = *(CCdpOpenTx *) pBaseTx;
    }

    CCdpOpenTx(const CUserID &userIdIn, int validHeightIn, uint64_t feeIn, uint64_t bcoinsIn):
        CBaseTx(CDP_OPEN_TX, validHeightIn, feeIn) {
        userId = userIdIn;
        bcoins = bcoinsIn;
    }

    ~CCdpOpenTx() {}

    IMPLEMENT_SERIALIZE(
        READWRITE(VARINT(this->nVersion));
        READWRITE(VARINT(nValidHeight));
        READWRITE(userId);
        READWRITE(VARINT(llFees));
        READWRITE(VARINT(bcoins));
        READWRITE(VARINT(scoins));
        READWRITE(signature);
    )

    uint256 SignatureHash(bool recalculate = false) const {
        if (recalculate || sigHash.IsNull()) {
            CHashWriter ss(SER_GETHASH, 0);
            ss << VARINT(nVersion) << nTxType << VARINT(nValidHeight) << userId
               << VARINT(llFees) << VARINT(bcoins) << VARINT(scoins);

            uint256 *hash = const_cast<uint256 *>(&sigHash);
            *hash         = ss.GetHash();
        }

        return sigHash;
    }

    virtual uint256 GetHash() const { return SignatureHash(); }
    virtual uint64_t GetFee() const { return llFees; }
    uint64_t GetValue() const { return 0; }
    double GetPriority() const { return llFees / GetSerializeSize(SER_NETWORK, PROTOCOL_VERSION); }
    std::shared_ptr<CBaseTx> GetNewInstance() { return std::make_shared<CCdpOpenTx>(this); }
    string ToString(CAccountViewCache &view) const;
    Object ToJson(const CAccountViewCache &AccountView) const;
    bool GetAddress(set<CKeyID> &vAddr, CAccountViewCache &view, CScriptDBViewCache &scriptDB);
    bool ExecuteTx(int nIndex, CAccountViewCache &view, CValidationState &state, CTxUndo &txundo,
                   int nHeight, CTransactionDBCache &txCache, CScriptDBViewCache &scriptDB);
    bool UndoExecuteTx(int nIndex, CAccountViewCache &view, CValidationState &state,
                       CTxUndo &txundo, int nHeight, CTransactionDBCache &txCache,
                       CScriptDBViewCache &scriptDB);
    bool CheckTx(CValidationState &state, CAccountViewCache &view,
                          CScriptDBViewCache &scriptDB);
};

enum CoinType: uint8_t {
    WICC = 1,
    MICC = 2,
    WUSD = 3,
};

enum PriceType: uint8_t {
    USD     = 1,
    CNY     = 2,
    EUR     = 3,
    BTC     = 10,
    USDT    = 11,
    GOLD    = 20,
    KWH     = 100, // killowatt hour
};
// ######################################################################
class CPriceFeedTx : public CBaseTx {
public:
    mutable CUserID userId;     // price feed owner's regid or pubkey
    CoinType coinType;          // the coin of which price is fed
    PriceType priceType;        // price type of the coin, E.g. USD/CNY

    uint64_t price;             // price of the coin by the base currency

public:
    CPriceFeedTx(): CBaseTx(PRICE_FEED_TX) {}

    CPriceFeedTx(const CBaseTx *pBaseTx): CBaseTx(PRICE_FEED_TX) {
        assert(PRICE_FEED_TX == pBaseTx->nTxType);
        *this = *(CPriceFeedTx *) pBaseTx;
    }

    CPriceFeedTx(const CUserID &userIdIn, int validHeightIn, uint64_t feeIn, uint64_t bcoinsIn):
        CBaseTx(PRICE_FEED_TX, validHeightIn, feeIn) {
        userId = userIdIn;
    }

    ~CPriceFeedTx() {}

    IMPLEMENT_SERIALIZE(
        READWRITE(VARINT(this->nVersion));
        READWRITE(VARINT(nValidHeight));
        READWRITE(userId);
        READWRITE(VARINT(llFees));
        READWRITE(VARINT(bcoins));
        READWRITE(VARINT(scoins));
        READWRITE(signature);
    )

    uint256 SignatureHash(bool recalculate = false) const {
        if (recalculate || sigHash.IsNull()) {
            CHashWriter ss(SER_GETHASH, 0);
            ss << VARINT(nVersion) << nTxType << VARINT(nValidHeight) << userId
               << VARINT(llFees) << VARINT(bcoins) << VARINT(scoins);

            uint256 *hash = const_cast<uint256 *>(&sigHash);
            *hash         = ss.GetHash();
        }

        return sigHash;
    }

    virtual uint256 GetHash() const { return SignatureHash(); }
    virtual uint64_t GetFee() const { return llFees; }
    uint64_t GetValue() const { return 0; }
    double GetPriority() const { return llFees / GetSerializeSize(SER_NETWORK, PROTOCOL_VERSION); }
    std::shared_ptr<CBaseTx> GetNewInstance() { return std::make_shared<CCdpOpenTx>(this); }
    string ToString(CAccountViewCache &view) const;
    Object ToJson(const CAccountViewCache &AccountView) const;
    bool GetAddress(set<CKeyID> &vAddr, CAccountViewCache &view, CScriptDBViewCache &scriptDB);
    bool ExecuteTx(int nIndex, CAccountViewCache &view, CValidationState &state, CTxUndo &txundo,
                   int nHeight, CTransactionDBCache &txCache, CScriptDBViewCache &scriptDB);
    bool UndoExecuteTx(int nIndex, CAccountViewCache &view, CValidationState &state,
                       CTxUndo &txundo, int nHeight, CTransactionDBCache &txCache,
                       CScriptDBViewCache &scriptDB);
    bool CheckTx(CValidationState &state, CAccountViewCache &view,
                          CScriptDBViewCache &scriptDB);
};

#endif