#ifndef SPARSE_PATCHMAP_H
#define SPARSE_PATCHMAP_H

#include <algorithm>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <boost/container/allocator.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <typeinfo>
#include <exception>
#include <memory>
#include "wmath_forward.hpp"
#include "wmath_bits.hpp"
#include "wmath_hash.hpp"
#include "wmath_math.hpp"
#include "hashed_array_tree.hpp"
#ifdef PATCHMAP_STAT
size_t recursion_depth;
size_t shift_count;
#endif

namespace{
  template<class T>
  double frac(const T& n){
    return n*pow(0.5,std::numeric_limits<T>::digits);
  }
}

namespace wmath{
  
  using std::allocator_traits;

  template<typename T>
  struct dummy_comp{ // dummy comparator for when we don't need a comparator
    constexpr bool operator()(const T&,const T&) const {return false;}
  };

  struct empty{
  };
  
  template<class key_type    = int,  // int is the default, why not
           class mapped_type = int,  // int is the default, why not
           class hash        = hash_functor<key_type>,
           class equal       = std::equal_to<key_type>,
           class comp        = typename conditional<is_injective<hash>::value,
                                                    dummy_comp<key_type>,
                                                    std::less<key_type>>::type,
           class alloc       = typename std::allocator<
             typename conditional<
               std::is_same<mapped_type,void>::value,
               std::pair<key_type,empty>,
               std::pair<key_type,mapped_type>
             >::type>,
           /*class alloc       = typename boost::container::allocator<
             typename conditional<
               std::is_same<mapped_type,void>::value,
               std::pair<key_type,empty>,
               std::pair<key_type,mapped_type>
             >::type,2>,*/
           bool dynamic      = true
          >
  class sparse_patchmap{
    public:
      typedef alloc allocator_type;
      typedef typename alloc::value_type value_type;
      typedef typename alloc::pointer value_pointer;
      typedef typename alloc::reference reference;
      typedef typename alloc::const_reference const_reference;
      typedef typename alloc::difference_type difference_type;
      typedef typename alloc::size_type size_type;
      typedef typename std::result_of<hash(key_type)>::type hash_type;
      typedef typename conditional<is_same<mapped_type,void>::value,
                                  key_type,
                                  mapped_type>::type
                                  _mapped_type;
    private:
      size_type num_data = 0;
      size_type datasize = 0;
      size_type masksize = 0;
      allocator_type allocator;
      boost::container::allocator<size_type,2> maskallocator;
      hashed_array_tree<value_type,alloc> data;
      hashed_array_tree< size_type,boost::container::allocator<size_type,2>>
        mask;
      comp  comparator;
      equal equator;
      hash  hasher;
      using uphold_iterator_validity = true_type;
      /* TODO
      size_type const inline masksize() const {
        return (datasize+digits<size_type>()-1)/digits<size_type>();
      }
      */
      template<typename T>
      const key_type& key_of(T&& value) const {
        if constexpr (is_same<void,mapped_type>::value) return value;
        else return value.first;
      }
      size_type inline map(
          const hash_type& h,
          const hash_type& n
          ) const {
        return get<0>(long_mul(h,n));
      }
      size_type inline map(const hash_type& h) const {
        return map(h,datasize);
      }
      size_type inline map_diff(
          const hash_type& h0,
          const hash_type& h1,
          const hash_type& n
          ) const {
        const auto lm = long_mul(h0-h1,n);
        return get<0>(lm);
      }
      size_type inline map_diff(
          const hash_type& h0,
          const hash_type& h1
          ) const {
        return map_diff(h0,h1,datasize);
      }
      size_type inline map_diff_round(
          const hash_type& h0,
          const hash_type& h1,
          const hash_type& n
          ) const {
        const auto lm = long_mul(h0-h1,n);
        return get<0>(lm)+(get<1>(lm)>((~hash_type(0))>>1));
      }
      size_type inline map_diff_round(
          const hash_type& h0,
          const hash_type& h1
          ) const {
        return map_diff_round(h0,h1,datasize);
      }
      hash_type inline order(const key_type& k) const {
        return distribute(hasher(k));
      }
      bool inline is_less(
          const key_type& a,
          const key_type& b,
          const hash_type& oa,
          const hash_type& ob
          ) const {
        if constexpr (is_injective<hash>::value){
          assert(equator(a,b)==(oa==ob));
          if (oa<ob) return true;
          else       return false;
        } else {
          if (oa<ob) return true;
          if (oa>ob) return false;
          return comparator(a,b);
        }
      }
      bool inline is_less(
          const key_type& a,
          const key_type& b,
          const hash_type& oa
          ) const {
        return is_less(a,b,oa,order(b));
      }
      bool inline is_less(const key_type& a,const key_type& b) const {
        return is_less(a,b,order(a),order(b));
      }
      bool inline is_more(
          const key_type& a,
          const key_type& b,
          const hash_type& oa,
          const hash_type& ob
          ) const {
        if constexpr (is_injective<hash>::value){
          assert(equator(a,b)==(oa==ob));
          if (oa>ob) return true;
          else       return false;
        } else {
          if (oa>ob) return true;
          if (oa<ob) return false;
          return !((comparator(a,b))||(equator(a,b)));
        }
      }
      bool inline is_more(
          const key_type& a,
          const key_type& b,
          const hash_type& oa
          ) const {
        return is_more(a,b,oa,order(b));
      }
      bool inline is_more(
          const key_type& a,
          const key_type& b
          ) const {
        return is_more(a,b,order(a),order(b));
      }
      bool inline is_set(const size_type& n) const {
        const size_type i = n/digits<size_type>();
        const size_type j = n%digits<size_type>();
        assert(i<masksize);
        return (mask[i]&(size_type(1)<<(digits<size_type>()-j-1)));
      }
      bool inline is_set_any(
          const size_type& lo,
          const size_type& hi) const {
        const size_type k0 = lo/digits<size_type>();
        const size_type l0 = lo%digits<size_type>();
        const size_type m0 = (~size_type(0))>>l0;
        const size_type k1 = hi/digits<size_type>();
        const size_type l1 = hi%digits<size_type>();
        const size_type m1 = (~size_type(0))<<(digits<size_type>()-l1-1);
        if (k0==k1) return ((m0&m1&mask[k0])!=0);
        if (((m0&mask[k0])!=0)||((m1&mask[k1])!=0)) return true;
        for (size_type i = k0+1;i!=k1;++i)
          if (mask[i]!=0) return true;
        return false;
      }
      void inline set(const size_type& n) {
        const size_type i = n/digits<size_type>();
        const size_type j = n%digits<size_type>();
        mask[i]|=size_type(1)<<(digits<size_type>()-j-1);
      }
      void inline unset(const size_type& n) {
        const size_type i = n/digits<size_type>();
        const size_type j = n%digits<size_type>();
        mask[i]&=((~size_type(0))^(size_type(1)<<(digits<size_type>()-j-1)));
      }
      void inline swap_set(const size_type& i,const size_type& j){
        if (is_set(i)==is_set(j)) return;
        if (is_set(i)){
          set(j);
          unset(i);
        }else{
          set(i);
          unset(j);
        }
      }
      bool inline index_key_is_less(const size_type& i,const key_type& k) const{
        if (is_set(i)) return is_less(data[i].first,k);
        return i<map(order(k));
      }
      bool inline key_index_is_less(const key_type& k,const size_type& i) const{
        if (is_set(i)) return is_less(k,data[i].first);
        return map(order(k))<i;
      }
      bool inline index_index_is_less(const size_type& i,const size_type& j)
        const {
        assert(i<datasize);
        assert(j<datasize);
        if (is_set(i)&&is_set(j)) return is_less(data[i].first,data[j].first);
        if (is_set(i)) return map(order(data[i].first))<j;
        if (is_set(j)) return i<map(order(data[j].first));
        return i<j;
      }
      bool inline index_index_is_more(const size_type& i,const size_type& j)
        const {
        return index_index_is_less(j,i);
      }
      size_type inline find_first() const {
        size_type i=0;
        if (i>=datasize) return ~size_type(0);
        while(true){
          const size_type k = i/digits<size_type>();
          const size_type l = i%digits<size_type>();
          const size_type m = (~size_type(0))>>l; 
          assert(k<masksize);
          size_type p = (mask[k]&m)<<l;
          if (k+1<masksize)
            p|=shr(mask[k+1]&(~m),digits<size_type>()-l);
          const size_type s = clz(p);
          if (s==0) return i;
          i+=s;
          if (i>=datasize) return ~size_type(0);
        }
      }
      // search for free bucket in decreasing order
      size_type inline search_free_dec(size_type i) const {
        while(true){
          const size_type k = i/digits<size_type>();
          const size_type l = i%digits<size_type>();
          const size_type m = (~size_type(0))<<(digits<size_type>()-l-1);
          assert(k<masksize);
                size_type p = ((~(mask[k]&m))>>(digits<size_type>()-l-1));
          if (k!=0) p|=shl(~(mask[k-1]&(~m)),l+1);
          const size_type s = ctz(p);
          if (s==0){
            assert(!is_set(i));
            return i;
          }
          i-=s;
          if (i>=datasize) return ~size_type(0);
        }
      }
      // search for free bucket in increasing order
      size_type inline search_free_inc(size_type i) const {
        while(true){
          const size_type k = i/digits<size_type>();
          const size_type l = i%digits<size_type>();
          const size_type m = (~size_type(0))>>l; 
          assert(k<masksize);
                size_type p = (~(mask[k]&m))<<l;
          if (k+1<masksize) p|=shr(~(mask[k+1]&(~m)),digits<size_type>()-l);
          const size_type s = clz(p);
          if (s==0){
            assert(!is_set(i));
            return i;
          }
          i+=s;
          if (i>=datasize) return ~size_type(0);
        }
      }
      // search for free bucket bidirectional
      size_type inline search_free_bidir_v0(size_type i) const {
        const size_type k = search_free_inc(i);
        const size_type l = search_free_dec(i);
        assert((k<datasize)||(l<datasize));
        if (k>=datasize) i=l;
        else if (l>=datasize) i=k;
        else if (k-i<i-l) i=k;
        else i=l;
        return i;
      }
      // search for free bucket truly bidirectional
      // this is optimal vor very high load factors > 0.98
      size_type inline search_free_bidir(const size_type& n) const {
        size_type i = n, j = n, si=~size_type(0),sj=~size_type(0);
        while(true){
          if ((i!=~size_type(0))&&si){
            const size_type k = i/digits<size_type>();
            const size_type l = i%digits<size_type>();
            const size_type m = (~size_type(0))>>l; 
                  size_type p = (~(mask[k]&m))<<l;
            if (k+1<masksize) p|=shr(~(mask[k+1]&(~m)),digits<size_type>()-l);
                            si= clz(p);
          }
          if (si==0){
            if (j==~size_type(0)) return i;
            if (i-n+digits<size_type>()<=n-j) return i;
          } else {
            i+=si;
            if (i>=datasize) i=~size_type(0);
            if ((i&j)==~size_type(0)) return ~size_type(0);
          }
          if ((j!=~size_type(0))&&sj){
            const size_type k = j/digits<size_type>();
            const size_type l = j%digits<size_type>();
            const size_type m = (~size_type(0))<<(digits<size_type>()-l-1);
                  size_type p = ((~(mask[k]&m))>>(digits<size_type>()-l-1));
            if (k!=0)       p|=shl(~(mask[k-1]&(~m)),l+1);
                            sj= ctz(p);
          }
          if (sj==0) {
            if (i==~size_type(0)) return j;
            if (n-j+digits<size_type>()<=i-n) return j;
          } else {
            j-=sj;
            if (j>=datasize) j=~size_type(0);
            if ((i&j)==~size_type(0)) return ~size_type(0);
          }
          if ((si==0)&&(sj==0)){
            if (i-n<=n-j) return i;
            else          return j;
          }
        }
        return ~size_type(0);
      }
      size_type const inline reserve_node(
          const  key_type& key,
          const size_type& mok,
          const hash_type& ok
          ){
#ifdef PATCHMAP_STAT
        shift_count = 0;
#endif
        assert(mok<datasize);
        assert(map(order(key))==mok);
        if (!is_set(mok)) {
          set(mok);
          ++num_data;
          return mok;
        }
        const size_type j = search_free_bidir_v0(mok);
        assert(j<datasize);
        assert(!is_set(j));
        set(j);
        //data[i].first = key;
        ++num_data;
        size_type i = j;
        while(true){
          if (i==0) break;
          if (!is_set(i-1)) break;
          if (is_less(data[i-1].first,key,order(data[i-1].first),ok)) break; 
          swap(data[i],data[i-1]);
          --i;
        }
#ifdef PATCHMAP_STAT
        shift_count = j-i;
#endif
        if (i!=j) return i;
        while(true){
          if (i+1>=datasize) break;
          if (!is_set(i+1)) break;
          if (is_less(key,data[i+1].first,ok,order(data[i+1].first))) break; 
          swap(data[i],data[i+1]);
          ++i;
        }
#ifdef PATCHMAP_STAT
        shift_count = i-j;
#endif
        return i;
      }
      size_type inline reserve_node(
          const key_type& key,
          const size_type& hint){
        const hash_type ok = order(key);
        return reserve_node(key,hint,ok);
      }
      size_type inline reserve_node(const key_type& key){
        const hash_type ok = order(key);
        const size_type hint = map(ok);
        assert(hint<datasize);
        return reserve_node(key,hint,ok);
      }
      size_type inline find_node_binary(
          const key_type& key,
          const hash_type& ok,
          const size_type& lo, // inclusive bounds
          const size_type& hi  // inclusive bounds
          ) const {
#ifdef PATCHMAP_STAT
        ++recursion_depth;
#endif
        assert(lo<datasize);
        assert(hi<datasize);
        assert(lo<=hi);
        const size_type  mi = (hi+lo)/2;
        if (is_set(mi)) if (equator(data[mi].first,key)) return mi;
        if constexpr (is_injective<hash>::value) {
          if (index_key_is_less(mi,key)){
            if (mi<hi) return find_node_binary(key,ok,mi+1,hi);
            else return ~size_type(0);
          } else {
            if (mi>lo) return find_node_binary(key,ok,lo,mi-1);
            else return ~size_type(0);
          }
        } else {
          if (index_key_is_less(mi,key)){
            if (mi<hi) return find_node_binary(key,ok,mi+1,hi);
            else return ~size_type(0);
          }
          if (key_index_is_less(key,mi)){
            if (mi>lo) return find_node_binary(key,ok,lo,mi-1);
            else return ~size_type(0);
          }
        }
      }
      
      size_type inline interpol(
          const hash_type& ok,
          const hash_type& olo,
          const hash_type& ohi,
          const size_type& lo,
          const size_type& hi
          ) const {
        auto lm             = long_mul(size_type(ok-olo),hi-lo);
        // this is theoretically better but not worth the time
        //const hash_type tmp = get<1>(lm)+(ohi-olo)/2;
        //if (tmp<get<1>(lm)) ++get<0>(lm);
        //get<1>(lm)          = tmp;
        const size_type n   = clz(get<0>(lm));
        const size_type m   = digits<hash_type>()-n;
        const hash_type den = (ohi-olo)>>m;
        const hash_type nom = (get<0>(lm)<<n)+(get<1>(lm)>>m);
        return lo+nom/den;
      }

      size_type inline find_node_linear(
          const key_type& k,
          const size_type& lo,
          const size_type& hi) const {
        size_type i = lo;
        while(true){
          if (is_set(i)) if (equator(data[i].first,k)) return i;
          if (i==hi) return ~size_type(0);
          ++i;
        }
      }
      
      size_type inline find_node_interpol(
        const  key_type&   k,
        const hash_type&  ok,
        const size_type& mok,
              size_type   lo,
              hash_type  olo,
              bool is_set_lo,
              size_type   hi,
              size_type  ohi,
              bool is_set_hi
          ) const {
        assert(lo<=hi||datasize==0);
        size_type mi;
        while(true) {
          if (!(lo<hi)) return ~size_type(0);
#ifdef PATCHMAP_STAT
          ++recursion_depth;
#endif
          if (hi-lo<2) {
            if (is_set_lo&&is_set_hi) return ~size_type(0);
            if (is_set(lo)) if (equator(k,data[lo].first)) return lo;
            if (is_set(hi)) if (equator(k,data[hi].first)) return hi;
            return ~size_type(0);
          }
          if (hi-lo<8) {
            if (is_set_hi && is_set_lo) {
              mi = lo + ((hi-lo)>>1);
            } else if (is_set_lo) {
              mi = lo + ((hi-lo+2)>>2);
            } else if (is_set_hi) {
              mi = hi - ((hi-lo+2)>>2);
            } else {
              return ~size_type(0);
            }
          } else {
            if (is_set_hi && is_set_lo) {
              mi = interpol(ok,olo,ohi,lo,hi);
            } else if (is_set_lo) {
              const size_type st = map_diff(ok,olo);
              mi = lo+st<hi?lo+st:hi;
            } else if (is_set_hi) {
              const size_type st = map_diff(ohi,ok);
              mi = lo+st<hi?hi-st:lo;
            } else {
              return ~size_type(0);
            }
            mi = clip(mi,lo+1,hi-1);
          }
          if (!is_set(mi)) {
            if (mi<mok) {
              lo = mi;
              is_set_lo=false;
              continue;
            }
            if (mi>mok) {
              hi = mi;
              is_set_hi=false;
              continue;
            }
            return ~size_type(0);
          }
          if (equator(k,data[mi].first)) return mi;
          const hash_type omi = order(data[mi].first);
          if (ok<omi) {
            hi = mi;
            ohi = omi;
            is_set_hi = true;
            continue;
          }
          if (ok>omi) {
            lo = mi;
            olo = omi;
            is_set_lo = true;
            continue;
          }
          if constexpr (is_injective<hash>::value) {
            return ~size_type(0);
          } else {
            if (k<data[mi].first) {
              hi = mi;
              ohi = omi;
              is_set_hi = true;
              continue;
            }
            if (k>data[mi].first) {
              lo = mi;
              olo = omi;
              is_set_lo = true;
              continue;
            }
          }
          return ~size_type(0);
        }
        return ~size_t(0);
      }

      size_type inline find_node(
          const key_type &  k,
          const hash_type& ok,
          const size_type& mok)
        const {
#ifdef PATCHMAP_STAT
        recursion_depth=0;
#endif
        assert((mok<datasize)||(datasize==0));
        if (datasize==0) return ~size_type(0);
        if (!is_set(mok)) return ~size_type(0);
        if (equator(data[mok].first,k)) return mok;
        const hash_type omi = order(data[mok].first);
        if (omi<ok) {
          return find_node_interpol(k,ok,mok,
              mok       ,omi          ,true ,
              datasize-1,~size_type(0),false);
        } else {
          return find_node_interpol(k,ok,mok,
              0         ,0            ,false,
              mok       ,          omi,true );
        }
      }
      
      size_type const inline find_node(
          const  key_type&  k,
          const size_type& ok
          ) const { return find_node(k,ok,map(ok)); }
      
      size_type const inline find_node(const key_type& k)
      const { return find_node(k,order(k)); }

      size_type const inline find_node_bruteforce(const key_type& k) const {
        for (size_type i = 0; i!=datasize; ++i)
          if (is_set(i)) if (equator(data[i].first,k)) return i;
        return ~size_type(0);
      }

      void inline const restore_order() { // insertion sort
        for (size_type i=0;i!=datasize;++i){
          for(size_type j=i;j!=0;--j){
            if (index_index_is_less(j-1,j)) break;
            swap_set(j,j-1);
            swap(data[j],data[j-1]);
          }
        }
      }
      template<typename map_type>
      typename conditional<is_const<map_type>::value,
        const _mapped_type&,
              _mapped_type&>::type
      static inline const_noconst_at(map_type& hashmap,const key_type& k) {
        size_type i = hashmap.find_node(k);
        if (i<hashmap.datasize){
          assert(hashmap.is_set(i));
          if constexpr (is_same<mapped_type,void>::value)
            return hashmap.data[i].first;
          else return
            hashmap.data[i].second;
        } else throw std::out_of_range(
            std::string(typeid(hashmap).name())
            +".const_noconst_at("+typeid(k).name()+" k)"
            +"key not found, array index "
            +to_string(i)+" out of bounds"
           );
      }
    public:
      void print() const {
        cerr << datasize << " " << num_data << endl;
        for (size_type i=0;i!=datasize;++i) {
          cout << std::fixed << std::setprecision(16);
          const size_type  ok = order(data[i].first);
          const size_type mok = map(ok);
          if (is_set(i)) cout << setw(6) << i;
          else           cout << "      "    ;
                         cout << setw(20) << frac(uint32_t(ok))
                              << setw(20) << frac(uint32_t(data[i].second));
          if (is_set(i)) cout << setw( 8) << mok
                              << setw( 8) << int(mok)-int(i);
          else           cout << setw( 8) << i
                              << setw( 8) << 0;
          cout << endl;
        }
        cout << endl;
      }
      size_type erase(
          const  key_type&  k,
          const hash_type& ok,
          const size_type& hint){
        size_type i = find_node(k,ok,hint);
        if (i>=datasize) return 0;
        //cout << "erasing " << wmath::frac(ok) << endl;
        //cout << "found at " << i << endl;
        const size_type j = i;
        while(true){
          if (i+1==datasize) break;
          if (!is_set(i+1)) break;
          if (map(order(data[i+1].first))>i) break;
          swap(data[i],data[i+1]);
          ++i;
        }
        if (i==j){
          while(true){
            if (i==0) break;
            if (!is_set(i-1)) break;
            if (map(order(data[i-1].first))<i) break;
            swap(data[i],data[i-1]);
            --i;
          }
        }
        unset(i);
        //cout << "unset position " << i << endl;
        //cout << k << " " << data[i].first << endl;
        --num_data;
        //check_ordering();
        //cout << num_data << endl;
        assert(num_data<datasize);
        return 1;
      }
      size_type erase(
          const  key_type&  k,
          const size_type& ok
          ){
        const hash_type hint = map(ok);
        return erase(k,ok,hint);
      }
      size_type erase(const key_type& k){
        const size_type ok = order(k);
        return erase(k,ok);
      }
      void inline clear(){
        for (size_type i=0;i!=masksize;++i) mask[i]=0;
        num_data=0;
      }
      void const resize(const size_type& n){
        //cout << "resizing patchmap " << num_data << endl;
        if (n <num_data) return resize(num_data);
        if (n==datasize) return;
        const size_type new_datasize = n;
        const size_type new_masksize =
          (new_datasize+digits<size_type>()-1)/digits<size_type>();
        if (n>datasize) {
          data.resize(new_datasize);
          mask.resize(new_masksize);
          const size_type old_masksize = masksize;
          masksize = new_masksize;
          const size_type old_datasize = datasize;
          datasize = new_datasize;
          for (size_type i=old_masksize;i!=new_masksize;++i) mask[i]=0;
          num_data = 0;
          for (size_type n=old_datasize;n!=~size_type(0);--n) {
            const size_type i = n/digits<size_type>();
            const size_type j = n%digits<size_type>();
            if (mask[i]&(size_type(1)<<(digits<size_type>()-j-1))) {
              const value_type tmp = move(data[n]);
              unset(n);
              const size_type l = reserve_node(tmp.first);
              data[l] = move(tmp);
              set(l);
            }
          }
        } else {
          const size_type old_datasize = datasize;
          datasize = new_datasize;
          masksize = new_masksize;
          num_data = 0;
          for (size_type n=0;n!=old_datasize;++n) {
            const size_type i = n/digits<size_type>();
            const size_type j = n%digits<size_type>();
            if (mask[i]&(size_type(1)<<(digits<size_type>()-j-1))) {
              const value_type tmp = move(data[n]);
              unset(n);
              const size_type l = reserve_node(tmp.first);
              data[l] = move(tmp);
              set(l);
            }
          }
          data.resize(datasize);
          mask.resize(masksize);
        }
      }
      size_type inline size() const { return num_data; }
      size_type const test_size() const {
        size_type test = 0;
        for (size_type i=0;i!=datasize;++i) test += is_set(i);
        return test;
      }
      void test_chunks() const {
        for (size_type i=0;i!=masksize;++i){
          cout << popcount(mask[i]) << endl;
        }
      }
      bool check_ordering() const {
        bool ordered = true;
        for (size_type i=0,j=1;j<datasize;(++i,++j)){
          if (!index_index_is_less(j,i)) continue;
          cout << std::fixed << std::setprecision(16)
               << is_set(i) << " " << is_set(j) << " "
               << i << " " << j << " "
               << data[i].first << " " << data[j].first << " "
               << order(data[i].first) << " " << order(data[j].first) << endl;
          //cout << index(i) << " " << index(j) << endl;
          /*cout << double(index(i))
            /pow(2.0,double(CHAR_BIT*sizeof(hash_type))) << " "
               << double(index(j))
            /pow(2.0,double(CHAR_BIT*sizeof(hash_type))) << endl;*/
          ordered = false;
        }
        return ordered;
      }
      bool check_ordering(const size_type& i) const {
        if (  i>0       ) if (!index_index_is_less(i-1,i)) return false;
        if (i+1<datasize) if (!index_index_is_less(i,i+1)) return false;
        return true;
      }
      void inline ensure_size(){
        if constexpr (!dynamic) return;
        if (num_data*32<datasize*31) return;
        //const size_type l2 = log2(datasize+1);
        //if ( (128*31+l2*l2*32)*num_data < (128+l2*l2)*31*datasize ) return;
        //if ( (128*15+l2*l2*16)*num_data < (128+l2*l2)*15*datasize ) return;
        //if ( (128*7+l2*l2*8)*num_data < (128+l2*l2)*7*datasize ) return;
        size_type nextsize;
        if (datasize == 0) {
          nextsize = digits<size_type>();
        } else {
          //nextsize = 50*datasize/31;
          //nextsize = 48*datasize/31;
          //nextsize = 47*datasize/31;
          //nextsize = 47*datasize/37;
          //nextsize = 53*datasize/41;
          //nextsize = (113*datasize+44)/89;
          nextsize = (107*datasize+89)/89;
          nextsize = (nextsize+digits<size_type>()-1)/digits<size_type>();
          nextsize = mask.next_size(nextsize);
          nextsize*= digits<size_type>();
        }
        resize(nextsize);
      }
      _mapped_type& operator[](const key_type& k){
        const size_type i = find_node(k);
        if (i<datasize) {
          if constexpr (is_same<mapped_type,void>::value) return data[i].first;
          else return data[i].second;
        }
        //assert(find_node_bruteforce(k)==~size_type(0));
        ensure_size();
        //assert(check_ordering());
        const size_type j = reserve_node(k);
        if constexpr (is_same<void,mapped_type>::value) {
          allocator_traits<alloc>::construct(
              allocator,&data[j],k,wmath::empty());
        } else {
          allocator_traits<alloc>::construct(
              allocator,&data[j],k,mapped_type());
        }
        //assert(check_ordering());
        //assert(find_node_bruteforce(k)==j);
        //assert(find_node(k)==j);
        assert(check_ordering(j));
        if constexpr (is_same<mapped_type,void>::value) return data[i].first;
        else return data[j].second;
      }
      const _mapped_type& operator[](const key_type& k) const {
        const size_type i = find_node(k);
        assert(i<datasize);
        if constexpr (is_same<void,mapped_type>::value) return data[i].first;
        return data[i].second; // this is only valid if key exists!
      }
      _mapped_type& at(const key_type& k){
        return const_noconst_at(*this,k);
      }
      const _mapped_type& at(const key_type& k) const {
        return const_noconst_at(*this,k);
      }
      size_type const inline count(const key_type& k) const {
        //assert(check_ordering());
        //assert(find_node(k)==find_node_bruteforce(k));
        return (find_node(k)<datasize);
      }
      double average_offset(){
        double v = 0;
        for (size_type i=0;i!=datasize;++i){
          if (is_set(i)){
            v+=double(map(data[i].first))-double(i);
            cout << map(order(data[i].first)) << " " << i << " "
                 << datasize << endl;
          }
        }
        return v/size()/datasize;
      }
      void print_offsets(){
        for (size_type i=0;i!=datasize;++i){
          if (is_set(i)) cout << map(order(data[i].first)) << " " << i << endl;
          //else           cout << i                         << " " << i << endl;
        }
      }
      void print_offsethist(){
        sparse_patchmap<int,size_t> hist;
        for (size_type i=0;i!=datasize;++i)
          ++hist[int(map(order(data[i].first)))-int(i)];
        for (auto it=hist.begin();it!=hist.end();++it)
          cout << it->first << " " << it->second << endl;
      }
      equal key_eq() const{ // get key equivalence predicate
        return equal{};
      }
      comp key_comp() const{ // get key order predicate
        return comp{};
      }
      alloc get_allocator() const{
        return allocator;
      }
      hash hash_function() const{ // get hash function
        return hash{};
      }  
      template<bool is_const>
      class const_noconst_iterator {
        friend class sparse_patchmap;
        public:
          size_type hint;
          key_type key;
          typename conditional<is_const,
                               const sparse_patchmap*,
                               sparse_patchmap*
                              >::type map;
        private:
          void inline update_hint(){
            if constexpr (!uphold_iterator_validity::value) return;
            if (hint<map->datasize)
              if (equal{}(map->data[hint].first,key)) return;
            hint = map->find_node(key,hint);
            if (hint>=map->datasize) hint = ~size_type(0);
          }
          void inline unsafe_increment(){ // assuming hint is valid
            //cout << "unsafe_increment() " << hint << " " << key << endl;
            if (++hint>=map->datasize){
              //cout << "test1" << endl;
              //cout << "becoming an end()" << endl;
              hint=~size_type(0);
              return;
            }
            while(true){
              //cout << "test2" << endl;
              const size_type k = hint/digits<size_type>();
              const size_type l = hint%digits<size_type>();
              const size_type m = (~size_type(0))>>l; 
              assert(k<map->masksize);
              size_type p = (map->mask[k]&m)<<l;
              if (k+1<map->masksize)
                p|=shr(map->mask[k+1]&(~m),digits<size_type>()-l);
              const size_type s = clz(p);
              if (s==0) break;
              hint+=s;
              //cout << hint << " " << s << endl;
              if (hint>=map->datasize){
                //cout << "test3" << endl;
                //cout << "becoming an end()" << endl;
                hint=~size_type(0);
                return;
              }
            }
            //cout << "test4" << endl;
            //cout << "new hint=" << hint << endl;
            key = map->data[hint].first;
            //cout << "new key=" << key << endl;
          }
          void inline unsafe_decrement(){ // assuming hint is valid
            if (--hint>=map->datasize){
              hint=~size_type(0);
              return;
            }
            while(true){
              const size_type k = hint/digits<size_type>();
              const size_type l = hint%digits<size_type>();
              const size_type m = (~size_type(0))<<(digits<size_type>()-l-1);
              assert(k<map->masksize);
              size_type p = (map->mask[k]&m)>>(digits<size_type>()-l-1);
              if (k!=0) p|=shl(map->mask[k-1]&(~m),l+1);
              const size_type s = ctz(p);
              if (s==0) break;
              hint-=s;
              if (hint>=map->datasize){
                hint=~size_type(0);
                return;
              }
            }
            key = map->data[hint].first;
          }
          template<bool is_const0,bool is_const1>
            difference_type inline friend diff(
                const_noconst_iterator<is_const0>& it0,
                const_noconst_iterator<is_const1>& it1){
              if (it1<it0) return -diff(it0,it1);
              it0.update_hint();
              it1.update_hint();
              const size_type k0 = it0->hint/digits<size_type>();
              const size_type l0 = it0->hint%digits<size_type>();
              const size_type m0 = (~size_type(0))>>l0;
              const size_type k1 = it1->hint/digits<size_type>();
              const size_type l1 = it1->hint%digits<size_type>();
              const size_type m1 = (~size_type(0))<<(digits<size_type>()-l1-1);
              if (k0==k1) return popcount(m0&m1&it0.map->mask[k0])-1;
            size_type d = popcount(m0&it0.map->mask[k0])
                         +popcount(m1&it1.map->mask[k1]);
            for (size_type i = k0+1;i!=k1;++i)
              d+=popcount(it0.map->mask[i]);
            return d;
          }
          void inline add(const size_type& n){
            update_hint();
                  size_type k = hint/digits<size_type>();
            const size_type l = hint%digits<size_type>();
            const size_type m = (~size_type(0))>>l;
                  size_type i = 0;
                  size_type p = popcount(map->mask[k]&m)-1; 
            while (i+p<n){
              if (++k>=map->mapsize){
                hint=~size_type(0);
                return;
              }
              hint+=digits<size_type>();
              p = popcount(map->mask[k]);
            }
            for (;i!=n;++i) unsafe_increment();
            key = map->data[hint].first;
          }
          void inline sub(const size_type& n){
            update_hint();
                  size_type k = hint/digits<size_type>();
            const size_type l = hint%digits<size_type>();
            const size_type m = (~size_type(0))<<(digits<size_type>()-l-1);
                  size_type i = 0;
                  size_type p = popcount(map->mask[k]&m)-1;
            while (i+p<n){
              if (--k>=map->mapsize){
                hint=~size_type(0);
                return;
              }
              hint+=digits<size_type>();
              p = popcount(map->mask[k]);
            }
            for (;i!=n;++i) unsafe_decrement();
            key = map->data[hint].first;
          }
        public:
          typedef typename alloc::difference_type difference_type;
          typedef typename alloc::value_type value_type;
          typedef typename
            conditional<is_const,
                        const typename alloc::reference,
                              typename alloc::reference
                       >::type
            reference;
          typedef typename
            conditional<is_const,
                        const typename alloc::pointer,
                              typename alloc::pointer
                       >::type
            pointer;
          typedef std::bidirectional_iterator_tag iterator_category;
          const_noconst_iterator(){
            //cout << "constructor 0" << endl;
          }
          const_noconst_iterator(
            const size_t& hint,
            typename conditional<is_const,
                                 const sparse_patchmap*,
                                       sparse_patchmap*
                                >::type map)
            :hint(hint),key(key_type{}),map(map){
            //cout << "constructor 1 " << hint << endl;
          }
          const_noconst_iterator(
            const size_t& hint,
            const key_type& key,
            typename conditional<is_const,
                                 const sparse_patchmap*,
                                       sparse_patchmap*
                                >::type map)
            :hint(hint),key(key),map(map) {
              //cout << "constructor 2 " << hint << endl;
          }
          ~const_noconst_iterator(){
          //cout << "destructor of const_noconst_iterator " << is_const << endl;
          //cout << hint << endl;
          }
          // copy constructor
          template<bool is_const_other>
          const_noconst_iterator(const const_noconst_iterator<is_const_other>& o)
          :hint(o.hint),key(o.key),map(o.map){
            //cout << "copy constructor" << endl;
          }
          // move constructor
          template<bool is_const_other>
          const_noconst_iterator(
              const_noconst_iterator<is_const_other>&& o) noexcept{
            //cout << "move constructor" << endl;
            swap(hint,o.hint);
            swap(key,o.key);
            swap(map,o.map);
          }
          // copy assignment
          template<bool is_const_other>
          const_noconst_iterator<is_const>& operator=(
              const const_noconst_iterator<is_const_other>& other){
            //cout << "copy assignment" << endl;
            return  (*this=const_noconst_iterator<is_const>(other));
          }
          template<bool is_const_other>
          bool operator==(
              const const_noconst_iterator<is_const_other>& o) const {
            //cout << "comparing " << hint << " " << key << " with "
            //     << o.hint << " " << key << endl;
            if ((hint>=map->datasize)&&(o.hint>=o.map->datasize)) return true;
            if ((hint>=map->datasize)||(o.hint>=o.map->datasize)) return false;
            if (key!=o.key) return false;
            return true;
          }
          template<bool is_const_other>
          bool operator!=(
              const const_noconst_iterator<is_const_other>& o) const{
            return !((*this)==o);
          }
          template<bool is_const_other>
          bool operator< (
              const const_noconst_iterator<is_const_other>& o) const{
            if ((o.hint<o.mpa->datasize)){
              if (hint<map->datasize){
                return comp(key,o.key);
              }else{
                return false;
              }
            } else {
              return false;
            }
          }
          template<bool is_const_other>
          bool operator> (
              const const_noconst_iterator<is_const_other>& o) const{
            if ((o.hint<o.mpa->datasize)){
              if (hint<map->datasize){
                return (!comp(key,o.key))&&(!equal(key,o.key));
              }else{
                return true;
              }
            } else {
              return false;
            }
          }
          template<bool is_const_other>
          bool operator<=(
              const const_noconst_iterator<is_const_other>& o) const{
            if ((o.hint<o.mpa->datasize)){
              if (hint<map->datasize){
                return comp(key,o.key)||equal(key,o.key);
              }else{
                return false;
              }
            } else {
              return true;
            }
          }
          template<bool is_const_other>
          bool operator>=(
              const const_noconst_iterator<is_const_other>& o) const{
            if ((o.hint<o.mpa->datasize)){
              if (hint<map->datasize){
               return !comp(key,o.key);
              }else{
                return true;
              }
            } else {
              return true;
            }
          }
          const_noconst_iterator<is_const>& operator++(){   // prefix
            //cout << "operator++()" << endl;
            update_hint();
            unsafe_increment();
            return *this;
          }
          const_noconst_iterator<is_const> operator++(int){ // postfix
            update_hint();
            iterator pre(*this);
            unsafe_increment();
            return pre;
          }
          const_noconst_iterator<is_const>& operator--(){   // prefix
            update_hint();
            unsafe_decrement();
            return *this;
          }
          const_noconst_iterator<is_const> operator--(int){ // postfix
            update_hint();
            iterator pre(*this);
            unsafe_decrement();
            return pre;
          }
          // not a random_acces_iterator but we can still do better than default
          template<bool is_const_other>
          difference_type operator-(
              const const_noconst_iterator<is_const_other>& o) const {
            iterator it0(*this);
            iterator it1(o);
            return diff(it0,it1);
          }
          const_noconst_iterator<is_const>& operator+=(const size_type& n){
            add(n);
            return *this;
          }
          const_noconst_iterator<is_const> operator+(const size_type& n) const {
            return (const_noconst_iterator<is_const>(*this)+=n);
          }
          friend const_noconst_iterator<is_const> operator+(
              const size_type& n,
              const const_noconst_iterator<is_const>& it){
            return (const_noconst_iterator<is_const>(it)+=n);
          }
          const_noconst_iterator<is_const>& operator-=(const size_type& n){
            sub(n);
            return *this;
          }
          const_noconst_iterator<is_const> operator-(const size_type& n) const{
            return (const_noconst_iterator<is_const>(*this)-=n);
          }
          reference operator*() {
            update_hint();
            return map->data[hint];
          }
          pointer operator->() {
            update_hint();
            return &(map->data[hint]);
          }
          reference operator*() const {
            size_type i;
            if (hint>=map->datasize){
              i = map->find_node(key);
            } else if (map->data[hint]!=key){
              i = map->find_node(key,hint);
            } else {
              i = hint;
            }
            return map->data[i];
          }
          pointer operator->() const {
            size_type i;
            if (hint>=map->datasize){
              i = map->find_node(key);
            } else if (map->data[hint]!=key){
              i = map->find_node(key,hint);
            } else {
              i = hint;
            }
            return &(map->data[i]);
          }
    };
    typedef const_noconst_iterator<false> iterator;
    typedef const_noconst_iterator<true>  const_iterator;    
    iterator begin(){
      //cout << "begin()" << endl;
      const size_type i = find_first();
      //cout << "this should call constructor 2" << endl;
      return iterator(i,data[i].first,this);
    }
    const_iterator begin() const {
      //cout << "begin()" << endl;
      const size_type i = find_first();
      //cout << "this should call constructor 2" << endl;
      return const_iterator(i,data[i].first,this);
    }
    const_iterator cbegin() const {
      //cout << "cbegin()" << endl;
      const size_type i = find_first();
      //cout << "this should call constructor 2" << endl;
      return const_iterator(i,data[i].first,this);
    }
    iterator end() {
      //cout << "end()" << endl;
      const size_type i = find_first();
      //cout << "this should call constructor 1" << endl;
      return iterator(~size_type(0),this);
    }
    const_iterator end() const {
      //cout << "end()" << endl;
      //cout << "this should call constructor 2" << endl;
      return const_iterator(~size_type(0),this);
    }
    const_iterator cend() const {
      //cout << "cend()" << endl;
      //cout << "this should call constructor 2" << endl;
      return const_iterator(~size_type(0),this);
    }
    size_type max_size()         const{return numeric_limits<size_type>::max();}
    bool empty()                 const{return (num_data==0);}
    size_type bucket_count()     const{return datasize;}
    size_type max_bucket_count() const{return numeric_limits<size_type>::max();}
    void rehash(const size_type& n) { if (n>=size()) resize(n); }
    void reserve(const size_type& n){ if (3*n>=2*(size()+1)) resize(n*3/2); }
    pair<iterator,bool> insert ( const value_type& val ){
      const size_type i = find_node(key_of(val));
      if (i<datasize) return {iterator(i,key_of(val),this),false};
      ensure_size();
      const size_type j = reserve_node(key_of(val));
      allocator_traits<alloc>::construct(allocator,data+j,val);
      return {{j,key_of(val),this},true};
    }
    template <class P>
    pair<iterator,bool> insert ( P&& val ){
      const size_type i = find_node(key_of(val));
      if (i<datasize) return {iterator(i,key_of(val),this),false};
      ensure_size();
      const size_type j = reserve_node(key_of(val));
      if constexpr (is_same<void,mapped_type>::value)
        allocator_traits<alloc>::construct(allocator,data+j,
            std::pair(val,wmath::empty{}));
      else
        allocator_traits<alloc>::construct(allocator,data+j,val);
      return {{j,key_of(val),this},true};
    }
    iterator insert ( const_iterator hint, const value_type& val ){
      const size_type i = find_node(key_of(val),hint.hint);
      if (i<datasize) return {iterator(i,key_of(val),this),false};
      ensure_size();
      const size_type j = reserve_node(key_of(val));
      if constexpr (is_same<void,mapped_type>::value)
        allocator_traits<alloc>::construct(allocator,data+j,{val,{}});
      else
        allocator_traits<alloc>::construct(allocator,data+j,val);
      return {{j,key_of(val),this},true};
    }
    template <class P>
    iterator insert ( const_iterator hint, P&& val ){
      const size_type i = find_node(key_of(val),hint.hint);
      if (i<datasize) return {iterator(i,key_of(val),this),false};
      ensure_size();
      const size_type j = reserve_node(key_of(val),hint.hint);
      if constexpr (is_same<void,mapped_type>::value)
        allocator_traits<alloc>::construct(allocator,data+j,{val,{}});
      else
        allocator_traits<alloc>::construct(allocator,data+j,val);
      return {{j,key_of(val),this},true};
    }
    template <class InputIterator>
    void insert ( InputIterator first, InputIterator last ){
      for (auto it(first);it!=last;++it){
        insert(*it);
      }
    }
    void insert ( initializer_list<value_type> il ){
      insert(il.begin(),il.end());
    }
    template <class... Args>
    pair<iterator, bool> emplace ( Args&&... args ){
      insert(value_type(args...));
    }
    template <class... Args>
    iterator emplace_hint(const_iterator position,Args&&... args){
      insert(position,value_type(args...));
    }
    pair<iterator,iterator> equal_range(const key_type& k){
      const size_type i = find_node(k);
      if (i>=datasize) return {end(),end()};
      iterator lo(i,data[i].first,this);
      iterator hi(lo);
      ++hi;
      return {lo,hi};
    }
    pair<const_iterator,const_iterator>
    equal_range ( const key_type& k ) const{
      const size_type i = find_node(k);
      if (i>=datasize) return {cend(),cend()};
      iterator lo(i,data[i].first,this);
      iterator hi(lo);
      ++hi;
      return {lo,hi};
    }
    float load_factor() const noexcept{
      return float(num_data)/float(datasize);
    }
    float average_patchsize() const noexcept{
      double avg = 0;
      double counter = 0;
      for (size_type i=0;i<datasize;++i){
        const size_type j = search_free_inc(i);
        if (j<datasize) avg += (j-i);
        else break;
        i=j;
        ++counter;
      }
      return avg/counter;
    }
    void print_patchsizes() const noexcept{
      for (size_type i=0;i<datasize;++i){
        const size_type j = search_free_inc(i);
        if (j<datasize) cout << j-i << endl;
        else break;
        i=j;
      }
    }
    float max_load_factor() const noexcept{
      return 1;
    }
    template<bool is_const>
    iterator erase(const_noconst_iterator<is_const> position){
      iterator it(position);
      ++it;
      erase(position.key);//,position.hint);
      return it;
    }
    template<bool is_const>
    iterator erase(
        const_noconst_iterator<is_const> first,
        const_noconst_iterator<is_const> last){
      for (auto it=first;it!=last;it=erase(it));
    }
  }; 
}



#endif // SPARSE_PATCHMAP_H
