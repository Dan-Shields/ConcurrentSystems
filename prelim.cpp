/* Concurrent Systems Preliminary Lab Part 2 v1 (no deadlock)

   Compile from the command line with, for example:
   C:\MinGW64\bin\g++ -std=c++11 -lpthread Part_2.cpp
*/

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace std;

//global variables
std::mutex mu;   //instance of mutual exclusion lock
std::condition_variable cond;  //instance of a condition variable for wait and notify_all
std::unique_lock<std::mutex> locker;
const int PSLEEP = 1; //producer thread sleep period
const int CSLEEP = 1; //consumer thread sleep period
const int NPRODS = 3; //number of producer threads
const int NCONS = 2; //number of consumer threads

class Buffer {
  public:
    Buffer()    // constructor
    : count(0),
      activeProducers(NPRODS), activeConsumers(NCONS),
      noActiveProducer(false), noActiveConsumer(false)
    {}  //end constructor

  void put() {
    std::unique_lock<std::mutex> locker(buf_mu);
    if (count == 10) {
      cout << "  buffer is full, producer thread "
           << std::this_thread::get_id()
           << " is about to suspend.." << endl;
      while ((count == 10) && !noActiveConsumer) {
           cond.wait(locker);
      }
    }
    else {
      // A successful deposit!
      count++;
      cout << "  producer thread " << std::this_thread::get_id()
           << ", count = " << count << endl;
      // Notify all waiting threads that the buffer is not empty
      cond.notify_all();
    }
  }  //end put method

  void get() {
    std::unique_lock<std::mutex> locker(buf_mu);
    if (count == 0) {
      cout << "  buffer is empty, consumer thread "
           << std::this_thread::get_id()
           << " is about to suspend.." << endl;
      while ((count == 0) && !noActiveProducer) {
           cond.wait(locker);
      }
    }
    else {
      // A successful extraction
      count--;
      cout << "  consumer thread " << std::this_thread::get_id()
           << ", count = " << count << endl;
      // Notify all waiting threads that the buffer is not full
      cond.notify_all();
    }
  } //end get method
  // Used by consumer threads to check if there are any active producers
  bool isNoActiveProducer() {
    return noActiveProducer;
  }
  // Used by producer threads to check if there are any active consumers
  bool isNoActiveConsumer() {
    return noActiveConsumer;
  }
  // Used to tell buffer when a consumer thread has terminated
  // This prevents deadlock (producers waiting when consumers have finished)
  void consumerTerminated() {
    std::unique_lock<std::mutex> get_lock(buf_mu);
    if (--activeConsumers == 0) {
      noActiveConsumer = true;
      cout << "                All consumers have terminated" << endl;
      cond.notify_all();
    }
  } // end consumerTerminated method

    // Used to tell buffer when a producer thread has terminated
    // This prevents deadlock (consumers waiting when producers have finished)
  void producerTerminated() {
    std::unique_lock<std::mutex> get_lock(buf_mu);
    if (--activeProducers == 0) {
        noActiveProducer = true;
        cout << "                All producers have terminated" << endl;
        cond.notify_all();
    }
  } // end producerTerminated method
  private:
    int count, activeProducers, activeConsumers;
    bool noActiveProducer, noActiveConsumer;
    std::mutex buf_mu;      //mutex shared by put and get
};  //end class Buffer

void prods(Buffer& b) {     //pass in a buffer reference
  for (int i=0; i<100; i++) {
     if (b.isNoActiveConsumer()) {   //no point continuing if not..
       break;
     }
    b.put();
    std::this_thread::sleep_for (std::chrono::milliseconds(PSLEEP));
  }

  cout << "  PRODUCER " << std::this_thread::get_id() << " FINISHED" << endl;
  b.producerTerminated();
}

void cons(Buffer& b) {     //pass in a buffer reference
  int i;
  for (i=0; i<100; i++) {
     if (b.isNoActiveProducer()) {   //no point continuing if not..
       break;
     }
    b.get();
    std::this_thread::sleep_for (std::chrono::milliseconds(CSLEEP));
  }
  cout << "  CONSUMER " << std::this_thread::get_id() << " FINISHED" << endl;
  b.consumerTerminated();
}

int main() {
  std::thread p_t[NPRODS];   //bunch of producer threads
  std::thread c_t[NCONS];    //bunch of consumer threads

  Buffer buf;  //instance of Buffer class

  //Launch a group of producer threads
  for (int i = 0; i < NPRODS; i++) {
    //launch the producer threads:
    p_t[i] = std::thread(prods, std::ref(buf));
  }
  for (int i=0; i < NCONS; i++) {
    //launch the consumer threads:
    c_t[i] = std::thread(cons, std::ref(buf));
  }

  //Join the threads with the main thread
  for (int i = 0; i < NPRODS; ++i) {
    p_t[i].join();
  }
  for (int i = 0; i < NCONS; ++i) {
    c_t[i].join();
  }

  cout << "  All threads terminated." << endl;

  return 0;
}
