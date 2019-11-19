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
int const NUM_OF_LINKS = 3;
int const DATA_BLOCK_SIZE = 20;

std::mt19937 gen(time(0)); //Random number generator
std::uniform_int_distribution<> dis(100, 500); //Distribution of 100-500ms

// global variables
std::condition_variable adcCond;
std::condition_variable linkCond;

std::map<std::thread::id, int> threadIDs; //Map of thread IDs to channel numbers
std::mutex map_mu; //Map mutex

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

class Receiver {
  private:
    double dataBlocks[MAX_NUM_OF_THREADS][DATA_BLOCK_SIZE];

  public:
    Receiver () {
      for (int i = 0; i < MAX_NUM_OF_THREADS; i++) {
        for (int j = 0; j < DATA_BLOCK_SIZE; j++) {
          dataBlocks[i][j] = 0;
        }
      }
    }

    //Store data[] in index `id` of dataBlocks[][]
    void receiveDataBlock(int id, double data[]) {
      for (int i = 0; i < DATA_BLOCK_SIZE; i++) {
        dataBlocks[id][i] = data[i];
      }
    }

    //print out all items in dataBlocks[][]
    void printBlocks() {
      for (int i = 0; i < MAX_NUM_OF_THREADS; i++) {
        cout << "Sample data from thread " << i << ": ";

        for (int j = 0; j < DATA_BLOCK_SIZE; j++) {
          cout << dataBlocks[i][j] << ", ";
        }

        cout << endl;
      }
    }
};

class Link {
  private:
    bool inUse;
    Receiver& myReceiver;
    int linkId;
  
  public:
    Link (Receiver& r, int linkNum)
    : inUse(false), myReceiver(r), linkId(linkNum)
    {}

    //Check if link is in use
    bool isInUse() {
      return inUse;
    }
    
    void setInUse(bool status) {
      inUse = status;
    }

    //Write data[] to the receiver
    void writeToDataLink(int id, double data[]) {
      myReceiver.receiveDataBlock(id, data);

      cout << "Thread " << getThreadId() << " transmitted data to the Receiver" << endl;
    }

    //Return link ID
    int getLinkId() {
      return linkId;
    }
};

class LinkAccessController {
  private:
    Receiver& myReceiver;
    int numOfAvailableLinks;
    std::vector<Link> commsLinks;
    std::mutex LAC_mu;

  public:
    LinkAccessController(Receiver& r)
    : myReceiver(r), numOfAvailableLinks(NUM_OF_LINKS)
    {
      for (int i = 0; i < NUM_OF_LINKS; i++) {
        commsLinks.push_back(Link(myReceiver, i));
      }
    }

    //Find a free link and return it, block if none free
    Link requestLink() {
      int linkNum = 0;

      bool found = false;

      std::unique_lock<std::mutex> locker(LAC_mu);

      cout << "Thread " << getThreadId() << " requested a link" << endl;

      while (!found) {
        for (int i = 0; i < NUM_OF_LINKS; i++) {
          if (!commsLinks[i].isInUse()) {
            linkNum = commsLinks[i].getLinkId();

            commsLinks[i].setInUse(true);

            found = true;

            break;
          }
        }

        if (!found) {
          cout << "Thread " << getThreadId() << " is about to be suspended until a link is free" << endl;

          linkCond.wait(locker);
        }
      }

      cout << "Thread " << getThreadId() << " was given link " << commsLinks[linkNum].getLinkId() << endl;

      return commsLinks[linkNum];
    }

    //Release a comms link
    void releaseLink(Link& releasedLink) {
      std::unique_lock<std::mutex> locker(LAC_mu);

      int id = releasedLink.getLinkId();

      commsLinks[id].setInUse(false);

      linkCond.notify_all();

      cout << "Thread " << getThreadId() << " released link " << releasedLink.getLinkId() << endl;
    }
};

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

      adcCond.notify_all();
    }
};

class ADC {
  private:
    Lock theADCLock; //Lock instance
    int sampleChannel; //Current sample channel
    std::vector<AdcInputChannel>& adcChannels; //Reference to channels vector
    std::mutex ADC_mu; //ADC mutex
    
  public:
    //constructor: initialises A vector of ADCInputChannels passed in by reference:
    ADC(std::vector<AdcInputChannel>& channels)
    : adcChannels(channels),
      theADCLock(Lock()) 
    {}

    //Attempts to lock the ADC then sets sampleChannel
    void requestADC(int channel) {
      std::unique_lock<std::mutex> locker(ADC_mu);
      cout << "Thread " << getThreadId() << " requested ADC" << endl;

      while (!theADCLock.lock()) {
        cout << "Thread " << getThreadId() << " is about to be suspended until the ADC is free" << endl;
        adcCond.wait(locker);
      }

      cout << "Thread " << getThreadId() << " locked ADC" << endl;

      sampleChannel = channel;
    }

    //Returns sample from ADC
    double sampleADC() {
      std::unique_lock<std::mutex> locker(ADC_mu);

      double sample = adcChannels[sampleChannel].getCurrentSample();

      cout << "Thread " << getThreadId() << " got ADC sample value of: " << sample << endl;

      return sample;
    }

    //Unlocks ADC
    void releaseADC() {
      std::unique_lock<std::mutex> locker(ADC_mu);

      theADCLock.unlock();

      cout << "Thread " << getThreadId() << " released ADC" << endl;
    }

};

//Delays thread for between 0.1 and 0.5 seconds
void delayRandom() {
  int millis = dis(gen);

  std::this_thread::sleep_for (std::chrono::milliseconds(millis));
}

//run function executed by each thread
void run(ADC& theADC, LinkAccessController& theLAC, int id) {
  addThreadId(id);

  double sampleBlock[DATA_BLOCK_SIZE] = {0.0}; //init all elements to 0

  for (int i = 0; i < DATA_BLOCK_SIZE; i++) {
    theADC.requestADC(id);


    sampleBlock[i] = theADC.sampleADC();

    theADC.releaseADC();

    delayRandom();
  }

  Link link = theLAC.requestLink();

  delayRandom();

  link.writeToDataLink(id, sampleBlock);

  theLAC.releaseLink(link);

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

  //Receiver Instance
  Receiver theReceiver;

  //LAC Instance
  LinkAccessController theLAC(theReceiver);


  //Array of threads
  std::thread the_threads[MAX_NUM_OF_THREADS];
  for (int i = 0; i < MAX_NUM_OF_THREADS; i++) {
    // Launch threads
    cout << "launching thread " << i << endl;
    the_threads[i] = std::thread(run, std::ref(theAdc), std::ref(theLAC), i);
  }

  //Join the threads with the main thread
  for (int i = 0; i < MAX_NUM_OF_THREADS; i++) {
    the_threads[i].join();
  }

  //Print out all the receiver data
  theReceiver.printBlocks();

  cout << "All threads terminated." << endl;

  return 0;
}
