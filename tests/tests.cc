#ifndef CATCH_CONFIG_MAIN
#  define CATCH_CONFIG_MAIN
#endif

#include "atm.hpp"
#include "catch.hpp"

/////////////////////////////////////////////////////////////////////////////////////////////
//                             Helper Definitions //
/////////////////////////////////////////////////////////////////////////////////////////////

bool CompareFiles(const std::string& p1, const std::string& p2) {
  std::ifstream f1(p1);
  std::ifstream f2(p2);

  if (f1.fail() || f2.fail()) {
    return false;  // file problem
  }

  std::string f1_read;
  std::string f2_read;
  while (f1.good() || f2.good()) {
    f1 >> f1_read;
    f2 >> f2_read;
    if (f1_read != f2_read || (f1.good() && !f2.good()) ||
        (!f1.good() && f2.good()))
      return false;
  }
  return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Test Cases
/////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Example: Create a new account", "[ex-1]") {
  Atm atm;
  atm.RegisterAccount(12345678, 1234, "Sam Sepiol", 300.30);
  auto accounts = atm.GetAccounts();
  REQUIRE(accounts.contains({12345678, 1234}));
  REQUIRE(accounts.size() == 1);

  Account sam_account = accounts[{12345678, 1234}];
  REQUIRE(sam_account.owner_name == "Sam Sepiol");
  REQUIRE(sam_account.balance == 300.30);

  auto transactions = atm.GetTransactions();
  REQUIRE(accounts.contains({12345678, 1234}));
  REQUIRE(accounts.size() == 1);
  std::vector<std::string> empty;
  REQUIRE(transactions[{12345678, 1234}] == empty);
}

TEST_CASE("Example: Simple widthdraw", "[ex-2]") {
  Atm atm;
  atm.RegisterAccount(12345678, 1234, "Sam Sepiol", 300.30);
  atm.WithdrawCash(12345678, 1234, 20);
  auto accounts = atm.GetAccounts();
  Account sam_account = accounts[{12345678, 1234}];

  REQUIRE(sam_account.balance == 280.30);
}

TEST_CASE("Example: Print Prompt Ledger", "[ex-3]") {
  Atm atm;
  atm.RegisterAccount(12345678, 1234, "Sam Sepiol", 300.30);
  auto& transactions = atm.GetTransactions();
  transactions[{12345678, 1234}].push_back(
      "Withdrawal - Amount: $200.40, Updated Balance: $99.90");
  transactions[{12345678, 1234}].push_back(
      "Deposit - Amount: $40000.00, Updated Balance: $40099.90");
  transactions[{12345678, 1234}].push_back(
      "Deposit - Amount: $32000.00, Updated Balance: $72099.90");
  atm.PrintLedger("./prompt.txt", 12345678, 1234);
  REQUIRE(CompareFiles("./ex-1.txt", "./prompt.txt"));
}
//Additional ones:

/////////////////////////////////////////////////////////////////////////////////////////////
// Additional Tests (Adversarial / Bug-finding)
/////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("RegisterAccount: duplicate (card,pin) should throw invalid_argument", "[reg-dup]") {
  // What this catches:
  // If the implementation accidentally overwrites an existing account
  // instead of rejecting duplicates, this test will fail.
  Atm atm;
  atm.RegisterAccount(11111111, 1111, "Alice", 10.0);

  REQUIRE_THROWS_AS(atm.RegisterAccount(11111111, 1111, "Evil Overwrite", 999.0),
                    std::invalid_argument);
}

TEST_CASE("RegisterAccount: same card number but different PIN should be allowed", "[reg-same-card-diff-pin]") {
  // What this catches:
  // If they incorrectly key accounts by only card_num (ignoring pin),
  // the second registration may throw or overwrite.
  Atm atm;
  atm.RegisterAccount(22222222, 1111, "User1", 10.0);
  atm.RegisterAccount(22222222, 2222, "User2", 20.0);

  REQUIRE(atm.CheckBalance(22222222, 1111) == Approx(10.0));
  REQUIRE(atm.CheckBalance(22222222, 2222) == Approx(20.0));
}

TEST_CASE("CheckBalance: invalid credentials should throw invalid_argument", "[balance-bad-creds]") {
  // What this catches:
  // If CheckBalance returns 0 or garbage for unknown accounts instead of throwing.
  Atm atm;
  REQUIRE_THROWS_AS(atm.CheckBalance(99999999, 9999), std::invalid_argument);
}

TEST_CASE("WithdrawCash: negative amount should throw invalid_argument", "[withdraw-neg]") {
  // What this catches:
  // Missing validation: a negative withdraw could ADD money (security issue).
  Atm atm;
  atm.RegisterAccount(33333333, 3333, "NegWithdraw", 100.0);

  REQUIRE_THROWS_AS(atm.WithdrawCash(33333333, 3333, -1.0), std::invalid_argument);

  // Ensure balance didn't change after rejected operation
  REQUIRE(atm.CheckBalance(33333333, 3333) == Approx(100.0));
}

TEST_CASE("DepositCash: negative amount should throw invalid_argument", "[deposit-neg]") {
  // What this catches:
  // Missing validation: a negative deposit could subtract money or corrupt state.
  Atm atm;
  atm.RegisterAccount(44444444, 4444, "NegDeposit", 100.0);

  REQUIRE_THROWS_AS(atm.DepositCash(44444444, 4444, -50.0), std::invalid_argument);
  REQUIRE(atm.CheckBalance(44444444, 4444) == Approx(100.0));
}

TEST_CASE("WithdrawCash: overdraft should throw runtime_error and not change balance", "[withdraw-overdraft]") {
  // What this catches:
  // If they allow balance to go negative or throw wrong exception type,
  // or if they partially update balance before throwing.
  Atm atm;
  atm.RegisterAccount(55555555, 5555, "Overdraft", 25.00);

  REQUIRE_THROWS_AS(atm.WithdrawCash(55555555, 5555, 25.01), std::runtime_error);

  // Balance must remain unchanged after failed withdrawal
  REQUIRE(atm.CheckBalance(55555555, 5555) == Approx(25.00));
}

TEST_CASE("WithdrawCash: wrong credentials should throw invalid_argument", "[withdraw-bad-creds]") {
  // What this catches:
  // If WithdrawCash accidentally creates accounts, or uses operator[] which inserts,
  // or throws the wrong exception type.
  Atm atm;
  atm.RegisterAccount(66666666, 6666, "RealUser", 100.0);

  REQUIRE_THROWS_AS(atm.WithdrawCash(66666666, 9999, 1.0), std::invalid_argument);
  REQUIRE_THROWS_AS(atm.WithdrawCash(77777777, 6666, 1.0), std::invalid_argument);
}

TEST_CASE("DepositCash: wrong credentials should throw invalid_argument", "[deposit-bad-creds]") {
  // What this catches:
  // Same idea as withdraw: deposit should never create new account state.
  Atm atm;
  atm.RegisterAccount(77770000, 7000, "RealUser2", 100.0);

  REQUIRE_THROWS_AS(atm.DepositCash(77770000, 1234, 10.0), std::invalid_argument);
  REQUIRE_THROWS_AS(atm.DepositCash(12345670, 7000, 10.0), std::invalid_argument);
}

TEST_CASE("Transactions: Deposit/Withdraw should append a transaction record", "[tx-log]") {
  // What this catches:
  // If implementation updates the balance but forgets to log transactions,
  // or logs to wrong key.
  Atm atm;
  unsigned int card = 88888888;
  unsigned int pin = 8888;

  atm.RegisterAccount(card, pin, "TxLogger", 100.0);

  auto& tx = atm.GetTransactions();
  REQUIRE(tx.contains({card, pin}));
  REQUIRE(tx[{card, pin}].empty());

  atm.DepositCash(card, pin, 50.0);
  REQUIRE(tx[{card, pin}].size() == 1);

  atm.WithdrawCash(card, pin, 20.0);
  REQUIRE(tx[{card, pin}].size() == 2);

  // Loose string checks (don’t require exact formatting)
  REQUIRE((tx[{card, pin}][0].find("Deposit") != std::string::npos ||
           tx[{card, pin}][0].find("deposit") != std::string::npos));
  REQUIRE((tx[{card, pin}][1].find("Withdrawal") != std::string::npos ||
           tx[{card, pin}][1].find("withdrawal") != std::string::npos));
}

TEST_CASE("PrintLedger: invalid credentials should throw invalid_argument", "[ledger-bad-creds]") {
  // What this catches:
  // If PrintLedger writes garbage / creates file for non-existent account instead of throwing.
  Atm atm;
  REQUIRE_THROWS_AS(atm.PrintLedger("./should_not_exist.txt", 12121212, 3434), std::invalid_argument);
}

TEST_CASE("PrintLedger: output should include header info and transactions", "[ledger-content]") {
  // What this catches:
  // If PrintLedger forgets the header lines (Name/Card/PIN),
  // or fails to print transactions from the stored transaction vector.
  Atm atm;
  unsigned int card = 99990006;
  unsigned int pin = 6060;
  std::string owner = "LedgerUser";

  atm.RegisterAccount(card, pin, owner, 100.00);

  // Create real transactions via interface
  atm.DepositCash(card, pin, 400.0);      // should log deposit
  atm.WithdrawCash(card, pin, 200.40);    // should log withdrawal

  std::string out = "./ledger_out.txt";
  atm.PrintLedger(out, card, pin);

  // Read file contents
  std::ifstream ifs(out);
  REQUIRE(ifs.good());

  std::string content((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());

  REQUIRE(content.find("Name:") != std::string::npos);
  REQUIRE(content.find(owner) != std::string::npos);
  REQUIRE(content.find("Card Number:") != std::string::npos);
  REQUIRE(content.find(std::to_string(card)) != std::string::npos);
  REQUIRE(content.find("PIN:") != std::string::npos);
  REQUIRE(content.find(std::to_string(pin)) != std::string::npos);

  // Should include at least one deposit and one withdrawal line
  REQUIRE((content.find("Deposit") != std::string::npos ||
           content.find("deposit") != std::string::npos));
  REQUIRE((content.find("Withdrawal") != std::string::npos ||
           content.find("withdrawal") != std::string::npos));
}