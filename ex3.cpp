#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <queue>
#include <unistd.h>
#include <semaphore.h>
#include <mutex>
#include <pthread.h>


// ##### Producer-Consumer Problem #####


// Bounded Queue Class.
class BQ {
private:
    std::queue<std::string> myQueue;
    sem_t empty;
    sem_t full;
    std::mutex mtx;
public:
    BQ(int size)
    {
        sem_init(&empty, 0, size);
        sem_init(&full, 0, 0);
    }
    void enqueue(std::string &str) {
        sem_wait(&empty);
        mtx.lock();
        myQueue.push(str);
        mtx.unlock();
        sem_post(&full);
    }

    std::string dequeue() {
        std::string x;
        sem_wait(&full);
        mtx.lock();
        x = myQueue.front();
        myQueue.pop();
        mtx.unlock();
        sem_post(&empty);
        return x;
    }
    void destroy() {
        sem_destroy(&empty);
        sem_destroy(&full);
    }
};

// Unbounded Queue Class.
class UBQ {
private:
    std::queue<std::string> myQueue;
    sem_t full;
    std::mutex mtx;
public:
    UBQ()
    {
        sem_init(&full, 0, 0);
    }
    void enqueue(std::string &str) {
        mtx.lock();
        myQueue.push(str);
        mtx.unlock();
        sem_post(&full);
    }

    std::string dequeue() {
        std::string x;
        sem_wait(&full);
        mtx.lock();
        x = myQueue.front();
        myQueue.pop();
        mtx.unlock();
        return x;
    }
    void destroy() {
        sem_destroy(&full);
    }
};


// Array which save all queues sizes of each producer.
std::vector<int> sizesPQ;

// Array which save all items amounts of each producer.
std::vector<int> sizesNewsP;

// Size of editors bounded queue (with the screen manager).
int sizeEdQ;

// Array of Bounded queues for the producers (and dispatcher).
std::vector<BQ*> queuesP;

// 3 Unbounded queues for Dispatcher (and co-Editors).
UBQ* dispatcherQueues[3];

// Bounded queue for co-Editors(and screen manager).
BQ* edQueue;


// Read the data from configuration file.
void readConfig(char* path) {
    int indexLine = 0;
    std::fstream configFile;
    configFile.open(path, std::ios::in);
    if(configFile.is_open()) {
        std::string line;
        std::string lastLine;
        while (getline(configFile, line)) {
            if(!line.empty()) {
                indexLine++;
                lastLine = line;
                // For each producer the information is : id, queue size, items amount.
                if((indexLine % 3) == 2) {
                    sizesNewsP.push_back(stoi(line));
                } else if((indexLine % 3) == 0) {
                    sizesPQ.push_back(stoi(line));
                }
            }
        }
        // Last line is the size of co-editors queue.
        sizeEdQ = stoi(lastLine);
        configFile.close();
    }
}

// Create all queues by the data we read from the config file.
void initialize() {
    for(int i = 0; i < sizesPQ.size(); i++) {
        queuesP.push_back(new BQ(sizesPQ[i]));
    }
    dispatcherQueues[0] = new UBQ();
    dispatcherQueues[1] = new UBQ();
    dispatcherQueues[2] = new UBQ();
    edQueue = new BQ(sizeEdQ);
}

/*
 * Producer function run on thread and produce strings
 * into the queue of the producer with the id sent to the function,
 * and the dispatcher dequeue it.
*/
void* producer(void* j) {
    int i = *((int*)j);
    // Produce strings from 3 subjects.
    std::string types[3] = {"SPORTS", "NEWS", "WEATHER"};
    std::string indexProducer = std::to_string(i + 1);
    int count = 1;
    // Produce by the amount from the config file.
    while (count <= sizesNewsP[i]) {
        std::string item = "Producer " + indexProducer + " " + types[count%3] + " " + std::to_string(count);
        queuesP[i]->enqueue(item);
        count++;
    }
    std::string str = "DONE";
    queuesP[i]->enqueue(str);

    return ((void*)0);
}

/*
 * dispatcher function run on thread and dequeue strings
 * from the producers queues, filter it
 * and enqueue to the correct queue (by subject).
*/
void* dispatcher(void* j) {
    int count = 0;

    // Mark active queues.
    for(int i = 0; i < sizesPQ.size(); i++) {
        sizesPQ[i] = 1;
    }

    // Need get 'DONE' from each producer.
    while (count < queuesP.size()) {
        for (int i = 0; i < queuesP.size(); i++) {
            if(sizesPQ[i] == 1) {
                std::string item = queuesP[i]->dequeue();
                if (item == "DONE") {
                    sizesPQ[i] = 0;
                    count++;
                } else {
                    int start = item.rfind(" ");
                    int index = (stoi(item.substr(start)) - 1) % 3;
                    dispatcherQueues[index]->enqueue(item);
                }
            }
        }
    }

    // Send 'DONE' to each co-editor.
    std::string str = "DONE";
    dispatcherQueues[0]->enqueue(str);
    dispatcherQueues[1]->enqueue(str);
    dispatcherQueues[2]->enqueue(str);

    return ((void*)0);
}

/*
 * editor function run on thread and dequeue strings
 * from the dispatcher queue (by his id),
 * and enqueue to the unbounded queue of the co-editors (with screen manager).
*/
void* editor(void* j) {
    int i = *((int*)j);
    bool shouldStop = false;
    while (!shouldStop) {
        std::string item = dispatcherQueues[i]->dequeue();
        if (item == "DONE") {
            shouldStop = true;
        } else {
            sleep(0.1);
            edQueue->enqueue(item);
        }
    }
    std::string str = "DONE";
    edQueue->enqueue(str);

    return ((void*)0);
}

// screenManager function run on thread and print strings from the unbounded queue.
void* screenManager(void* j) {
    int count = 0;
    // Need get 'DONE' from each co-editor.
    while (count < 3) {
        std::string item = edQueue->dequeue();
        if(item == "DONE") {
            count++;
        } else {
            std::cout << item << std::endl;
        }
    }

    return ((void*)0);
}

// The function delete and free allocated memory and release resources.
void deleteFunction() {
    edQueue->destroy();
    delete(edQueue);
    edQueue = nullptr;

    for(int i = 0; i < 3; i++) {
        dispatcherQueues[i]->destroy();
        delete(dispatcherQueues[i]);
        dispatcherQueues[i] = nullptr;
    }

    for(int i = 0; i < queuesP.size(); i++) {
        queuesP[i]->destroy();
        delete(queuesP[i]);
        queuesP[i] = nullptr;
    }
}


// Use Option 2 of config file.
int main(int argc, char* argv[]) {
    readConfig(argv[1]);
    initialize();

    pthread_t threadsP[queuesP.size()];
    int numbersP[queuesP.size()];
    for(int i = 0; i < queuesP.size(); i++) {
        numbersP[i] = i;
    }

    pthread_t threadD;

    pthread_t threadsE[3];
    int numbersE[3];
    for(int i = 0; i < 3; i++) {
        numbersE[i] = i;
    }

    pthread_t threadScreen;
    void* retVal;

    // Create producers threads.
    for (int i = 0; i < queuesP.size(); i++) {
        pthread_create(&threadsP[i], nullptr, producer, (void*)&numbersP[i]);
    }

    // Create dispatcher thread.
    pthread_create(&threadD, nullptr, dispatcher, nullptr);

    // Create co-editors threads.
    pthread_create(&threadsE[0], nullptr, editor, (void*)&numbersE[0]);
    pthread_create(&threadsE[1], nullptr, editor, (void*)&numbersE[1]);
    pthread_create(&threadsE[2], nullptr, editor, (void*)&numbersE[2]);

    // Create screen manager thread.
    pthread_create(&threadScreen, nullptr, screenManager, nullptr);

    pthread_join(threadScreen, &retVal);
    std::cout << "DONE" << std::endl;
    deleteFunction();

    return 0;
}
