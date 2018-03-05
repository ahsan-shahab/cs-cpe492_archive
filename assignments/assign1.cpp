#include <iostream>
#include <queue>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

//If this value is true, all debug statements are printed.
bool DEBUG = false, METRIC = true;

// Global Constants
// • PMAX: Total number of products to be generated by all producer threads
// • QMAX: Size of the queue to store products for both producer and consumer threads (0 for
// unlimited queue size)
// • QNTM: Value of quantum used for round-robin scheduling.
int PMAX, QMAX, QNTM;

// Global Variables
// • NPROD: Total Number of Products produced
// • NCONS: Total Number of Products cosumed
// • UNLIM: QUEUE has no limit on size
int NPROD = 0, NCONS = 0;
bool PDONE = false, CDONE = false, UNLIM = false, RDRB = false;

// Global Metrics
// • TIMET: Total time
// • MINTA: Minimum Turnaround Time
// • MAXTA: Maximum Turnaround Time
// • AVGTA: Average Turnaround Time
// • MINW: Minimum Wait
// • MAXW: Maximum Wait
// • AVGW: Average Wait
// • PRODT: Producer Throughput
// • CNSMRT: Consumer Throughput
float TIMET=0;
clock_t PRODT, CNSMRT;

// Global pthread variables
// • queue_mutex: Mutex for the QUEUE variable
// • condp: condition for producers
// • condc: condition for consumers
pthread_mutex_t queue_mutex;
pthread_cond_t condp = PTHREAD_COND_INITIALIZER, condc = PTHREAD_COND_INITIALIZER;

// Product class that holds a product ID, timestamp, and life
// Consume methods executes fb N times depending on algo
class Product{
    private:
        int id;
        clock_t timestamp;
        int life;
        int fb(int n){
           if (n <= 1)
              return n;
           return fb(n-1) + fb(n-2);
        }

    public:
        Product (int id) : id(id), timestamp(clock()), life(rand() % 1024) {
            if(DEBUG) std::cout << "+Product ID (produced): " << this->id << std::endl;
        }
        int get_id(){
            return this->id;
        }
        // Runs fibo sequence [N=life] times
        void consume(){
            for(int i = 0; i < this->life; i++) fb(10);
            if(DEBUG) std::cout << "-ProductID (consumed): " << this->id << std::endl;
        }
        // Round robin consume. If ready to be removed from queue, function returns true
        bool consume(int quantum){
            if(this->life > quantum){
                this->life -= quantum;
                if(DEBUG) std::cout << "<3Life reduced: " << this->life << std::endl;
                for(int i = 0; i < quantum; i++) fb(10);
                if(DEBUG) std::cout << "-ProductID (consumed): " << this->id << std::endl;
                return false;
            }else{
                consume();
                if(DEBUG) std::cout << "<3Life Drained" << std::endl;
                if(DEBUG) std::cout << "-ProductID (consumed): " << this->id << std::endl;
                return true;
            }
        }
};

// • QUEUE: Queue that holds all products
std::queue<Product> QUEUE;

void *producer(void *id){
    int int_id = *(int*)id; // The unique producer thread id
    if(DEBUG) std::cout << "!Producer Thread ID: " << int_id << std::endl;

    // PDONE is true when PMAX has been reached
    while(!PDONE){
        if(DEBUG) std::cout << "$PRODUCER LOCK REQUESTED ID: " << int_id << std::endl;
        pthread_mutex_lock(&queue_mutex); 

        // Checks if Queue limit is reached or skips if the queue size has no limit
        while(QUEUE.size() >= QMAX && !UNLIM) {
            if(DEBUG) std::cout << "...Producer Thread Waiting ID: " << int_id << std::endl;
            pthread_cond_wait(&condp, &queue_mutex);    // Waits for consumer to consume
            if(DEBUG) std::cout << "...Producer Thread Finished Waiting ID: " << int_id << std::endl;
        }
        if(DEBUG) std::cout << "$PRODUCER LOCK RECEVIED ID: " << int_id << std::endl;

        // If enough products have been made, quit.
        if(NPROD == PMAX - 1){
            PRODT = clock() - PRODT;
            PDONE = true;
            pthread_cond_broadcast(&condp); // Lets all waiting producers continue
            pthread_mutex_unlock(&queue_mutex);
            if(DEBUG) std::cout << "$PRODUCER UNLOCKED ID: " << int_id << std::endl;
            break;
	    }

        QUEUE.push(Product(NPROD));
        if(DEBUG) std::cout << "^Queue Size (produced): " << QUEUE.size() << std::endl;
        std::cout << "Producer " << int_id << " has produced product " << NPROD << std::endl;
	    ++NPROD;
        if(DEBUG) std::cout << "*Number of Products Produced: " << NPROD << std::endl;
        pthread_cond_signal(&condc);    // Lets a single waiting consumer continue
        pthread_mutex_unlock(&queue_mutex); 
        if(DEBUG) std::cout << "$PRODUCER UNLOCKED ID: " << int_id << std::endl;
        usleep(100000); // Sleep 100 milliseconds (100000 microseconds)
    }
    if(DEBUG) std::cout << "!Producer ID: " << int_id << " Exited" << std::endl;
    pthread_exit(NULL);
}

void *consumer(void *id){
    int int_id = *(int*)id; // The unique consumer thread ID
    if(DEBUG) std::cout << "!Consumer Thread ID: " << int_id << std::endl;

    // CDONE is true when PMAX has been reached
    while(!CDONE){
        if(DEBUG) std::cout << "$CONSUMER LOCK REQUEST ID: " << int_id << std::endl;
        pthread_mutex_lock(&queue_mutex);

        // Checks if there are any products or continue if all possible products have been consumed 
        while(QUEUE.size() < 1 && NCONS < PMAX - 1) {
            if(DEBUG) std::cout << "...Consumer Thread Waiting ID: " << int_id << std::endl;
            pthread_cond_wait(&condc, &queue_mutex);
            if(DEBUG) std::cout << "...Consumer Thread Finished Waiting ID: " << int_id << std::endl;
        }
        if(DEBUG) std::cout << "$CONSUMER LOCK RECEVIED ID: " << int_id << std::endl; 

        // If all possible products have been consumed, exit.
        if(NCONS == PMAX - 1){
            CNSMRT = clock() - CNSMRT;
            CDONE = true;
            pthread_cond_broadcast(&condc); // Lets all waiting consumers continue
            pthread_mutex_unlock(&queue_mutex);
            if(DEBUG) std::cout << "$CONSUMER UNLOCKED ID: " << int_id << std::endl;
            break;
	    }
        
        // If Round Robin and the item hasn't reached the end of its life, push it back
        if(RDRB){
            Product prod = QUEUE.front();
            QUEUE.pop();
            if(!prod.consume(QNTM)) QUEUE.push(prod);
            else {
                ++NCONS;
                std::cout << "Consumer " << int_id << " has consumed product " << prod.get_id() << std::endl;
            }
        }
        else{
            Product prod = QUEUE.front();
	        ++NCONS;
            prod.consume();
            QUEUE.pop();
            std::cout << "Consumer " << int_id << " has consumed product " << prod.get_id() << std::endl;
        }

        if(DEBUG) std::cout << "*Number of Products Consumed: " << NCONS << std::endl;
        if(DEBUG) std::cout << "vQueue Size (consumed): " << QUEUE.size() << std::endl;
        pthread_cond_signal(&condp);    // Lets a single waiting producer continue
        pthread_mutex_unlock(&queue_mutex); 
        if(DEBUG) std::cout << "$CONSUMER UNLOCKED ID: " << int_id << std::endl;
        usleep(100000); //Sleep 100 milliseconds (100000 microseconds)
    }
    if(DEBUG) std::cout << "!Consumer ID: " << int_id << " Exited" << std::endl;
    pthread_exit(NULL);
}

int main(int argc, char* argv[]){

    clock_t total_time = clock();
    if(argc != 8) {
        std::cout << "Usage: ./assign1 P1 P2 P3 P4 P5 P6 P7\n"
                  << "P1: Number of producer threads\n"
                  << "P2: Number of consumer threads\n"
                  << "P3: Total number of products to be generated by all producer threads\n"
                  << "P4: Size of the queue to store products for both producer and consumer threads (0 for unlimited queue size)\n"
                  << "P5: 0 or 1 for type of scheduling algorithm: 0 for First-Come-First-Serve, and 1 for Round-Robin\n"
                  << "P6: Value of quantum used for round-robin scheduling\n"
                  << "P7: Seed for a random number generator"
                  << std::endl;
        return -1;
    }

    int nump = atoi(argv[1]);   // Number of Producers
    if(nump < 1) std::cout << "P1 should be at least 1" << std::endl;
    int numc = atoi(argv[2]);   // Number of Consumers
    if(numc < 1) std::cout << "P2 should be at least 1" << std::endl;
    PMAX = atoi(argv[3]);       // Number of Products to be Produced
    if(PMAX < 1) std::cout << "P3 should be at least 1" << std::endl;
    QMAX = atoi(argv[4]);       // Queue Limit
    if(QMAX < 0) std::cout << "P4 should be at least 0" << std::endl;
    int algo = atoi(argv[5]);   // Algorithm: 0 for FIFS, 1 for Round-Robin
    if(algo != 0 && algo != 1) std::cout << "P5 should be 0 or 1" << std::endl;
    QNTM = atoi(argv[6]);       // Round Robin Quantum
    if(QNTM < 1 && QNTM > 1023) std::cout << "P6 should be at least 1 and no more than 1023" << std::endl;
    int seed = atoi(argv[7]);   // Seed for random value in Product life
    if(seed < 1) std::cout << "P7 should be larger than 1" << std::endl;

    // Set Seed/Initialize Mutexes/Declare threads & ids/Set UNLIM & RDRB

    srand(seed);
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_t prod_thread[nump], consmr_thread[numc];
    int prodID[nump], consmrID[numc];
    if(QMAX == 0) UNLIM = true;
    if(algo == 1) RDRB = true;
    
    // Create prod threads

    PRODT = clock();
    for (int i=0;i<nump;i++){
        prodID[i] = i;
        pthread_create(&prod_thread[i], NULL, producer, &prodID[i]);
    }

    // Create consumer threads

    CNSMRT = clock();
    for (int i=0;i<numc;i++){
        consmrID[i] = i;
        pthread_create(&consmr_thread[i], NULL, consumer, &consmrID[i]);
    }

    // Join consumer/producer threads

    for (int i=0;i<nump;i++)
        pthread_join(prod_thread[i],NULL);

    for (int i=0;i<numc;i++)
        pthread_join(consmr_thread[i],NULL);

    // Destroy everything

    pthread_mutex_destroy(&queue_mutex);
    pthread_cond_destroy(&condp);
    pthread_cond_destroy(&condc);

    // Print Metrics
    if(METRIC){
        std::cout << "_______________________________\n" << "[METRICS]" << std::endl;
        total_time = clock() - total_time;
        TIMET = ((float)total_time)/CLOCKS_PER_SEC;
        std::cout << "Total Time: " << TIMET << " seconds" << std::endl;
        std::cout << "Producer Throughput: " << (float)PRODT/CLOCKS_PER_SEC << std::endl;
        std::cout << "Consumer Throughput: " << (float)CNSMRT/CLOCKS_PER_SEC << std::endl;
        std::cout << "_______________________________\n" << std::endl;
    }

    pthread_exit(0);
    return 0;
}
