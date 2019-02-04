/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <eosio.token/eosio.token.hpp>

namespace eosio {
//
// Standard EOS Token
//
void token::create( name   issuer,
                    asset  maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}


void token::issue( name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must issue positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer );

    if( to != st.issuer ) {
      SEND_INLINE_ACTION( *this, transfer, { {st.issuer, "active"_n} },
                          { st.issuer, to, quantity, memo }
      );
    }
}

void token::retire( asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must retire positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    });

    sub_balance( st.issuer, quantity );
}

void token::transfer( name    from,
                      name    to,
                      asset   quantity,
                      string  memo )
{
    eosio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    eosio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable( _self, sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity );
    add_balance( to, quantity, payer );
}

void token::sub_balance( name owner, asset value )
{
   accounts from_acnts( _self, owner.value );

   const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
   eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );

   from_acnts.modify( from, owner, [&]( auto& a ) {
         a.balance -= value;
      });
}

void token::add_balance( name owner, asset value, name ram_payer )
{
   accounts to_acnts( _self, owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

void token::open( name owner, const symbol& symbol, name ram_payer )
{
   require_auth( ram_payer );

   auto sym_code_raw = symbol.code().raw();

   stats statstable( _self, sym_code_raw );
   const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
   eosio_assert( st.supply.symbol == symbol, "symbol precision mismatch" );

   accounts acnts( _self, owner.value );
   auto it = acnts.find( sym_code_raw );
   if( it == acnts.end() ) {
      acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = asset{0, symbol};
      });
   }
}
//
// Proof of Stake
//
void token::startpos( asset  base_token,
                      asset  base_stake,
                      double weight,
                      string memo)
{
  auto sym = base_token.symbol;
  eosio_assert( sym.is_valid(), "invalid symbol name" );

  stats statstable( _self, sym.code().raw() );
  auto existing = statstable.find( sym.code().raw() );
  eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
  const auto& st = *existing;

  require_auth( st.issuer );
  eosio_assert( base_token.is_valid(), "invalid base token quantity" );
  eosio_assert( base_token.amount > 0, "need positive quantity" );

  eosio_assert( base_token.symbol == st.supply.symbol, "symbol precision mismatch" );
  eosio_assert( base_token.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

  sstats sstatstable( _self, sym.code().raw() );
  auto sexisting = sstatstable.find( sym.code().raw() );
  eosio_assert( sexisting == sstatstable.end(), "token with symbol is already started POS" );
  eosio_assert( (weight <= 1000 && weight > 0), "invalid connector weight" );

  sstatstable.emplace( _self, [&]( auto& ss ) {
     ss.balance = base_token;
     ss.stakes  = base_stake;
     ss.weight  = weight;
  });

}

void token::pos(name owner, const symbol& symbol)
{
  auto sym = symbol;
  eosio_assert( sym.is_valid(), "invalid symbol name" );

  stats statstable( _self, sym.code().raw() );
  auto existing = statstable.find( sym.code().raw() );
  eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before POS" );
  const auto& st = *existing;

  sstats sstatstable( _self, sym.code().raw() );
  auto sexisting = sstatstable.find( sym.code().raw() );
  eosio_assert( sexisting != sstatstable.end(), "token with symbol can not POS" );

  require_auth( owner );
  add_stake( owner, asset{0, symbol}, owner );

  stakeactns pos_acnts( _self, owner.value );

  const auto& p = pos_acnts.get( sym.code().raw(), "no stake object found" );
  auto stake = p.stake.amount;
  auto age   = (now() - p.timestamp) / (24); // Set it as 3600*24 (One Day) on mainnet
  age = age > 365 ? 365:age;

  if (stake > 0 && age >= 1) {
    asset get;
    get.symbol = symbol;
    get.amount = int64_t(stake*age); // TODO : decreacing with the supply

    eosio_assert( get.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    pos_acnts.modify( p, owner, [&]( auto& a ) {
      a.timestamp = now();
    });

    statstable.modify( st, same_payer, [&]( auto& s ) {
      s.supply += get;
    });

    add_balance( owner, get, owner );
  };

}


void token::buystake( name buyer, asset quantity )
{
  require_auth( buyer );

  auto sym = quantity.symbol.code();
  stats statstable( _self, sym.raw() );
  const auto& st = statstable.get( sym.raw() );
  sstats sstatstable( _self, sym.raw() );
  const auto& sst = sstatstable.get( sym.raw() );

  require_recipient( buyer );

  eosio_assert( quantity.is_valid(), "invalid quantity" );
  eosio_assert( quantity.amount > 0, "need positive quantity" );
  eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
  eosio_assert( quantity.symbol == sst.balance.symbol, "symbol precision mismatch" );

  double R(sst.stakes.amount);
  double C(sst.balance.amount+quantity.amount);
  double F(sst.weight/1000.0);
  double T(quantity.amount);
  double ONE(1.0);
  double E = -R * (ONE - pow( ONE + T / C, F) );

  asset bought;
  bought.symbol = quantity.symbol;
  bought.amount = int64_t(E);

  eosio_assert( bought.amount > 0, "need a bigger quantity");

  pos( buyer, quantity.symbol );
  sub_balance( buyer, quantity );
  add_stake( buyer, bought, buyer );

  sstatstable.modify( sst, same_payer, [&]( auto& ss ) {
     ss.balance += quantity ;
     ss.stakes  += bought;
  });

}


void token::sellstake( name seller, asset stake )
{
  require_auth( seller );

  auto sym = stake.symbol.code();
  stats statstable( _self, sym.raw() );
  const auto& st = statstable.get( sym.raw() );
  sstats sstatstable( _self, sym.raw() );
  const auto& sst = sstatstable.get( sym.raw() );

  require_recipient( seller );

  eosio_assert( stake.is_valid(), "invalid quantity" );
  eosio_assert( stake.amount > 0, "need positive quantity" );
  eosio_assert( stake.symbol == st.supply.symbol, "symbol precision mismatch" );
  eosio_assert( stake.symbol == sst.balance.symbol, "symbol precision mismatch" );

  double R(sst.stakes.amount - stake.amount);
  double C(sst.balance.amount);
  double F(1000.0/sst.weight);
  double E(stake.amount);
  double ONE(1.0);
  double T = C * (pow( ONE + E/R, F) - ONE);

  asset get;
  get.symbol = stake.symbol;
  get.amount = int64_t(T);
  asset fee = asset{get.amount/100, get.symbol};

  eosio_assert( get.amount > 0, "need a bigger quantity");

  pos(seller, stake.symbol);
  sub_stake( seller, stake );
  add_balance( seller, (get-fee), seller );

  sstatstable.modify( sst, same_payer, [&]( auto& ss ) {
     ss.balance -= (get-fee);
     ss.stakes  -= stake;
  });

}

void token::sub_stake( name owner, asset value )
{
   stakeactns from_acnts( _self, owner.value );

   const auto& from = from_acnts.get( value.symbol.code().raw(), "no stake object found" );
   eosio_assert( from.stake.amount >= value.amount, "overdrawn stake" );

   from_acnts.modify( from, owner, [&]( auto& a ) {
         a.stake    -= value;
      });
}

void token::add_stake( name owner, asset value, name ram_payer )
{
   stakeactns to_acnts( _self, owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.stake     = value;
        a.timestamp = now();
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.stake    += value;
      });
   }
}

} /// namespace eosio

EOSIO_DISPATCH( eosio::token, (create)(issue)(transfer)(open)(retire)(startpos)(pos)(buystake)(sellstake) )
