#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <map>
#include <random>
#include <chrono>

using namespace std;

// global constants
int const MAX_NUM_OF_CHAN = 6; //Number of AdcInputChannels
int const MAX_NUM_OF_THREADS = 6;
int const DATA_BLOCK_SIZE = 20;

std::mt19937 gen(time(0)); //Random number generator
std::uniform_int_distribution<> dis(100, 500); //Distribution of 100-500ms

// global variables
std::condition_variable cond; //Instance of a condition variable for wait and notify_all

class AdcInputChannel {
  private:
    int currentSample;

  public:
    AdcInputChannel(int d)
    : currentSample(d) {}

    //Request a sample from the sample channel
    double getCurrentSample() {
      return currentSample * 2;
    }
};

class Lock {
  private:
    bool open;

  public:
    Lock()
    : open(true) {}

    bool lock() {
      if (open) {
        // Lock not in use, set it as closed and report it has been locked
        open = false;
        return true;
      } else {
        // Lock in use, report already locked
        return false;
      }
    }

    void unlock() {
      open = true;

      cond.notify_all();
    }
};

class ADC {
  private:
    Lock theADCLock; //Lock instance
    int sampleChannel; //Current sample channel
    std::vector<AdcInputChannel>& adcChannels; //Reference to channels vector
    std::mutex ADC_mu; //ADC mutex

    std::map<std::thread::id, int> threadIDs; //Map of thread IDs to channel numbers
    std::mutex map_mu; //Map mutex
    
  public:
    //constructor: initialises A vector of ADCInputChannels passed in by reference:
    ADC(std::vector<AdcInputChannel>& channels)
    : adcChannels(channels),
      theADCLock(Lock()) 
    {}

    //Adds tuple of thread id and channel # to map
    void addThreadId(int id) {
      std::unique_lock<std::mutex> locker(map_mu);

      threadIDs.insert(std::make_pair(std::this_thread::get_id(), id));
    }

    //Returns channel # for this thread
    int getThreadId() {
      std::map <std::thread::id, int>::iterator it = threadIDs.find(std::this_thread::get_id());

      // if not found, return -1, otherwise return int id
      return it == threadIDs.end() ? -1 : it->second;
    }

    //Attempts to lock the ADC then sets sampleChannel
    void requestADC(int channel) {
      std::unique_lock<std::mutex> locker(ADC_mu);
      cout << "Thread " << channel << " requested ADC" << endl;

      while (!theADCLock.lock()) {
        cout << "Thread " << channel << " is about to be suspended until the ADC is free" << endl;
        cond.wait(locker);
      }

      cout << "Thread " << channel << " locked ADC" << endl;

      sampleChannel = channel;
    }

    //Returns sample from ADC
    double sampleADC() {
      double sample = adcChannels[sampleChannel].getCurrentSample();

      cout << "Thread " << getThreadId() << " got ADC sample value of: " << sample << endl;

      return sample;
    }

    //Unlocks ADC
    void releaseADC() {
      theADCLock.unlock();

      cout << "Thread " << getThreadId() << " released ADC" << endl;
    }

};

//Delays thread for between 0.1 and 0.5 seconds
void delayRandom() {
  int millis = dis(gen);

  std::this_thread::sleep_for (std::chrono::milliseconds(millis));
}

//run function executed by each thread:
void run(ADC& theADC, int id) {
  theADC.addThreadId(id);

  double sampleBlock[DATA_BLOCK_SIZE] = {0.0}; //init all elements to 0

  for (int i=0; i<50; i++) { //replace with 'i<DATA_BLOCK_SIZE;' for A2
    theADC.requestADC(id);

    theADC.sampleADC();

    theADC.releaseADC();

    delayRandom();
  }

  cout << "Thread " << id << " finished executing" << endl;
}

int main() {
  cout << "starting main" << endl;

  //Vector storing all the ADC channels
  std::vector<AdcInputChannel> adcChannels;
  for (int i=0; i < MAX_NUM_OF_CHAN; i++) {
    cout << "initing ADCChannel " << i << endl;

    //Channel instance with unique channel num
    AdcInputChannel channel(i);

    //Add channel to vector
    adcChannels.push_back(channel);
  }

  //ADC Instance
  ADC theAdc(adcChannels);

  //Array of threads
  std::thread the_threads[MAX_NUM_OF_THREADS];
  for (int i = 0; i < MAX_NUM_OF_THREADS; i++) {
    // Launch threads
    cout << "launching thread " << i << endl;
    the_threads[i] = std::thread(run, std::ref(theAdc), i);
  }

  //Join the threads with the main thread
  for (int i = 0; i < MAX_NUM_OF_THREADS; i++) {
    the_threads[i].join();
  }

  cout << "All threads terminated." << endl;

  return 0;
}
