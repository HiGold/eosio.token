/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>

#include <string>
#include <cmath>

using namespace eosio;
using std::string;

CONTRACT higoldtokens : public contract {
  public:
    using contract::contract;
    higoldtokens(eosio::name receiver, eosio::name code, datastream<const char*> ds):contract(receiver, code, ds) {}

    ACTION hi(name user);

    ACTION create( name   issuer,
                   asset  maximum_supply);

    ACTION issue( name to, asset quantity, string memo );

    ACTION retire( name owner, asset quantity, string memo );

    ACTION transfer( name    from,
                     name    to,
                     asset   quantity,
                    string  memo );

    ACTION open( name owner, const symbol& symbol, name ram_payer );

    ACTION startpos( asset  base_token,
                     asset  base_stake,
                     double weight,
                     string memo);

    ACTION pos( name owner, const symbol& symbol );

    ACTION buystake( name buyer, asset quantity );

    ACTION sellstake( name seller, asset stake );

    static asset get_supply( name token_contract_account, symbol_code sym_code )
    {
       stats statstable( token_contract_account, sym_code.raw() );
       const auto& st = statstable.get( sym_code.raw() );
       return st.supply;
    }

    static asset get_balance( name token_contract_account, name owner, symbol_code sym_code )
    {
       accounts accountstable( token_contract_account, owner.value );
       const auto& ac = accountstable.get( sym_code.raw() );
       return ac.balance;
    }

    static asset get_stakes( name token_contract_account, symbol_code sym_code )
    {
       sstats sstatstable( token_contract_account, sym_code.raw() );
       const auto& sst = sstatstable.get( sym_code.raw() );
       return sst.stakes;
    }

    static asset get_stakeprice( name token_contract_account, symbol_code sym_code )
    {
       sstats sstatstable( token_contract_account, sym_code.raw() );
       const auto& sst = sstatstable.get( sym_code.raw() );
       double P = sst.balance.amount / (sst.stakes.amount * (sst.weight / 1000));
       asset price;
       price.symbol = sst.balance.symbol;
       price.amount =int64_t(P);
       return price;
    }

  private:

    TABLE account {
       asset    balance;
       uint64_t primary_key()const { return balance.symbol.code().raw(); }
    };

    TABLE currency_stats {
       asset    supply;
       asset    max_supply;
       name     issuer;
       uint64_t primary_key()const { return supply.symbol.code().raw(); }
    };

    TABLE stake_account {
       asset    stake;
       uint32_t timestamp;
       uint64_t primary_key()const { return stake.symbol.code().raw(); }
    };

    TABLE stake_stats {
       asset    balance;
       asset    stakes;
       uint64_t weight;
       uint64_t primary_key()const { return balance.symbol.code().raw(); }
    };

    typedef eosio::multi_index< "accounts"_n, account > accounts;
    typedef eosio::multi_index< "stat"_n, currency_stats > stats;
    typedef eosio::multi_index< "stakeactns"_n, stake_account > stakeactns;
    typedef eosio::multi_index< "sstat"_n, stake_stats > sstats;

    ACTION ret_balance( name owner, name ram_payer, asset value );
    ACTION sub_balance( name owner, asset value );
    ACTION add_balance( name owner, asset value, name ram_payer );

    ACTION sub_stake( name owner, asset value );
    ACTION add_stake( name owner, asset value, name ram_payer );
};

EOSIO_DISPATCH(higoldtokens, (create)(issue)(transfer)(open)(retire)(startpos)(pos)(buystake)(sellstake))
