#include <eosiolib/eosio.hpp>
#include "eosio.token.hpp"

using namespace eosio;

CONTRACT poleos : public eosio::contract {
  public:
      using contract::contract;
      // receiver = contract account name, code = ?? (maybe action name)
      // datastream = serialized version of action parameters
      poleos( name receiver, name code, datastream<const char*> ds )
         : contract(receiver, code, ds), testv(receiver, receiver.value), testp(receiver, receiver.value) {}
      // copied from eosio.token
     struct [[eosio::table]] account {
	asset    balance;

	uint64_t primary_key()const { return balance.symbol.code().raw(); }
     };

      struct [[eosio::table]] currency_stats {
	asset    supply;
	asset    max_supply;
	name     issuer;

	uint64_t primary_key()const { return supply.symbol.code().raw(); }
      };

      typedef eosio::multi_index< "accounts"_n, account > accounts;
      typedef eosio::multi_index< "stat"_n, currency_stats > stats;
      
      void add_balance( name owner, asset value, name ram_payer )
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


      void sub_balance( name owner, asset value ) {
	 accounts from_acnts( _self, owner.value );

	 const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );

	 from_acnts.modify( from, owner, [&]( auto& a ) {
	       a.balance -= value;
	    });
      }

      void transfer(  name    from,
                      name    to,
                      asset   quantity,
                      string  memo )
      {
	  auto sym = quantity.symbol.code();
	  stats statstable( _self, sym.raw() );
	  const auto& st = statstable.get( sym.raw() );

	  auto payer = has_auth( to ) ? to : from;

	  sub_balance( from, quantity );
	  add_balance( to, quantity, payer );
      }
      // done copying



      ACTION vote( name user, uint64_t poll, uint64_t outcome, double stake  ) {
	print_f( "Vote % from poleos_vote", user );
	print_f( "The poll % from poleos_vote", poll );
	print_f( "outcome % from poleos_vote", outcome );
	print_f( "stake % from poleos_vote", stake );

	testv.emplace( _self, [&]( auto& v ) {
	    v.key = testv.available_primary_key();

	    v.user = user;
	    v.poll = poll;
	    v.outcome = outcome;
	    v.stake = stake;
        });

	auto itr = testp.find(poll);
	if ( itr == testp.end() ) {
	  testp.emplace(_self, [&]( auto& v) {
	      v.poll = poll;
	      v.outcome = outcome;
	      v.total_stake = stake;
	  });
	} else {
	    eosio_assert( itr != testp.end(), "test table not set" );
	    testp.modify(itr, _self, [&]( auto& row) {
	      row.total_stake += stake;	
	    });
	}
      }
      
      ACTION deletedata() {
	print( "Deleting data!!" );
	auto itr = testv.begin();
        while (itr != testv.end()) {
	    print_f( "deleting vote with stake %", itr->stake );
            itr = testv.erase(itr);
        }
      }

      ACTION getvotes( uint64_t poll ) {
	 print_f( "The poll % from poleos_vote", poll );
	 
	 auto idx = testv.get_index<"poll"_n>();
         for ( auto itr = idx.begin(); itr != idx.end(); itr++ ) {
	  if (itr->poll == poll) {
	    print_f( "Vote % from poleos_vote \n", itr->user );
	  }
         }
      }

      ACTION payout( uint64_t poll, uint64_t winning_outcome ) {
	 print_f( "Paying out poll %", poll );

	 // get total payout
	auto itr = testp.find(poll);
	eosio_assert( itr != testp.end(), "test table not set" );
	double total_stake = itr -> total_stake;
	 
	auto idx = testv.get_index<"outcome"_n>();
	for ( auto itr = idx.begin(); itr != idx.end(); itr++ ) {
	  if (itr->outcome == winning_outcome && itr->poll == poll) {
	    print_f( "Paying out % amount of % \n", itr->user, total_stake );
	    transfer("poleosaccont"_n, itr->user, asset(1, symbol("EOS", 1)), "winnigns");
	  }
	}
      }

      // accessor for external contracts to easily send inline actions to your contract
      using vote_action = action_wrapper<"vote"_n, &poleos::vote>;
      using get_vote_action = action_wrapper<"getvotes"_n, &poleos::getvotes>;

      TABLE test_vote {
         name user;
	 uint64_t key;
         
	 uint64_t poll;
	 uint64_t outcome;
	 double stake;
         
	 uint64_t by_poll() const { return poll; }
	 uint64_t by_outcome() const { return outcome; }
	 uint64_t by_user() const { return user.value; }
        
	 auto primary_key() const { return key; }
      };

      TABLE test_payout {
	 uint64_t poll;
	 uint64_t outcome;
	 double total_stake;
         
	 auto primary_key() const { return poll; }
      };

      typedef eosio::multi_index<"testvote"_n, test_vote, 
	      eosio::indexed_by<"poll"_n, eosio::const_mem_fun<test_vote, uint64_t, &test_vote::by_poll>>, 
	      eosio::indexed_by<"outcome"_n, eosio::const_mem_fun<test_vote, uint64_t, &test_vote::by_outcome>>,
	      eosio::indexed_by<"user"_n, eosio::const_mem_fun<test_vote, uint64_t, &test_vote::by_user>>> test_votes;


      typedef eosio::multi_index<"testpayout"_n, test_payout> test_payouts;


  private:
      test_votes testv;
      test_payouts testp;
};

EOSIO_DISPATCH( poleos, (vote) (getvotes) (deletedata) (payout))
