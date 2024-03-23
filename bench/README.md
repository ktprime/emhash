# about bench
  each cpp bench file(collected from github) can be complied and benched.

# how to build (compile options in makefile)
set c++ compiler and absl hash/some hash map(ET=1 : martin/phmap/ska/tsl. ET=2.. more hash maps)
 ###  make CXX=clang++ ET=1 SW=1 BF=1

set key=string,val=int (0-uint32_t 1-int64_t 2-std::string/(view) 4-struct)
 ###  make Key=2 Val=0

use martinus's hash and rand function (RT=1-5)
 ###  make MH=1 RT=3

set interger hash function and c++20 (FH=1-6, if FH > 100 means bad hash function ex: hash(n) = n / FH or n * FH)
 ###  make FH=2 std=20

other compile options
 ### make AVX2=1 AH=1 QB=1 std=20

 # compile ebench (ET=1/2 means more hash map added)
 ### g++ -I.. -I../thirdparty -O3 -march=native -DET=1 -DHOOD_HASH=1 -DABSL_HMAP=1 ebench.cpp -o eb

# compile martinus bench
 ### g++ -I.. -I../thirdparty -O3 -march=native -DET=2 -DHAVE_BOOST=1 -DABSL_HMAP=1 martin_bench.cpp -o mb

# compile qc bench
 ### g++ -I.. -I../thirdparty -std=c++20 -O3 -march=native -DCXX20=1 -DET=1 -DABSL_HMAP=1 qbench.cpp -o qb
 


|10       hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|absl::f_hash_map|  25.8| 4.39| 5.14| 15.5| 6.65| 66.7     |
|emilib1::HashMap|  26.2| 5.52| 7.05| 11.2| 4.32| 62.5     |
|emilib2::HashMap|  26.0| 5.30| 6.86| 16.0| 3.93| 62.5     |
|emilib3::HashMap|  13.6| 5.13| 5.49| 14.1| 3.90| 62.5     |
|phmap::fhash_map|  26.1| 6.01| 7.22| 14.7| 6.41| 66.7     |
|martinus::fhmap |  29.8| 9.30| 6.89| 9.19| 4.20| 62.5     |
|ska:flat_hashmap|  31.0| 7.34| 10.0| 7.90| 8.42| 62.1     |
|tsl::robin_map  |  25.0| 7.52| 10.1| 8.38| 7.58| 62.5     |
|std::unorder_map|  69.9| 8.33| 12.8| 30.6| 3.39| 58.8     |
|jg::den_hash_map|  24.3| 6.13| 8.75| 9.36| 3.42| 62.5     |
|rigtorp::HashMap|  10.1| 6.84| 12.8| 11.9| 7.17| 62.5     |
|qc__hash::RawMap|  13.3| 4.88| 9.57| 5.17| 11.2| 31.2     |
|emhash8::HashMap|  24.1| 6.17| 8.77| 10.4| 3.45| 62.5     |
|emhash7::HashMap|  23.8| 6.07| 9.03| 6.72| 3.81| 62.5     |
|emhash6::HashMap|  23.8| 5.76| 8.77| 8.22| 4.89| 62.5     |
|emhash5::HashMap|  23.7| 5.89| 8.76| 5.92| 7.52| 62.5     |


|200      hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|absl::f_hash_map|  12.6| 2.03| 5.77| 9.27| 2.10| 78.4     |
|emilib1::HashMap|  12.6| 2.87| 7.56| 5.59| 2.01| 78.1     |
|emilib2::HashMap|  12.3| 2.69| 6.24| 9.08| 1.64| 78.1     |
|emilib3::HashMap|  12.3| 3.26| 6.54| 5.63| 1.65| 78.1     |
|phmap::fhash_map|  13.8| 3.70| 8.31| 10.9| 2.09| 78.4     |
|martinus::fhmap |  23.2| 10.2| 7.35| 9.81| 2.25| 78.1     |
|ska:flat_hashmap|  25.6| 7.11| 8.67| 6.84| 3.90| 65.9     |
|tsl::robin_map  |  22.3| 8.65| 10.3| 9.82| 2.78| 78.1     |
|std::unorder_map|  40.1| 6.94| 11.5| 25.0| 1.12| 77.8     |
|jg::den_hash_map|  12.0| 4.41| 7.06| 6.21| 0.54| 78.1     |
|rigtorp::HashMap|  8.97| 6.46| 14.9| 14.5| 2.54| 78.1     |
|qc__hash::RawMap|  10.3| 6.21| 12.5| 6.39| 2.57| 78.1     |
|emhash8::HashMap|  16.5| 4.27| 6.01| 8.19| 0.79| 78.1     |
|emhash7::HashMap|  14.6| 4.38| 6.21| 4.12| 1.25| 78.1     |
|emhash6::HashMap|  14.2| 3.94| 7.08| 5.43| 1.30| 78.1     |
|emhash5::HashMap|  16.9| 4.13| 6.05| 3.70| 2.76| 78.1     |


|3000     hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|absl::f_hash_map|  11.5| 1.79| 4.48| 6.71| 2.25| 73.3     |
|emilib1::HashMap|  10.0| 2.48| 5.75| 3.60| 1.95| 73.2     |
|emilib2::HashMap|  9.97| 2.33| 4.77| 7.84| 1.52| 73.2     |
|emilib3::HashMap|  10.5| 2.98| 5.28| 3.35| 1.51| 73.2     |
|phmap::fhash_map|  13.6| 3.47| 6.89| 9.03| 2.24| 73.3     |
|martinus::fhmap |  21.9| 9.88| 6.45| 8.94| 2.00| 73.2     |
|ska:flat_hashmap|  25.5| 7.33| 8.99| 6.84| 3.77| 63.4     |
|tsl::robin_map  |  21.4| 8.88| 10.6| 9.09| 3.08| 73.2     |
|std::unorder_map|  38.4| 7.48| 12.6| 26.2| 2.09| 80.2     |
|jg::den_hash_map|  10.1| 4.49| 7.28| 6.28| 0.36| 73.2     |
|rigtorp::HashMap|  8.63| 6.02| 14.1| 13.5| 2.83| 73.2     |
|qc__hash::RawMap|  9.90| 5.80| 12.3| 5.98| 2.82| 73.2     |
|emhash8::HashMap|  15.6| 3.97| 5.97| 8.20| 0.31| 73.2     |
|emhash7::HashMap|  13.0| 4.13| 6.39| 4.02| 1.09| 73.2     |
|emhash6::HashMap|  13.5| 3.66| 7.01| 5.39| 1.09| 73.2     |
|emhash5::HashMap|  16.3| 3.86| 6.16| 3.64| 3.14| 73.2     |


|40000    hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|absl::f_hash_map|  21.3| 3.06| 3.14| 6.36| 3.42| 61.0     |
|emilib1::HashMap|  15.8| 3.65| 3.63| 4.03| 2.41| 61.0     |
|emilib2::HashMap|  16.1| 3.72| 3.40| 10.1| 1.80| 61.0     |
|emilib3::HashMap|  16.7| 4.07| 3.73| 4.48| 1.80| 61.0     |
|phmap::fhash_map|  23.5| 5.51| 5.55| 9.01| 3.35| 61.0     |
|martinus::fhmap |  27.6| 9.43| 5.27| 8.47| 2.17| 61.0     |
|ska:flat_hashmap|  28.5| 8.35| 10.7| 8.14| 4.26| 60.9     |
|tsl::robin_map  |  29.0| 8.57| 11.2| 8.83| 4.80| 61.0     |
|std::unorder_map|  56.0| 10.7| 17.9| 44.1| 7.01| 81.3     |
|jg::den_hash_map|  14.1| 5.71| 8.92| 9.04| 0.39| 61.0     |
|rigtorp::HashMap|  10.2| 6.31| 16.4| 13.7| 4.45| 61.0     |
|qc__hash::RawMap|  14.1| 6.09| 18.0| 6.33| 4.51| 61.0     |
|emhash8::HashMap|  22.7| 4.42| 7.26| 10.3| 0.32| 61.0     |
|emhash7::HashMap|  20.5| 4.85| 7.99| 4.91| 1.20| 61.0     |
|emhash6::HashMap|  19.4| 4.00| 7.66| 6.85| 1.20| 61.0     |
|emhash5::HashMap|  22.7| 4.33| 7.71| 4.29| 5.00| 61.0     |


|500000   hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|absl::f_hash_map|  40.5| 13.8| 4.45| 19.9| 4.55| 47.7     |
|emilib1::HashMap|  29.2| 15.5| 4.37| 13.6| 2.99| 47.7     |
|emilib2::HashMap|  30.9| 15.6| 4.53| 19.1| 2.25| 47.7     |
|emilib3::HashMap|  30.5| 14.2| 4.79| 13.8| 2.43| 47.7     |
|phmap::fhash_map|  42.6| 20.9| 6.99| 26.7| 4.45| 47.7     |
|martinus::fhmap |  42.2| 15.1| 6.00| 14.4| 2.54| 47.7     |
|ska:flat_hashmap|  47.4| 12.2| 15.5| 17.2| 6.43| 47.7     |
|tsl::robin_map  |  43.2| 12.8| 15.7| 15.7| 6.95| 47.7     |
|std::unorder_map|  113.| 20.2| 31.9| 106.| 31.2| 82.1     |
|jg::den_hash_map|  32.3| 10.7| 11.8| 18.5| 1.26| 47.7     |
|rigtorp::HashMap|  32.9| 9.02| 17.5| 17.5| 6.39| 47.7     |
|qc__hash::RawMap|  35.4| 8.77| 35.1| 8.64| 6.32| 47.7     |
|emhash8::HashMap|  39.1| 6.57| 8.09| 14.2| 0.88| 47.7     |
|emhash7::HashMap|  36.6| 11.3| 10.2| 10.6| 1.62| 47.7     |
|emhash6::HashMap|  28.9| 9.08| 10.6| 14.1| 1.65| 47.7     |
|emhash5::HashMap|  31.2| 8.10| 11.2| 8.06| 7.13| 47.7     |


|7200000  hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|absl::f_hash_map|  41.5| 18.6| 18.2| 29.0| 1.56| 85.8     |
|emilib1::HashMap|  36.4| 22.1| 25.7| 22.4| 1.94| 85.8     |
|emilib2::HashMap|  35.1| 23.0| 22.2| 27.6| 1.52| 85.8     |
|emilib3::HashMap|  36.0| 23.5| 22.6| 26.8| 1.58| 85.8     |
|phmap::fhash_map|  45.1| 27.8| 23.9| 39.7| 1.52| 85.8     |
|martinus::fhmap |  70.6| 34.9| 25.3| 45.6| 2.17| 85.8     |
|ska:flat_hashmap|  85.6| 15.2| 17.6| 25.1| 6.62| 42.9     |
|tsl::robin_map  |  81.3| 73.3| 73.5| 52.9| 2.10| 85.8     |
|std::unorder_map|  280.| 32.0| 49.4| 276.| 60.8| 81.4     |
|jg::den_hash_map|  73.6| 23.8| 29.1| 49.6| 1.03| 85.8     |
|rigtorp::HashMap|  54.5| 42.0| 118.| 72.7| 1.80| 85.8     |
|qc__hash::RawMap|  62.1| 40.7| 124.| 43.9| 1.82| 85.8     |
|emhash8::HashMap|  79.5| 18.2| 17.4| 49.5| 0.69| 85.8     |
|emhash7::HashMap|  48.6| 19.0| 20.5| 20.5| 1.28| 85.8     |
|emhash6::HashMap|  52.8| 15.8| 20.9| 33.7| 1.35| 85.8     |
|emhash5::HashMap|  60.8| 17.0| 19.1| 21.2| 2.00| 85.8     |


|10000000 hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|absl::f_hash_map|  53.2| 22.6| 11.7| 41.5| 3.49| 59.6     |
|emilib1::HashMap|  49.1| 29.2| 14.0| 32.4| 2.46| 59.6     |
|emilib2::HashMap|  48.9| 28.1| 13.8| 41.0| 2.05| 59.6     |
|emilib3::HashMap|  47.8| 27.1| 13.5| 34.8| 1.99| 59.6     |
|phmap::fhash_map|  59.5| 36.4| 19.7| 56.2| 3.46| 59.6     |
|martinus::fhmap |  70.9| 28.0| 23.6| 33.5| 2.43| 59.6     |
|ska:flat_hashmap|  69.8| 19.9| 23.5| 30.9| 4.60| 59.6     |
|tsl::robin_map  |  70.9| 20.8| 24.3| 30.6| 5.20| 59.6     |
|std::unorder_map|  295.| 33.0| 52.0| 293.| 62.6| 82.5     |
|jg::den_hash_map|  84.5| 23.6| 26.4| 46.8| 1.20| 59.6     |
|rigtorp::HashMap|  39.4| 21.9| 66.5| 47.0| 4.52| 59.6     |
|qc__hash::RawMap|  47.3| 20.9| 111.| 25.6| 4.51| 59.6     |
|emhash8::HashMap|  84.8| 16.5| 16.8| 47.9| 0.67| 59.6     |
|emhash7::HashMap|  50.3| 16.6| 16.9| 18.6| 1.50| 59.6     |
|emhash6::HashMap|  51.6| 13.6| 17.6| 30.8| 1.46| 59.6     |
|emhash5::HashMap|  59.4| 15.2| 17.6| 19.7| 5.10| 59.6     |


|50000000 hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|absl::f_hash_map|  65.1| 27.5| 21.1| 49.7| 2.12| 74.5     |
|emilib1::HashMap|  63.8| 33.8| 25.1| 43.3| 2.01| 74.5     |
|emilib2::HashMap|  65.3| 33.9| 24.2| 50.5| 1.61| 74.5     |
|emilib3::HashMap|  66.1| 32.1| 24.4| 43.4| 1.64| 74.5     |
|phmap::fhash_map|  75.6| 41.2| 31.3| 60.8| 2.12| 74.5     |
|martinus::fhmap |  83.3| 37.7| 32.4| 45.6| 2.11| 74.5     |
|ska:flat_hashmap|  100.| 15.2| 16.8| 26.2| 6.86| 37.3     |
|tsl::robin_map  |  81.6| 31.8| 35.5| 42.3| 3.12| 74.5     |
|std::unorder_map|  334.| 35.3| 53.9| 319.| 69.2| 85.5     |
|jg::den_hash_map|  81.0| 24.1| 29.0| 52.6| 1.06| 74.5     |
|rigtorp::HashMap|  55.1| 40.1| 122.| 67.2| 2.67| 74.5     |
|qc__hash::RawMap|  60.5| 35.1| 155.| 45.7| 2.75| 74.5     |
|emhash8::HashMap|  89.0| 19.3| 18.4| 53.1| 0.67| 74.5     |
|emhash7::HashMap|  52.5| 18.8| 19.4| 22.3| 1.26| 74.5     |
|emhash6::HashMap|  56.1| 15.7| 21.1| 41.0| 1.54| 74.5     |
|emhash5::HashMap|  68.9| 18.4| 19.1| 22.0| 3.18| 74.5     |

