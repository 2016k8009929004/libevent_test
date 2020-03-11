all:
	g++ -std=c++11 test_main.cc obj/threadpool.cc obj/btree.cc mem/pm_alloc.cc lib/city.cc lib/pflush.c ntstore/ntstore.c -Intstore -Imem -Ilib -Iobj -Itbb -Ipmdk/include -o hikv  -L ./third-party/jemalloc-4.2.1/lib -ljemalloc -L ./third-party/tbb -ltbb -lpmem -lpthread

use_pmdk:
	g++ -std=c++11 test_main.cc obj/threadpool.cc obj/btree.cc mem/pm_alloc.cc lib/city.cc lib/pflush.c ntstore/ntstore.c -Intstore -Imem -Ilib -Iobj -Itbb -Ipmdk/include -o hikv  -L ./third-party/jemalloc-4.2.1/lib -ljemalloc -L ./third-party/tbb -ltbb -lpmem -lpthread -DPM_USED -DPMDK_USED -DPERSIST_HASHTABLE

export_lib:
	export LD_LIBRARY_PATH=./third-party/tbb:./third-party/jemalloc-4.2.1/lib/
