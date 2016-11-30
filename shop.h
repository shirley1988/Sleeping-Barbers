#ifndef SHOP_H
#define SHOP_H

#include <pthread.h>

#define DEFAULT_CHAIR 3
#define DEFAULT_BARBER 1


class Shop {
public:
    Shop(int nBarbers, int nChairs);
    Shop();
    ~Shop();

    int visitShop(int customerId);
    void leaveShop(int customerId, int barberId);
    void helloCustomer(int berberId);
    void byeCustomer(int berberId);
    int nDropsOff;
    void close();

private:
    int totalBarber;
    int totalChair;
    int turn;
    bool isOpen;

    pthread_mutex_t myLock;
    pthread_cond_t *waitingCalled; // length = totalChair
    pthread_cond_t *customerReady; // length = totalBarber
    pthread_cond_t *serviceDone; // length = totalBarber
    pthread_cond_t *customerLeave;


    int *waitingChairs; // length = totalChair, for waiting customers
    int *serviceChairs; // length = totalBarber,  for customers at service
    bool *atService;
    bool *inShop;

    void init(int nBarbers, int nChairs);
    int availableWaitingChair();
    int availableBarber();
    int takeWaitingChair(int customerId);
    void leaveWaitingChair(int customerId, int seat);
    int findMyBarber(int customerId);
    int pickMyBarber(int customerId);
    int pickMyCustomer(int barberId);
};

#endif
