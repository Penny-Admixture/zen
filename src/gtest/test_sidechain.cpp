#include <gtest/gtest.h>
#include <sc/sidechain.h>
#include <chainparams.h>
#include <chainparamsbase.h>
#include <consensus/validation.h>
#include <txmempool.h>
#include <undo.h>
#include <main.h>

#include "gtestUtils.h"

class SidechainTestSuite: public ::testing::Test {

public:
    SidechainTestSuite() :
            sidechainManager(Sidechain::ScMgr::instance()), coinViewCache(),
            aBlock(), aTransaction(), anHeight(1789),
            txState(), aFeeRate(), aMemPool(aFeeRate){};

    ~SidechainTestSuite() {
        sidechainManager.reset();
    };

    void SetUp() override {
        SelectBaseParams(CBaseChainParams::REGTEST);
        SelectParams(CBaseChainParams::REGTEST);

        ASSERT_TRUE(sidechainManager.initPersistence(0, false, Sidechain::ScMgr::persistencePolicy::STUB));
    };

    void TearDown() override {};

protected:
    //Subjects under test
    Sidechain::ScMgr&           sidechainManager;
    Sidechain::ScCoinsViewCache coinViewCache;

    //Helpers
    CBlock              aBlock;
    CTransaction        aTransaction;
    int                 anHeight;
    CValidationState    txState;

    CFeeRate   aFeeRate;
    CTxMemPool aMemPool;
    CBlockUndo aBlockUndo;

    void preFillSidechainsCollection();
    CBlockUndo   createBlockUndoWith(const uint256 & scId, int height, CAmount amount);
    CBlockUndo   createEmptyBlockUndo();
};

///////////////////////////////////////////////////////////////////////////////
/////////////////////////// checkTxSemanticValidity ///////////////////////////
///////////////////////////////////////////////////////////////////////////////
TEST_F(SidechainTestSuite, TransparentCcNullTxsAreSemanticallyValid) {
    aTransaction = gtestUtils::createTransparentTx(/*ccIsNull = */true);

    //test
    bool res = sidechainManager.checkTxSemanticValidity(aTransaction, txState);

    //checks
    EXPECT_TRUE(res);
    EXPECT_TRUE(txState.IsValid());
}

TEST_F(SidechainTestSuite, TransparentNonCcNullTxsAreNotSemanticallyValid) {
    aTransaction = gtestUtils::createTransparentTx(/*ccIsNull = */false);

    //test
    bool res = sidechainManager.checkTxSemanticValidity(aTransaction, txState);

    //checks
    EXPECT_FALSE(res);
    EXPECT_FALSE(txState.IsValid());
    EXPECT_TRUE(txState.GetRejectCode() == REJECT_INVALID)
        <<"wrong reject code. Value returned: "<<txState.GetRejectCode();
}

TEST_F(SidechainTestSuite, SproutCcNullTxsAreCurrentlySupported) {
    aTransaction = gtestUtils::createSproutTx(/*ccIsNull = */true);

    //test
    bool res = sidechainManager.checkTxSemanticValidity(aTransaction, txState);

    //checks
    EXPECT_TRUE(res);
    EXPECT_TRUE(txState.IsValid());
}

TEST_F(SidechainTestSuite, SproutNonCcNullTxsAreCurrentlySupported) {
    aTransaction = gtestUtils::createSproutTx(/*ccIsNull = */false);

    //test
    bool res = sidechainManager.checkTxSemanticValidity(aTransaction, txState);

    //checks
    EXPECT_FALSE(res);
    EXPECT_FALSE(txState.IsValid());
    EXPECT_TRUE(txState.GetRejectCode() == REJECT_INVALID)
        <<"wrong reject code. Value returned: "<<txState.GetRejectCode();
}

TEST_F(SidechainTestSuite, SidechainCreationsWithoutForwardTransferAreNotSemanticallyValid) {
    aTransaction = gtestUtils::createSidechainTxWithNoFwdTransfer(uint256S("1492"));

    //test
    bool res = sidechainManager.checkTxSemanticValidity(aTransaction, txState);

    //checks
    EXPECT_FALSE(res);
    EXPECT_FALSE(txState.IsValid());
    EXPECT_TRUE(txState.GetRejectCode() == REJECT_INVALID)
        <<"wrong reject code. Value returned: "<<txState.GetRejectCode();
}

TEST_F(SidechainTestSuite, SidechainCreationsWithPositiveForwardTransferAreSemanticallyValid) {
    aTransaction = gtestUtils::createSidechainTxWith( uint256S("1492"), CAmount(1000));

    //test
    bool res = sidechainManager.checkTxSemanticValidity(aTransaction, txState);

    //checks
    EXPECT_TRUE(res);
    EXPECT_TRUE(txState.IsValid());
}

TEST_F(SidechainTestSuite, SidechainCreationsWithTooLargePositiveForwardTransferAreNotSemanticallyValid) {
    aTransaction = gtestUtils::createSidechainTxWith(uint256S("1492"), CAmount(MAX_MONEY +1));

    //test
    bool res = sidechainManager.checkTxSemanticValidity(aTransaction, txState);

    //checks
    EXPECT_FALSE(res);
    EXPECT_FALSE(txState.IsValid());
    EXPECT_TRUE(txState.GetRejectCode() == REJECT_INVALID)
        <<"wrong reject code. Value returned: "<<txState.GetRejectCode();
}

TEST_F(SidechainTestSuite, SidechainCreationsWithZeroForwardTransferAreNotSemanticallyValid) {
    aTransaction = gtestUtils::createSidechainTxWith(uint256S("1492"), CAmount(0));

    //test
    bool res = sidechainManager.checkTxSemanticValidity(aTransaction, txState);

    //checks
    EXPECT_FALSE(res);
    EXPECT_FALSE(txState.IsValid());
    EXPECT_TRUE(txState.GetRejectCode() == REJECT_INVALID)
        <<"wrong reject code. Value returned: "<<txState.GetRejectCode();
}

TEST_F(SidechainTestSuite, SidechainCreationsWithNegativeForwardTransferNotAreSemanticallyValid) {
    aTransaction = gtestUtils::createSidechainTxWith(uint256S("1492"), CAmount(-1));

    //test
    bool res = sidechainManager.checkTxSemanticValidity(aTransaction, txState);

    //checks
    EXPECT_FALSE(res);
    EXPECT_FALSE(txState.IsValid());
    EXPECT_TRUE(txState.GetRejectCode() == REJECT_INVALID)
        <<"wrong reject code. Value returned: "<<txState.GetRejectCode();
}

TEST_F(SidechainTestSuite, FwdTransferCumulatedAmountDoesNotOverFlow) {
    uint256 scId = uint256S("1492");
    CAmount initialFwdTrasfer(1);
    aTransaction = gtestUtils::createSidechainTxWith(scId, initialFwdTrasfer);
    gtestUtils::extendTransaction(aTransaction, scId, MAX_MONEY);

    //test
    bool res = sidechainManager.checkTxSemanticValidity(aTransaction, txState);

    //checks
    EXPECT_FALSE(res);
    EXPECT_FALSE(txState.IsValid());
    EXPECT_TRUE(txState.GetRejectCode() == REJECT_INVALID)
        <<"wrong reject code. Value returned: "<<txState.GetRejectCode();
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////////// IsTxApplicableToState ////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TEST_F(SidechainTestSuite, NewScCreationsAreApplicableToState) {
    aTransaction = gtestUtils::createSidechainTxWith(uint256S("1492"), CAmount(1953));

    //test
    bool res = sidechainManager.IsTxApplicableToState(aTransaction, &coinViewCache);

    //checks
    EXPECT_TRUE(res);
}

TEST_F(SidechainTestSuite, DuplicatedScCreationsAreNotApplicableToState) {
    uint256 scId = uint256S("1492");
    aTransaction = gtestUtils::createSidechainTxWith(scId, CAmount(1953));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, anHeight);

    CTransaction duplicatedTx = gtestUtils::createSidechainTxWith(scId, CAmount(1815));

    //test
    bool res = sidechainManager.IsTxApplicableToState(duplicatedTx, &coinViewCache);

    //checks
    EXPECT_FALSE(res);
}

TEST_F(SidechainTestSuite, ForwardTransfersToExistingSCsAreApplicableToState) {
    uint256 scId = uint256S("1492");
    aTransaction = gtestUtils::createSidechainTxWith(scId, CAmount(1953));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, anHeight);

    aTransaction = gtestUtils::createFwdTransferTxWith(scId, CAmount(5));

    //test
    bool res = sidechainManager.IsTxApplicableToState(aTransaction, &coinViewCache);

    //checks
    EXPECT_TRUE(res);
}

TEST_F(SidechainTestSuite, ForwardTransfersToNonExistingSCsAreNotApplicableToState) {
    aTransaction = gtestUtils::createFwdTransferTxWith(uint256S("1492"), CAmount(1815));

    //test
    bool res = sidechainManager.IsTxApplicableToState(aTransaction, &coinViewCache);

    //checks
    EXPECT_FALSE(res);
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////////// IsTxAllowedInMempool /////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TEST_F(SidechainTestSuite, ScCreationTxsAreAllowedInEmptyMemPool) {
    aTransaction = gtestUtils::createSidechainTxWith(uint256S("1492"), CAmount(1953));

    //test
    bool res = sidechainManager.IsTxAllowedInMempool(aMemPool, aTransaction, txState);

    //check
    EXPECT_TRUE(res);
    EXPECT_TRUE(txState.IsValid());
}

TEST_F(SidechainTestSuite, NewScCreationTxsAreAllowedInMemPool) {
    aTransaction = gtestUtils::createSidechainTxWith(uint256S("1987"), CAmount(1994));
    CTxMemPoolEntry memPoolEntry(aTransaction, CAmount(), GetTime(), double(0.0), anHeight);
    aMemPool.addUnchecked(aTransaction.GetHash(), memPoolEntry);

    CTransaction aNewTx = gtestUtils::createSidechainTxWith(uint256S("1991"), CAmount(5));

    //test
    bool res = sidechainManager.IsTxAllowedInMempool(aMemPool, aNewTx, txState);

    //check
    EXPECT_TRUE(res);
    EXPECT_TRUE(txState.IsValid());
}

TEST_F(SidechainTestSuite, DuplicatedScCreationTxsAreNotAllowedInMemPool) {
    uint256 scId = uint256S("1987");

    aTransaction = gtestUtils::createSidechainTxWith(scId, CAmount(10));
    CTxMemPoolEntry memPoolEntry(aTransaction, CAmount(), GetTime(), double(0.0), anHeight);
    aMemPool.addUnchecked(aTransaction.GetHash(), memPoolEntry);

    CTransaction duplicatedTx = gtestUtils::createSidechainTxWith(scId, CAmount(15));

    //test
    bool res = sidechainManager.IsTxAllowedInMempool(aMemPool, aTransaction, txState);

    //check
    EXPECT_FALSE(res);
    EXPECT_FALSE(txState.IsValid());
    EXPECT_TRUE(txState.GetRejectCode() == REJECT_INVALID)
        <<"wrong reject code. Value returned: "<<txState.GetRejectCode();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////// ApplyMatureBalances /////////////////////////////
///////////////////////////////////////////////////////////////////////////////
TEST_F(SidechainTestSuite, InitialCoinsTransferDoesNotModifyScBalanceBeforeCoinsMaturity) {
    uint256 scId = uint256S("a1b2");
    CAmount initialAmount = 1000;
    int scCreationHeight = 5;
    aTransaction = gtestUtils::createSidechainTxWith(scId, initialAmount);
    coinViewCache.UpdateScInfo(aTransaction, aBlock, scCreationHeight);

    int coinMaturityHeight = scCreationHeight + Params().ScCoinsMaturity();
    int lookupBlockHeight = coinMaturityHeight - 1;

    //test
    bool res = coinViewCache.ApplyMatureBalances(lookupBlockHeight, aBlockUndo);

    //check
    EXPECT_TRUE(res);

    coinViewCache.Flush();
    EXPECT_TRUE(sidechainManager.getScBalance(scId) < initialAmount)
        <<"resulting balance is "<<coinViewCache.getScInfoMap().at(scId).balance
        <<" while initial amount is "<<initialAmount;
}

TEST_F(SidechainTestSuite, InitialCoinsTransferModifiesScBalanceAtCoinMaturity) {
    uint256 scId = uint256S("a1b2");
    CAmount initialAmount = 1000;
    int scCreationHeight = 7;
    aTransaction = gtestUtils::createSidechainTxWith(scId, initialAmount);
    coinViewCache.UpdateScInfo(aTransaction, aBlock, scCreationHeight);

    int coinMaturityHeight = scCreationHeight + Params().ScCoinsMaturity();
    int lookupBlockHeight = coinMaturityHeight;

    //test
    bool res = coinViewCache.ApplyMatureBalances(lookupBlockHeight, aBlockUndo);

    //checks
    EXPECT_TRUE(res);

    coinViewCache.Flush();
    EXPECT_TRUE(sidechainManager.getScBalance(scId) == initialAmount)
        <<"resulting balance is "<<coinViewCache.getScInfoMap().at(scId).balance
        <<" expected one is "<<initialAmount;
}

TEST_F(SidechainTestSuite, InitialCoinsTransferDoesNotModifyScBalanceAfterCoinsMaturity) {
    uint256 scId = uint256S("a1b2");
    CAmount initialAmount = 1000;
    int scCreationHeight = 11;
    aTransaction = gtestUtils::createSidechainTxWith(scId, initialAmount);
    coinViewCache.UpdateScInfo(aTransaction, aBlock, scCreationHeight);

    int coinMaturityHeight = anHeight + Params().ScCoinsMaturity();
    int lookupBlockHeight = coinMaturityHeight + 1;

    //test
    bool res = coinViewCache.ApplyMatureBalances(lookupBlockHeight, aBlockUndo);

    //check
    EXPECT_FALSE(res);

    coinViewCache.Flush();
    EXPECT_TRUE(sidechainManager.getScBalance(scId) < initialAmount)
        <<"resulting balance is "<<coinViewCache.getScInfoMap().at(scId).balance
        <<" while initial amount is "<<initialAmount;
}

///////////////////////////////////////////////////////////////////////////////
/////////////////////////// RestoreImmatureBalances ///////////////////////////
///////////////////////////////////////////////////////////////////////////////
TEST_F(SidechainTestSuite, RestoreImmatureBalancesAffectsScBalance) {
    uint256 scId = uint256S("ca1985");
    int scCreationHeight = 71;
    aTransaction = gtestUtils::createSidechainTxWith(scId, CAmount(34));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, scCreationHeight);

    coinViewCache.ApplyMatureBalances(scCreationHeight + Params().ScCoinsMaturity(), aBlockUndo);
    CAmount scBalance = coinViewCache.getScInfoMap().at(scId).balance;

    CAmount amountToUndo = 17;
    aBlockUndo = createBlockUndoWith(scId,scCreationHeight,amountToUndo);

    //test
    bool res = coinViewCache.RestoreImmatureBalances(scCreationHeight, aBlockUndo);

    //checks
    EXPECT_TRUE(res);
    EXPECT_TRUE(coinViewCache.getScInfoMap().at(scId).balance == scBalance - amountToUndo)
        <<"balance after restore is "<<coinViewCache.getScInfoMap().at(scId).balance
        <<" instead of"<< scBalance - amountToUndo;
}

TEST_F(SidechainTestSuite, YouCannotRestoreMoreCoinsThanAvailableBalance) {
    uint256 scId = uint256S("ca1985");
    int scCreationHeight = 1991;
    aTransaction = gtestUtils::createSidechainTxWith(scId, CAmount(34));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, scCreationHeight);

    coinViewCache.ApplyMatureBalances(scCreationHeight + Params().ScCoinsMaturity(), aBlockUndo);
    CAmount scBalance = coinViewCache.getScInfoMap().at(scId).balance;

    aBlockUndo = createBlockUndoWith(scId,scCreationHeight,CAmount(50));

    //test
    bool res = coinViewCache.RestoreImmatureBalances(scCreationHeight, aBlockUndo);

    //checks
    EXPECT_FALSE(res);
    EXPECT_TRUE(coinViewCache.getScInfoMap().at(scId).balance == scBalance)
        <<"balance after restore is "<<coinViewCache.getScInfoMap().at(scId).balance
        <<" instead of"<< scBalance;
}

TEST_F(SidechainTestSuite, RestoringBeforeBalanceMaturesHasNoEffects) {
    uint256 scId = uint256S("ca1985");
    int scCreationHeight = 71;
    aTransaction = gtestUtils::createSidechainTxWith(scId, CAmount(34));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, scCreationHeight);

    coinViewCache.ApplyMatureBalances(scCreationHeight + Params().ScCoinsMaturity() -1, aBlockUndo);

    aBlockUndo = createBlockUndoWith(scId,scCreationHeight,CAmount(17));

    //test
    bool res = coinViewCache.RestoreImmatureBalances(scCreationHeight, aBlockUndo);

    //checks
    EXPECT_FALSE(res);
    EXPECT_TRUE(coinViewCache.getScInfoMap().at(scId).balance == 0)
        <<"balance after restore is "<<coinViewCache.getScInfoMap().at(scId).balance
        <<" instead of 0";
}

TEST_F(SidechainTestSuite, YouCannotRestoreCoinsFromInexistentSc) {
    uint256 inexistentScId = uint256S("ca1985");
    int scCreationHeight = 71;

    CAmount amountToUndo = 10;

    aBlockUndo = createBlockUndoWith(inexistentScId,scCreationHeight,amountToUndo);

    //test
    bool res = coinViewCache.RestoreImmatureBalances(scCreationHeight, aBlockUndo);

    //checks
    EXPECT_FALSE(res);
}

///////////////////////////////////////////////////////////////////////////////
/////////////////////////////// RevertTxOutputs ///////////////////////////////
///////////////////////////////////////////////////////////////////////////////
TEST_F(SidechainTestSuite, RevertingScCreationTxRemovesTheSc) {
    uint256 scId = uint256S("a1b2");
    int scCreationHeight = 1;
    aTransaction = gtestUtils::createSidechainTxWith(scId, CAmount(10));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, scCreationHeight);

    //test
    bool res = coinViewCache.RevertTxOutputs(aTransaction, scCreationHeight);

    //checks
    EXPECT_TRUE(res);
    EXPECT_FALSE(coinViewCache.sidechainExists(scId));
}

TEST_F(SidechainTestSuite, RevertingFwdTransferRemovesCoinsFromImmatureBalance) {
    uint256 scId = uint256S("a1b2");
    int scCreationHeight = 1;
    aTransaction = gtestUtils::createSidechainTxWith(scId, CAmount(10));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, scCreationHeight);

    int fwdTxHeight = 5;
    aTransaction = gtestUtils::createFwdTransferTxWith(scId, CAmount(7));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, fwdTxHeight);

    //test
    bool res = coinViewCache.RevertTxOutputs(aTransaction, fwdTxHeight);

    //checks
    EXPECT_TRUE(res);
    Sidechain::ScInfo viewInfo = coinViewCache.getScInfoMap().at(scId);
    EXPECT_TRUE(viewInfo.mImmatureAmounts.count(fwdTxHeight + Params().ScCoinsMaturity()) == 0)
        <<"resulting immature amount is "<< viewInfo.mImmatureAmounts.count(fwdTxHeight + Params().ScCoinsMaturity());
}

TEST_F(SidechainTestSuite, ScCreationTxCannotBeRevertedIfScIsNotPreviouslyCreated) {
    aTransaction = gtestUtils::createSidechainTxWith(uint256S("a1b2"),CAmount(15));

    //test
    bool res = coinViewCache.RevertTxOutputs(aTransaction, anHeight);

    //checks
    EXPECT_FALSE(res);
}

TEST_F(SidechainTestSuite, FwdTransferTxToUnexistingScCannotBeReverted) {
    aTransaction = gtestUtils::createFwdTransferTxWith(uint256S("a1b2"), CAmount(999));

    //test
    bool res = coinViewCache.RevertTxOutputs(aTransaction, anHeight);

    //checks
    EXPECT_FALSE(res);
}

TEST_F(SidechainTestSuite, RevertingAFwdTransferOnTheWrongHeightHasNoEffect) {
    uint256 scId = uint256S("a1b2");
    int scCreationHeight = 1;
    aTransaction = gtestUtils::createSidechainTxWith(scId, CAmount(10));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, scCreationHeight);

    int fwdTxHeight = 5;
    CAmount fwdAmount = 7;
    aTransaction = gtestUtils::createFwdTransferTxWith(scId, fwdAmount);
    coinViewCache.UpdateScInfo(aTransaction, aBlock, fwdTxHeight);

    //test
    int faultyHeight = fwdTxHeight -1;
    bool res = coinViewCache.RevertTxOutputs(aTransaction, faultyHeight);

    //checks
    EXPECT_FALSE(res);
    Sidechain::ScInfo viewInfo = coinViewCache.getScInfoMap().at(scId);
    EXPECT_TRUE(viewInfo.mImmatureAmounts.at(fwdTxHeight + Params().ScCoinsMaturity()) == fwdAmount)
        <<"Immature amount is "<<viewInfo.mImmatureAmounts.at(fwdTxHeight + Params().ScCoinsMaturity())
        <<"instead of "<<fwdAmount;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////// UpdateScInfo ////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TEST_F(SidechainTestSuite, NewSCsAreRegistered) {
    uint256 newScId = uint256S("1492");
    aTransaction = gtestUtils::createSidechainTxWith(newScId, CAmount(1));

    //test
    bool res = coinViewCache.UpdateScInfo(aTransaction, aBlock, anHeight);

    //check
    EXPECT_TRUE(res);
    EXPECT_TRUE(coinViewCache.sidechainExists(newScId));
}

TEST_F(SidechainTestSuite, DuplicatedSCsAreRejected) {
    uint256 scId = uint256S("1492");
    aTransaction = gtestUtils::createSidechainTxWith(scId, CAmount(1));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, anHeight);

    CTransaction duplicatedTx = gtestUtils::createSidechainTxWith(scId, CAmount(999));

    //test
    bool res = coinViewCache.UpdateScInfo(duplicatedTx, aBlock, anHeight);

    //check
    EXPECT_FALSE(res);
}

TEST_F(SidechainTestSuite, NoRollbackIsPerformedOnceInvalidTransactionIsEncountered) {
    uint256 firstScId = uint256S("1492");
    uint256 secondScId = uint256S("1912");
    aTransaction = gtestUtils::createSidechainTxWith(firstScId, CAmount(10));
    gtestUtils::extendTransaction(aTransaction, firstScId, CAmount(20));
    gtestUtils::extendTransaction(aTransaction, secondScId, CAmount(30));

    //test
    bool res = coinViewCache.UpdateScInfo(aTransaction, aBlock, anHeight);

    //check
    EXPECT_FALSE(res);
    EXPECT_TRUE(coinViewCache.sidechainExists(firstScId));
    EXPECT_FALSE(coinViewCache.sidechainExists(secondScId));
}

TEST_F(SidechainTestSuite, ForwardTransfersToNonExistentSCsAreRejected) {
    uint256 nonExistentId = uint256S("1492");
    aTransaction = gtestUtils::createFwdTransferTxWith(nonExistentId, CAmount(10));

    //test
    bool res = coinViewCache.UpdateScInfo(aTransaction, aBlock, anHeight);

    //check
    EXPECT_FALSE(res);
    EXPECT_FALSE(coinViewCache.sidechainExists(nonExistentId));
}

TEST_F(SidechainTestSuite, ForwardTransfersToExistentSCsAreRegistered) {
    uint256 newScId = uint256S("1492");
    aTransaction = gtestUtils::createSidechainTxWith(newScId, CAmount(5));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, anHeight);

    aTransaction = gtestUtils::createFwdTransferTxWith(newScId, CAmount(15));

    //test
    bool res = coinViewCache.UpdateScInfo(aTransaction, aBlock, anHeight);

    //check
    EXPECT_TRUE(res);
}

///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Flush /////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
TEST_F(SidechainTestSuite, FlushAlignsPersistedTxsWithViewOnes) {
    aTransaction = gtestUtils::createSidechainTxWith(uint256S("a1b2"), CAmount(1));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, anHeight);

    //prerequisites
    ASSERT_TRUE(sidechainManager.getScInfoMap().size() == 0);

    //test
    bool res = coinViewCache.Flush();

    //check
    EXPECT_TRUE(res);
    EXPECT_TRUE(sidechainManager.getScInfoMap() == coinViewCache.getScInfoMap());
}

TEST_F(SidechainTestSuite, UponViewCreationAllPersistedTxsAreLoaded) {
    preFillSidechainsCollection();

    //test
    Sidechain::ScCoinsViewCache newView;

    //check
    EXPECT_TRUE(sidechainManager.getScInfoMap() == newView.getScInfoMap());
}

TEST_F(SidechainTestSuite, FlushPersistsNewSidechains) {
    uint256 scId = uint256S("a1b2");
    aTransaction = gtestUtils::createSidechainTxWith(scId, CAmount(1000));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, anHeight);

    //test
    bool res = coinViewCache.Flush();

    //checks
    EXPECT_TRUE(res);
    EXPECT_TRUE(sidechainManager.sidechainExists(scId));
}

TEST_F(SidechainTestSuite, FlushPersistsForwardTransfers) {
    uint256 scId = uint256S("a1b2");
    int scCreationHeight = 1;
    aTransaction = gtestUtils::createSidechainTxWith(scId, CAmount(1));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, scCreationHeight);
    coinViewCache.Flush();

    CAmount fwdTxAmount = 1000;
    int fwdTxHeght = scCreationHeight + 10;
    int fwdTxMaturityHeight = fwdTxHeght + Params().ScCoinsMaturity();
    aTransaction = gtestUtils::createFwdTransferTxWith(scId, CAmount(1000));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, fwdTxHeght);

    //test
    bool res = coinViewCache.Flush();

    //checks
    EXPECT_TRUE(res);

    Sidechain::ScInfo persistedInfo = sidechainManager.getScInfoMap().at(scId);
    ASSERT_TRUE(persistedInfo.mImmatureAmounts.at(fwdTxMaturityHeight) == fwdTxAmount)
        <<"Following flush, persisted fwd amount should equal the one in view";
}

TEST_F(SidechainTestSuite, FlushPersistScErasureToo) {
    uint256 scId = uint256S("a1b2");
    aTransaction = gtestUtils::createSidechainTxWith(scId, CAmount(10));
    coinViewCache.UpdateScInfo(aTransaction, aBlock, anHeight);
    coinViewCache.Flush();

    coinViewCache.RevertTxOutputs(aTransaction, anHeight);

    //test
    bool res = coinViewCache.Flush();

    //checks
    EXPECT_TRUE(res);
    EXPECT_FALSE(sidechainManager.sidechainExists(scId));
}

///////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Structural UTs ////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
TEST_F(SidechainTestSuite, ManagerIsSingleton) {
    //test
    Sidechain::ScMgr& rAnotherScMgrInstance = Sidechain::ScMgr::instance();

    //check
    EXPECT_TRUE(&sidechainManager == &rAnotherScMgrInstance)
            << "ScManager Instances have different address:"
            << &sidechainManager << " and " << &rAnotherScMgrInstance;
}

TEST_F(SidechainTestSuite, ManagerDoubleInitializationIsForbidden) {
    //test
    bool res = sidechainManager.initPersistence(size_t(0), false, Sidechain::ScMgr::STUB);

    //Checks
    EXPECT_FALSE(res) << "Db double initialization should be forbidden";
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////// Test Fixture definitions ///////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SidechainTestSuite::preFillSidechainsCollection() {
    Sidechain::ScInfoMap & rManagerInternalMap
        = const_cast<Sidechain::ScInfoMap&>(sidechainManager.getScInfoMap());

    Sidechain::ScInfo info;
    uint256 scId;

    scId = uint256S("a123");
    info.creationBlockHash = uint256S("aaaa");
    info.creationBlockHeight = 1992;
    info.creationTxHash = uint256S("bbbb");
    rManagerInternalMap[scId] = info;

    scId = uint256S("b987");
    info.creationBlockHash = uint256S("1111");
    info.creationBlockHeight = 1993;
    info.creationTxHash = uint256S("2222");
    rManagerInternalMap[scId] = info;
}

CBlockUndo SidechainTestSuite::createBlockUndoWith(const uint256 & scId, int height, CAmount amount)
{
    CBlockUndo retVal;
    std::map<int, CAmount> AmountPerHeight;
    AmountPerHeight[height] = amount;
    retVal.msc_iaundo[scId] = AmountPerHeight;

    return retVal;
}

CBlockUndo SidechainTestSuite::createEmptyBlockUndo()
{
    return CBlockUndo();
}
