/*
 * MySharedMemory.hpp
 *
 *  Created on: Jun 7, 2019
 *      Author: David Daharewa Gureya
 */

#ifndef INCLUDE_MYSHAREDMEMORY_HPP_
#define INCLUDE_MYSHAREDMEMORY_HPP_

#include <boost/interprocess/containers/string.hpp>

namespace ipc = boost::interprocess;

class MySharedMemory {
 public:
  void* pageAlignedStartAddress;
  unsigned long pageAlignedLength;
  pid_t processID;

  //constructor
  MySharedMemory(void* start, unsigned long len, pid_t pid);
};

MySharedMemory::MySharedMemory(void* start, unsigned long len, pid_t pid) {
  pageAlignedStartAddress = start;
  pageAlignedLength = len;
  processID = pid;
}

#endif /* INCLUDE_MYSHAREDMEMORY_HPP_ */
