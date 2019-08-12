// Copyright (c) 2017-2019 The WaykiChain Core Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php

#include "rpccommons.h"

#include "entities/key.h"
#include "main.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "init.h"
#include "rpcserver.h"

/*
std::string split implementation by using delimeter as a character.
*/
std::vector<std::string> split(std::string strToSplit, char delimeter)
{
    std::stringstream ss(strToSplit);
    std::string item;
	std::vector<std::string> splittedStrings;
    while (std::getline(ss, item, delimeter))
	{
		splittedStrings.push_back(item);
    }
	return splittedStrings;
}

/*
std::string split implementation by using delimeter as an another string
*/
std::vector<std::string> split(std::string stringToBeSplitted, std::string delimeter)
{
	std::vector<std::string> splittedString;
	size_t startIndex = 0;
	size_t  endIndex = 0;
	while( (endIndex = stringToBeSplitted.find(delimeter, startIndex)) < stringToBeSplitted.size() ) {
		std::string val = stringToBeSplitted.substr(startIndex, endIndex - startIndex);
		splittedString.push_back(val);
		startIndex = endIndex + delimeter.size();
	}

	if(startIndex < stringToBeSplitted.size()) {
		std::string val = stringToBeSplitted.substr(startIndex);
		splittedString.push_back(val);
	}

	return splittedString;
}

// [N|R|A]:address
// NickID (default) | RegID | Address
bool ParseRpcInputAccountId(const string &comboAccountIdStr, tuple<AccountIDType, string> &comboAccountId) {
    vector<string> comboAccountIdArr = split(comboAccountIdStr, ':');
    switch (comboAccountIdArr.size()) {
        case 0: {
            get<0>(comboAccountId) = AccountIDType::NICK_ID;
            get<1>(comboAccountId) = comboAccountIdArr[0];
            break;
        }
        case 1: {
            if (comboAccountIdArr[0].size() > 1)
                return false;

            char tag = toupper(comboAccountIdArr[0][0]);
            if (tag == 'N') {
                get<0>(comboAccountId) = AccountIDType::NICK_ID;

            } else if (tag == 'R') {
                get<0>(comboAccountId) = AccountIDType::REG_ID;

            } else if (tag == 'A') {
                get<0>(comboAccountId) = AccountIDType::ADDRESS;

            } else
                return false;

            get<1>(comboAccountId) = comboAccountIdArr[1];

            break;
        }
        default: break;
    }

    return true;
}

// [symbol]:amount:[unit]
// [WICC(default)|WUSD|WGRT|...]:amount:[sawi(default)]
bool ParseRpcInputMoney(const string &comboMoneyStr, ComboMoney &comboMoney, const TokenSymbol defaultSymbol) {
	vector<string> comboMoneyArr = split(comboMoneyStr, ':');

    switch (comboMoneyArr.size()) {
        case 1: {
            if (!is_number(comboMoneyArr[0]))
                return false;

            int64_t iValue = std::atoll(comboMoneyArr[0].c_str());
            if (iValue < 0)
                return false;

            comboMoney.symbol = defaultSymbol;
            comboMoney.amount = (uint64_t) iValue;
            comboMoney.unit   = COIN_UNIT::SAWI;
            break;
        }
        case 2: {
            if (is_number(comboMoneyArr[0])) {
                int64_t iValue = std::atoll(comboMoneyArr[0].c_str());
                if (iValue < 0)
                    return false;

                if (!CoinUnitTypeTable.count(comboMoneyArr[1]))
                    return false;

                comboMoney.symbol = defaultSymbol;
                comboMoney.amount = (uint64_t) iValue;
                comboMoney.unit   = comboMoneyArr[1];

            } else if (is_number(comboMoneyArr[1])) {
                if (comboMoneyArr[0].size() > MAX_TOKEN_SYMBOL_LEN) // check symbol len
                    return false;

                int64_t iValue = std::atoll(comboMoneyArr[1].c_str());
                if (iValue < 0)
                    return false;

                string strSymbol = comboMoneyArr[0];
                std::for_each(strSymbol.begin(), strSymbol.end(), [](char & c){
                    c = ::toupper(c);
                });

                comboMoney.symbol = strSymbol;
                comboMoney.amount = (uint64_t) iValue;
                comboMoney.unit   = COIN_UNIT::SAWI;

            } else {
                return false;
            }

            break;
        }
        case 3: {
            if (comboMoneyArr[0].size() > MAX_TOKEN_SYMBOL_LEN) // check symbol len
                return false;

            if (!is_number(comboMoneyArr[1]))
                return false;

            int64_t iValue = std::atoll(comboMoneyArr[1].c_str());
            if (iValue < 0)
                return false;

            if (!CoinUnitTypeTable.count(comboMoneyArr[2]))
                return false;

            string strSymbol = comboMoneyArr[0];
            std::for_each(strSymbol.begin(), strSymbol.end(), [](char & c){
                c = ::toupper(c);
            });

            comboMoney.symbol = strSymbol;
            comboMoney.amount = (uint64_t) iValue;
            comboMoney.unit   = comboMoneyArr[2];
            break;
        }
        default:
            return false;
    }

    return true;
}

Object SubmitTx(const CUserID &userId, CBaseTx &tx) {

    CAccount account;
    if (pCdMan->pAccountCache->GetAccount(userId, account) && account.HaveOwnerPubKey()) {
        uint64_t balance = account.GetToken(SYMB::WICC).free_amount;
        if (balance < tx.llFees) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Account balance is insufficient");
        }
    } else {
        throw JSONRPCError(RPC_WALLET_ERROR, "Account is unregistered");
    }

    CKeyID keyId;
    if (!pCdMan->pAccountCache->GetKeyId(userId, keyId)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Failed to acquire key id");
    }

    CRegID regId;
    pCdMan->pAccountCache->GetRegId(userId, regId);
    tx.txUid = regId;

    assert(pWalletMain != nullptr);
    {
        EnsureWalletIsUnlocked();
        if (!pWalletMain->HaveKey(keyId)) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Sender address not found in wallet");
        }
        if (!pWalletMain->Sign(keyId, tx.ComputeSignatureHash(), tx.signature)) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Sign failed");
        }

        std::tuple<bool, string> ret = pWalletMain->CommitTx((CBaseTx*)&tx);
        if (!std::get<0>(ret)) {
            throw JSONRPCError(RPC_WALLET_ERROR, std::get<1>(ret));
        }

        Object obj;
        obj.push_back(Pair("txid", std::get<1>(ret)));
        return obj;
    }
}

string RegIDToAddress(CUserID &userId) {
    CKeyID keyId;
    if (pCdMan->pAccountCache->GetKeyId(userId, keyId))
        return keyId.ToAddress();

    return "cannot get address from given RegId";
}

bool GetKeyId(const string &addr, CKeyID &keyId) {
    if (!CRegID::GetKeyId(addr, keyId)) {
        keyId = CKeyID(addr);
        if (keyId.IsEmpty())
            return false;
    }

    return true;
}

Object GetTxDetailJSON(const uint256& txid) {
    Object obj;
    std::shared_ptr<CBaseTx> pBaseTx;
    {
        LOCK(cs_main);
        CBlock genesisblock;
        CBlockIndex* pGenesisBlockIndex = mapBlockIndex[SysCfg().GetGenesisBlockHash()];
        ReadBlockFromDisk(pGenesisBlockIndex, genesisblock);
        assert(genesisblock.GetMerkleRootHash() == genesisblock.BuildMerkleTree());
        for (unsigned int i = 0; i < genesisblock.vptx.size(); ++i) {
            if (txid == genesisblock.GetTxid(i)) {
                obj = genesisblock.vptx[i]->ToJson(*pCdMan->pAccountCache);
                obj.push_back(Pair("block_hash", SysCfg().GetGenesisBlockHash().GetHex()));
                obj.push_back(Pair("confirmed_height", (int) 0));
                obj.push_back(Pair("confirmed_time", (int) genesisblock.GetTime()));
                CDataStream ds(SER_DISK, CLIENT_VERSION);
                ds << genesisblock.vptx[i];
                obj.push_back(Pair("rawtx", HexStr(ds.begin(), ds.end())));
                return obj;
            }
        }

        if (SysCfg().IsTxIndex()) {
            CDiskTxPos postx;
            if (pCdMan->pContractCache->ReadTxIndex(txid, postx)) {
                CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
                CBlockHeader header;
                try {
                    file >> header;
                    fseek(file, postx.nTxOffset, SEEK_CUR);
                    file >> pBaseTx;
                    obj = pBaseTx->ToJson(*pCdMan->pAccountCache);
                    obj.push_back(Pair("confirmed_height", (int) header.GetHeight()));
                    obj.push_back(Pair("confirmed_time", (int) header.GetTime()));
                    obj.push_back(Pair("block_hash", header.GetHash().GetHex()));

                    if (pBaseTx->nTxType == LCONTRACT_INVOKE_TX) {
                        vector<CVmOperate> vOutput;
                        pCdMan->pContractCache->GetTxOutput(pBaseTx->GetHash(), vOutput);
                        Array outputArray;
                        for (auto& item : vOutput) {
                            outputArray.push_back(item.ToJson());
                        }
                        obj.push_back(Pair("list_output", outputArray));
                    }
                    CDataStream ds(SER_DISK, CLIENT_VERSION);
                    ds << pBaseTx;
                    obj.push_back(Pair("rawtx", HexStr(ds.begin(), ds.end())));
                } catch (std::exception &e) {
                    throw runtime_error(tfm::format("%s : Deserialize or I/O error - %s", __func__, e.what()).c_str());
                }
                return obj;
            }
        }
        {
            pBaseTx = mempool.Lookup(txid);
            if (pBaseTx.get()) {
                obj = pBaseTx->ToJson(*pCdMan->pAccountCache);
                CDataStream ds(SER_DISK, CLIENT_VERSION);
                ds << pBaseTx;
                obj.push_back(Pair("rawtx", HexStr(ds.begin(), ds.end())));
                return obj;
            }
        }
    }
    return obj;
}

Array GetTxAddressDetail(std::shared_ptr<CBaseTx> pBaseTx) {
    Array arrayDetail;
    Object obj;
    std::set<CKeyID> vKeyIdSet;
    auto spCW = std::make_shared<CCacheWrapper>(pCdMan);

    double dAmount = static_cast<double>(pBaseTx->GetValues()[SYMB::WICC]) / COIN;
    switch (pBaseTx->nTxType) {
        case BLOCK_REWARD_TX: {
            if (!pBaseTx->GetInvolvedKeyIds(*spCW, vKeyIdSet))
                return arrayDetail;

            obj.push_back(Pair("address", vKeyIdSet.begin()->ToAddress()));
            obj.push_back(Pair("category", "receive"));
            obj.push_back(Pair("amount", dAmount));
            obj.push_back(Pair("tx_type", "BLOCK_REWARD_TX"));
            arrayDetail.push_back(obj);

            break;
        }
        case ACCOUNT_REGISTER_TX: {
            if (!pBaseTx->GetInvolvedKeyIds(*spCW, vKeyIdSet))
                return arrayDetail;

            obj.push_back(Pair("address", vKeyIdSet.begin()->ToAddress()));
            obj.push_back(Pair("category", "send"));
            obj.push_back(Pair("amount", dAmount));
            obj.push_back(Pair("tx_type", "ACCOUNT_REGISTER_TX"));
            arrayDetail.push_back(obj);

            break;
        }
        case BCOIN_TRANSFER_TX: {
            CBaseCoinTransferTx* ptx = (CBaseCoinTransferTx*)pBaseTx.get();
            CKeyID sendKeyID;
            if (ptx->txUid.type() == typeid(CPubKey)) {
                sendKeyID = ptx->txUid.get<CPubKey>().GetKeyId();
            } else if (ptx->txUid.type() == typeid(CRegID)) {
                sendKeyID = ptx->txUid.get<CRegID>().GetKeyId(*pCdMan->pAccountCache);
            }

            CKeyID recvKeyId;
            if (ptx->toUid.type() == typeid(CKeyID)) {
                recvKeyId = ptx->toUid.get<CKeyID>();
            } else if (ptx->toUid.type() == typeid(CRegID)) {
                CRegID desRegID = ptx->toUid.get<CRegID>();
                recvKeyId       = desRegID.GetKeyId(*pCdMan->pAccountCache);
            }

            obj.push_back(Pair("tx_type", "BCOIN_TRANSFER_TX"));
            obj.push_back(Pair("from_address", sendKeyID.ToAddress()));
            obj.push_back(Pair("to_address", recvKeyId.ToAddress()));
            obj.push_back(Pair("transfer_amount", dAmount));
            obj.push_back(Pair("memo", HexStr(ptx->memo)));
            arrayDetail.push_back(obj);

            break;
        }
        case LCONTRACT_INVOKE_TX: {
            CLuaContractInvokeTx* ptx = (CLuaContractInvokeTx*)pBaseTx.get();
            CKeyID sendKeyID;
            if (ptx->txUid.type() == typeid(CPubKey)) {
                sendKeyID = ptx->txUid.get<CPubKey>().GetKeyId();
            } else if (ptx->txUid.type() == typeid(CRegID)) {
                sendKeyID = ptx->txUid.get<CRegID>().GetKeyId(*pCdMan->pAccountCache);
            }

            CKeyID recvKeyId;
            if (ptx->app_uid.type() == typeid(CRegID)) {
                CRegID appUid = ptx->app_uid.get<CRegID>();
                recvKeyId     = appUid.GetKeyId(*pCdMan->pAccountCache);
            }

            obj.push_back(Pair("tx_type",       "LCONTRACT_INVOKE_TX"));
            obj.push_back(Pair("from_address",  sendKeyID.ToAddress()));
            obj.push_back(Pair("to_address",    recvKeyId.ToAddress()));
            obj.push_back(Pair("arguments",     HexStr(ptx->arguments)));
            obj.push_back(Pair("transfer_amount", dAmount));
            arrayDetail.push_back(obj);

            vector<CVmOperate> vOutput;
            pCdMan->pContractCache->GetTxOutput(pBaseTx->GetHash(), vOutput);
            Array outputArray;
            for (auto& item : vOutput) {
                Object objOutPut;
                string address;
                if (item.accountType == ACCOUNT_TYPE::REGID) {
                    vector<unsigned char> vRegId(item.accountId, item.accountId + 6);
                    CRegID regId(vRegId);
                    CUserID userId(regId);
                    address = RegIDToAddress(userId);

                } else if (item.accountType == ACCOUNT_TYPE::BASE58ADDR) {
                    address.assign(item.accountId[0], sizeof(item.accountId));
                }

                objOutPut.push_back(Pair("address", address));

                uint64_t amount;
                memcpy(&amount, item.money, sizeof(item.money));
                double dAmount = amount / COIN;

                if (item.opType == ADD_BCOIN) {
                    objOutPut.push_back(Pair("category", "receive"));
                    objOutPut.push_back(Pair("amount", dAmount));
                } else if (item.opType == MINUS_BCOIN) {
                    objOutPut.push_back(Pair("category", "send"));
                    objOutPut.push_back(Pair("amount", -dAmount));
                }

                if (item.timeoutHeight > 0)
                    objOutPut.push_back(Pair("freeze_height", (int)item.timeoutHeight));

                arrayDetail.push_back(objOutPut);
            }

            break;
        }
        case LCONTRACT_DEPLOY_TX:
        case DELEGATE_VOTE_TX: {

            if (!pBaseTx->GetInvolvedKeyIds(*spCW, vKeyIdSet))
                return arrayDetail;

            double dAmount = static_cast<double>(pBaseTx->GetValues()[SYMB::WICC]) / COIN;

            obj.push_back(Pair("from_address", vKeyIdSet.begin()->ToAddress()));
            obj.push_back(Pair("category", "send"));
            obj.push_back(Pair("transfer_amount", dAmount));

            if (pBaseTx->nTxType == LCONTRACT_DEPLOY_TX)
                obj.push_back(Pair("tx_type", "LCONTRACT_DEPLOY_TX"));
            else if (pBaseTx->nTxType == DELEGATE_VOTE_TX)
                obj.push_back(Pair("tx_type", "DELEGATE_VOTE_TX"));

            arrayDetail.push_back(obj);

            break;
        }
        case BCOIN_TRANSFER_MTX: {
            CMulsigTx* ptx = (CMulsigTx*)pBaseTx.get();

            CAccount account;
            set<CPubKey> pubKeys;
            for (const auto& item : ptx->signaturePairs) {
                if (!pCdMan->pAccountCache->GetAccount(item.regid, account))
                    return arrayDetail;

                pubKeys.insert(account.owner_pubkey);
            }

            CMulsigScript script;
            script.SetMultisig(ptx->required, pubKeys);
            CKeyID sendKeyId = script.GetID();

            CKeyID recvKeyId;
            if (ptx->desUserId.type() == typeid(CKeyID)) {
                recvKeyId = ptx->desUserId.get<CKeyID>();
            } else if (ptx->desUserId.type() == typeid(CRegID)) {
                CRegID desRegID = ptx->desUserId.get<CRegID>();
                recvKeyId       = desRegID.GetKeyId(*pCdMan->pAccountCache);
            }

            obj.push_back(Pair("tx_type", "BCOIN_TRANSFER_MTX"));
            obj.push_back(Pair("from_address", sendKeyId.ToAddress()));
            obj.push_back(Pair("to_address", recvKeyId.ToAddress()));
            obj.push_back(Pair("transfer_amount", dAmount));
            obj.push_back(Pair("memo", HexStr(ptx->memo)));

            arrayDetail.push_back(obj);
            break;
        }
        //TODO: other Tx types
        case CDP_STAKE_TX:
        case CDP_REDEEM_TX:
        case CDP_LIQUIDATE_TX:
        case PRICE_FEED_TX:
        case FCOIN_STAKE_TX:
        case DEX_TRADE_SETTLE_TX:
        case DEX_CANCEL_ORDER_TX:
        case DEX_LIMIT_BUY_ORDER_TX:
        case DEX_LIMIT_SELL_ORDER_TX:
        case DEX_MARKET_BUY_ORDER_TX:
        case DEX_MARKET_SELL_ORDER_TX:
        default:
            break;
    }

    return arrayDetail;
}

///////////////////////////////////////////////////////////////////////////////
// namespace JSON

const Value& JSON::GetObjectFieldValue(const Value &jsonObj, const string &fieldName) {
    const Value& jsonValue = find_value(jsonObj.get_obj(), fieldName);
    if (jsonValue.type() == null_type || jsonValue == null_type) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("field %s not found in json object", fieldName));
    }

    return jsonValue;
}

const char* JSON::GetValueTypeName(Value_type valueType) {
    if (valueType >= 0 && valueType <= json_spirit::Value_type::null_type) {
        return json_spirit::Value_type_name[valueType];
    }
    return "unknown";
}

///////////////////////////////////////////////////////////////////////////////
// namespace RPC_PARAM

ComboMoney RPC_PARAM::GetComboMoney(const Value &jsonValue,
                                    const TokenSymbol &defaultSymbol) {
    ComboMoney money;
    Value_type valueType = jsonValue.type();
    if (valueType == json_spirit::Value_type::int_type ) {
        money.symbol = defaultSymbol;
        money.amount = AmountToRawValue(jsonValue.get_int64());
        money.unit = COIN_UNIT::SAWI;

    } else if (valueType == json_spirit::Value_type::str_type) {
        if (!ParseRpcInputMoney(jsonValue.get_str(), money, defaultSymbol)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid combo money format");
        }
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid json value type: %s", JSON::GetValueTypeName(valueType)));
    }
    return money;
}

ComboMoney RPC_PARAM::GetFee(const Array& params, size_t index, TxType txType) {
    ComboMoney fee;
    if (params.size() > index) {
        fee = GetComboMoney(params[index], SYMB::WICC);
        if (!kFeeSymbolSet.count(fee.symbol))
            throw JSONRPCError(RPC_INVALID_PARAMS,
                strprintf("Fee symbol is %s, but expect %s", fee.symbol, GetFeeSymbolSetStr()));

        uint64_t minFee;
        if (!GetTxMinFee(txType, chainActive.Height(), fee.symbol, minFee))
            throw JSONRPCError(RPC_INVALID_PARAMS,
                strprintf("Can not find the min tx fee! symbol=%s", fee.symbol));
        if (fee.GetSawiAmount() < minFee)
            throw JSONRPCError(RPC_INVALID_PARAMS,
                strprintf("The given fee is too small: %llu < %llu sawi", fee.amount, minFee));
    } else {
        uint64_t minFee;
        if (!GetTxMinFee(txType, chainActive.Height(), SYMB::WICC, minFee))
            throw JSONRPCError(RPC_INVALID_PARAMS,
                strprintf("Can not find the min tx fee! symbol=%s", SYMB::WICC));
        fee.symbol = SYMB::WICC;
        fee.amount = minFee;
        fee.unit = COIN_UNIT::SAWI;
    }

    return fee;
}

uint64_t RPC_PARAM::GetWiccFee(const Array& params, size_t index, TxType txType) {
    uint64_t fee, minFee;
    if (!GetTxMinFee(txType, chainActive.Height(), SYMB::WICC, minFee))
        throw JSONRPCError(RPC_INVALID_PARAMS,
            strprintf("Can not find the min tx fee! symbol=%s", SYMB::WICC));
    if (params.size() > index) {
        fee = AmountToRawValue(params[index]);
        if (fee < minFee)
            throw JSONRPCError(RPC_INVALID_PARAMS,
                strprintf("The given fee is too small: %llu < %llu sawi", fee, minFee));
    } else {
        fee = minFee;
    }

    return fee;
}


CUserID RPC_PARAM::GetUserId(const Value &jsonValue) {
    auto pUserId = CUserID::ParseUserId(jsonValue.get_str());
    if (!pUserId) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid addr");
    }
    return *pUserId;
}


uint64_t RPC_PARAM::GetPrice(const Value &jsonValue) {
    // TODO: check price range??
    return AmountToRawValue(jsonValue);
}

uint256 RPC_PARAM::GetTxid(const Value &jsonValue, const string &paramName, bool canBeEmpty) {
    string binStr, errStr;
    if (!ParseHex(jsonValue.get_str(), binStr, errStr))
        throw JSONRPCError(RPC_INVALID_PARAMS, strprintf("Get param %s error! %s", paramName, errStr));
    
    if (binStr.empty()) {
        if (!canBeEmpty)
            throw JSONRPCError(RPC_INVALID_PARAMS, strprintf("Get param %s error! Can not be emtpy", paramName));
        return uint256();
    }

    if (binStr.size() != uint256::WIDTH) {
        throw JSONRPCError(RPC_INVALID_PARAMS, strprintf("Get param %s error! The hex str size should be %d", 
            paramName, uint256::WIDTH * 2));
    }
    return uint256(binStr.rbegin(), binStr.rend());
}

CAccount RPC_PARAM::GetUserAccount(CAccountDBCache &accountCache, const CUserID &userId) {
    CAccount account;
    if (!accountCache.GetAccount(userId, account))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                           strprintf("The account not exists! userId=%s", userId.ToString()));

    assert(!account.keyid.IsEmpty());
    return account;
}

TokenSymbol RPC_PARAM::GetOrderCoinSymbol(const Value &jsonValue) {
    // TODO: check coin symbol for orders
    return jsonValue.get_str();
}

TokenSymbol RPC_PARAM::GetOrderAssetSymbol(const Value &jsonValue) {
    // TODO: check asset symbol for oders
    return jsonValue.get_str();
}

TokenSymbol RPC_PARAM::GetAssetIssueSymbol(const Value &jsonValue) {
    TokenSymbol symbol = jsonValue.get_str();
    if (symbol.empty() || symbol.size() > MAX_TOKEN_SYMBOL_LEN)
        throw JSONRPCError(RPC_INVALID_PARAMS,
                           strprintf("asset_symbol is empty or len=%d greater than %d",
                                     symbol.size(), MAX_TOKEN_SYMBOL_LEN));
    return symbol;
}

TokenName RPC_PARAM::GetAssetName(const Value &jsonValue) {
    TokenName name = jsonValue.get_str();
    if (name.empty() || name.size() > MAX_ASSET_NAME_LEN)
        throw JSONRPCError(RPC_INVALID_PARAMS,
                           strprintf("asset name is empty or len=%d greater than %d", name.size(),
                                     MAX_ASSET_NAME_LEN));
    return name;
}

void RPC_PARAM::CheckAccountBalance(CAccount &account, const TokenSymbol &tokenSymbol,
                                    const BalanceOpType opType, const uint64_t &value) {
    if (!account.OperateBalance(tokenSymbol, opType, value))
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
                           strprintf("Account does not have enough %s", tokenSymbol));
}

void RPC_PARAM::CheckActiveOrderExisted(CDexDBCache &dexCache, const uint256 &orderTxid) {
    CDEXActiveOrder activeOrder;
    if (!dexCache.GetActiveOrder(orderTxid, activeOrder))
        throw JSONRPCError(RPC_DEX_ORDER_INACTIVE, "Order is inactive or not existed");
}


bool RPC_PARAM::ParseHex(const string &hexStr, string &binStrOut, string &errStrOut) {
    int begin = 0;
    int end = hexStr.size();
    // skip left spaces
    while (begin != end && isspace(hexStr[begin]))
        begin++;
    // skip right spaces
    while (begin != end && isspace(hexStr[end - 1]))
        end--;
    
    // skip 0x
    if (begin + 1 < end && hexStr[begin] == '0' && tolower(hexStr[begin + 1]) == 'x')
        begin += 2;

    if (begin == end) return true; // ignore empty hex str

    if ((end - begin) % 2 != 0) {
        errStrOut = "Invalid hex format! Hex digit count is not even number";
        return false;
    }

    binStrOut.reserve((end - begin) / 2);
    while (begin != end) {
        uint8_t c1 = HexDigit(hexStr[begin]);
        uint8_t c2 = HexDigit(hexStr[begin + 1]);
        if (c1 == (uint8_t)-1 || c2 == (uint8_t)-1) {
            errStrOut = strprintf("Invalid hex char in the position %d or %d", begin, begin + 1);
            return false;
        }
        binStrOut.push_back((c1 << 4) | c2);
        begin += 2;
    }
    return true;
}