cmake_minimum_required(VERSION 3.13)
project(ConcurrentSkipList)
set(CMAKE_CXX_STANDARD 17)
include_directories(C:/SkipListRealisations/mingw-std-threads-master/)

add_executable(ConcurrentSkipList main.cpp concurrent_lockfree_skiplist.h atomic_markable_reference.h hazard_domain.h)