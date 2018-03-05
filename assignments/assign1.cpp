#include <iostream>
#include <queue>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

//Global Variables
// • PMAX: Total number of products to be generated by all producer threads
// • QMAX: Size of the queue to store products for both producer and consumer threads (0 for
// unlimited queue size)
// • QUANTUM: Value of quantum used for round-robin scheduling.
int PMAX, QMAX, QUANTUM;

// • NPROD: Total Number of Products produced
// • NCONS: Total Number of Products cosumed
int NPROD = 0, NCONS = 0;
bool PDONE, CDONE;

// • queue_mutex: Mutex for the QUEUE variable
// • condp: condition for producers
// • condc: condition for consumers
pthread_mutex_t queue_mutex;
pthread_cond_t condp = PTHREAD_COND_INITIALIZER, condc = PTHREAD_COND_INITIALIZER;

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
        Product () : id(0), timestamp(0), life(0) {}
        Product (int id) : id(id), timestamp(clock()), life(random() % 1024) {
            std::cout << "Life: " << this->life << std::endl;;
        }
        int get_life(){
            return life;
        }
        void set_life(int life){
            this->life = life;
        }
        void consume(){
            for(int i = 0; i < this->life; i++) fb(10);
            std::cout << "ProductID: " << this->id << std::endl;;
        }
        void consume(int quantum){
            for(int i = 0; i < this->life; i++){
                fb(10);
            }
            this->life -= quantum;
            std::cout << "ProductID: " << this->id << std::endl;;
        }
};

// • QUEUE: Queue that holds all products
std::queue<Product> QUEUE;

void *producer(void *id){
    int int_id = *(int*)id;
    while(!PDONE){
        //Insert Product 
        pthread_mutex_lock(&queue_mutex);
        while(QUEUE.size() >= QMAX || QMAX == 0) pthread_cond_wait(&condp, &queue_mutex);
        QUEUE.push(Product(NPROD));
        NPROD++;
        if(NPROD == PMAX) PDONE = true;
        pthread_cond_signal(&condc);
        pthread_mutex_unlock(&queue_mutex);
        //Sleep 100 milliseconds
        usleep(100000);
    }
    pthread_exit(NULL);
}

void *consumer(void *id){
    int int_id = *(int*)id;
    Product prod;
    //Consume Product
    while(!CDONE){
        pthread_mutex_lock(&queue_mutex);
        while(QUEUE.size() < 1) pthread_cond_wait(&condc, &queue_mutex);
        prod = QUEUE.front();
        QUEUE.pop();
        NCONS++;
        if(NCONS == PMAX) CDONE = true;
        pthread_cond_signal(&condp);
        pthread_mutex_unlock(&queue_mutex);
        prod.consume();
        //Sleep 100 milliseconds
        usleep(100000);
    }
    pthread_exit(NULL);
}

int main(int argc, char* argv[]){
    // for (int i = 0; i < argc; ++i)
    //     std::cout << argv[i] << std::endl;

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

    int nump = atoi(argv[1]), numc = atoi(argv[2]);
    PMAX = atoi(argv[3]);
    QMAX = atoi(argv[4]);
    int algo = atoi(argv[5]);
    QUANTUM = atoi(argv[6]);
    int seed = atoi(argv[7]);
    
    // Initialize Seed
    
    srandom(seed);

    // Initialize Mutexes/Declare threads & ids

    pthread_mutex_init(&queue_mutex, NULL);
    pthread_t prod_thread[nump], consmr_thread[numc];
    int prodID[nump], consmrID[numc];
    
    // Create prod threads

    for (int i=0;i<nump;i++){
        prodID[i] = i;
        pthread_create(&prod_thread[i], NULL, producer, &prodID[i]);
    }

    // create consumer threads

    for (int i=0;i<numc;i++){
        consmrID[i] = i;
        pthread_create(&consmr_thread[i], NULL, consumer, &consmrID[i]);
    }

    
    // join consumer/producer threads

    for (int i=0;i<nump;i++)
        pthread_join(prod_thread[i],NULL);

    for (int i=0;i<numc;i++)
        pthread_join(consmr_thread[i],NULL);

    // delete everything

    pthread_mutex_destroy(&queue_mutex);
    pthread_exit(0);
    return 0;
}
