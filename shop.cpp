#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "shop.h"
using namespace std;

/**
 * constructor
 */
Shop::Shop(int nBarbers, int nChairs) {
    init(nBarbers, nChairs);
}

/**
 * constructor
 */
Shop::Shop() {
    init(DEFAULT_BARBER, DEFAULT_CHAIR);
}

/**
 * constructor helper
 */
void Shop::init(int nBarbers, int nChairs) {
    totalBarber = nBarbers > 1 ? nBarbers : 1;
    totalChair = nChairs > 0 ? nChairs : 0;
    nDropsOff = 0;      
    turn = 0;
    isOpen = true;    // shop is open
    if (pthread_mutex_init(&myLock, NULL) < 0)   {  // initialize mutex
        fprintf(stdout, "mutex initialze error");
        return;
    }

    // initialize conds
    waitingCalled = new pthread_cond_t[totalChair + 1];
    customerReady = new pthread_cond_t[totalBarber];
    serviceDone = new pthread_cond_t[totalBarber];
    customerLeave = new pthread_cond_t[totalBarber];

    waitingChairs = new int[totalChair + 1]; // in case totalChair == 0
    for (int i = 0; i < totalChair + 1; i++) {
        pthread_cond_init(waitingCalled + i, NULL);  // initialize cond
        waitingChairs[i] = -1; // not occupied
    }

    serviceChairs = new int[totalBarber];   // same number of service chairs with totalBarber
    atService = new bool[totalBarber];    // indicating whether service is done or not
    inShop = new bool[totalBarber];  // indicating whether the customer has left shop or not
    for (int i = 0; i < totalBarber; i++) {
        serviceChairs[i] = 0 - i; // Barber is sleeping on this chair
        atService[i] = false;     // not at service
        inShop[i] = false;
        pthread_cond_init(customerReady + i, NULL);
        pthread_cond_init(serviceDone + i, NULL);
        pthread_cond_init(customerLeave + i, NULL);
    }

}

/**
 * destructor
 */
Shop::~Shop() {
    delete [] waitingChairs;
    delete [] serviceChairs;
    delete [] atService;
    delete [] waitingCalled;
    delete [] customerReady;
    delete [] serviceDone;
}

/**
 * visitShop: when no waiting chairs and all barbers are busy or all waiting chairs are occupied, customer leave and increase nDropOff.
 * whereas when there are waiting chairs available, customerId take a waiting chair and wait to be called by a barber then leave waiting
 * chair for service chair or go directly pick a barber, before return the barberId, customerReady signal was sent to barber threads;
 */
int Shop::visitShop(int customerId) {
    pthread_mutex_lock(&myLock);
    int myBarber = -1;   

    if ( availableWaitingChair() == 0 && availableBarber() == 0 ) {  // no available barber and waiting chair, leave.
        fprintf(stdout, "Customer[%d] leaves shop because of no waiting seats available\n", customerId);
        nDropsOff++;
    } else {
        if ( availableBarber() == 0 ) {       // no available barber, take a waiting chair
            // take a seat and wait
            int mySeat = takeWaitingChair(customerId);
            fprintf(stdout, "Customer[%d] takes a waiting seat, # waiting seat available = %d\n", customerId, availableWaitingChair() );
            while ( findMyBarber(customerId) == -1 ) {  // waiting to be signaled by a barber thread
                pthread_cond_wait(waitingCalled + mySeat, &myLock);
            }
            myBarber = findMyBarber(customerId);  // find the barber calling
            leaveWaitingChair(customerId, mySeat);  // leave waiting chair 
            fprintf(stdout, "Customer[%d] moves to service chair[%d], # waiting seat available = %d\n", customerId, myBarber, availableWaitingChair() );
        } else {
            // go to service chair and wake up barber
            myBarber = pickMyBarber(customerId);  // pick a sleeping barber
            fprintf(stdout, "Customer[%d] wakes up barber[-%d]\n", customerId, myBarber);
        }
        atService[myBarber] = true;  // barber ready to start service
        pthread_cond_signal(customerReady + myBarber);  // send to the calling barber thread that customerId was ready 
    }
    pthread_mutex_unlock(&myLock);
    return myBarber;
}

/**
 * availableWaitingChair(); return number of available waiting chairs
 */
int Shop::availableWaitingChair() {
    int avail = 0;
    for (int i = 0; i < totalChair; i++) {
        if ( waitingChairs[i] == -1 ) {
            avail++;
        }
    }
    return avail;
}

/**
 * availableBarber(): return number of available barbers
 */
int Shop::availableBarber() {
    int avail = 0;
    for (int i = 0; i < totalBarber; i++) {
        if ( serviceChairs[i] <= 0 ) {
            avail++;
        }
    }
    return avail;
}

/**
 * takeWaitingChair(): find a waiting chair for customerId from turn in the array of waitingChairs
 * if no seats available, -1 was returned.
 */
int Shop::takeWaitingChair(int customerId) {
    int mySeat = -1;
    for (int i = 0; i < totalChair; i++) {
        mySeat = (i + turn) % totalChair; // look for the waiting chair starting from the turn
        if (waitingChairs[mySeat] == -1) {
            waitingChairs[mySeat] = customerId;  // put customerId in the waiting chair
            break;
        }
    }
    if (mySeat == -1) {
        fprintf(stdout, "ERROR!!!!!! No seat available for customer[%d]\n", customerId);
    }
    return mySeat;
}

/**
 * findMyBarber(): once customer in the waiting room was signaled and its index was return to the
 * calling barber, the customerId was then stored in the serviceChairs array, the customer would 
 * find its calling barber by checking through the serviceChairs array, if not -1 would returned.
 */
int Shop::findMyBarber(int customerId) {
    int myBarber = -1;
    for (int i = 0; i < totalBarber; i++) {
        if (serviceChairs[i] == customerId) { // look for index which customerId was stored
            myBarber = i;
            break;
        }
    }
    return myBarber;
}

/**
 * pickMyBarber(): find a sleeping barber for customerId, else if, -1 would be returned.
 */
int Shop::pickMyBarber(int customerId) {
    int myBarber = -1;
    for (int i = 0; i < totalBarber; i++) {
        if (serviceChairs[i] <= 0) {   // barber is sleeping
            serviceChairs[i] = customerId; // store customerId in serviceChair
            myBarber = i;     // keep index of the barber
            break;
        }
    }
    if (myBarber == -1) {
        fprintf(stdout, "ERROR!!!!!! No barber available for customer[%d]\n", customerId);
    }
    return myBarber;
}

/**
 * leaveWaitingChair(): set waitingChairs[seat] available to indicate customer is left for a service
 */
void Shop::leaveWaitingChair(int customerId, int seat) {
    if ( waitingChairs[seat] != customerId ) {   
        fprintf(stdout, "ERROR!!!!!! Customer[%d] is not sitting on the waiting chair #%d\n", customerId, seat);
    } else {
        waitingChairs[seat] = -1;
    }
}

/**
 * leaveShop(): customerId wait for barberId to be done with hair-cut, say bye to barberId and leave shop
 */
void Shop::leaveShop(int customerId, int barberId) {
    pthread_mutex_lock(&myLock);
    fprintf(stdout, "Customer[%d] is waiting for barber[-%d] to be done with hair-cut\n", customerId, barberId);
    while (atService[barberId]) {     // service is not done
        pthread_cond_wait(serviceDone + barberId, &myLock); // wait barber to be done with hair-cut
    }
    fprintf(stdout, "Customer[%d] pays barber[-%d] and leaves shop\n", customerId, barberId);  // customer left
    inShop[barberId] = false;
    pthread_cond_signal(customerLeave + barberId);
    pthread_mutex_unlock(&myLock);
}

/**
 * helloCustomer(): a barber thread is created and look for customer to be served, if no customer, go to sleep
 * called a customer when there is by sending a signal to the first waiting customer in array.
 */
void Shop::helloCustomer(int barberId) {
    pthread_mutex_lock(&myLock);
    if (availableWaitingChair() == totalChair) {  
        // no customer is wating, go to sleep
        fprintf(stdout, "Barber[-%d] goes to sleep because of no customer\n", barberId);
        serviceChairs[barberId] = 0 - barberId;
    } else {
        // call a customer
        int myCustomer = pickMyCustomer(barberId);
        serviceChairs[barberId] = waitingChairs[myCustomer];   // store customerId in serviceChairs
        fprintf(stdout, "Barber[-%d] calls for customer[%d]\n", barberId, serviceChairs[barberId]);
        pthread_cond_signal(waitingCalled + myCustomer);   // signal waiting customer in waiting chairs
    }
    while (!atService[barberId]) {    // barberId not ready to start service
        pthread_cond_wait(customerReady + barberId, &myLock);  // wait customer to be ready for a service
    }
    if (isOpen) {  
        inShop[barberId] = true;
        fprintf(stdout, "Barber[-%d] starts hair-cut for customer[%d]\n", barberId, serviceChairs[barberId]);
    } 
    pthread_mutex_unlock(&myLock);
}

/**
 * pickMyCustomer(): when a barber done with a service and the customer served left, the barber would called
 * another waiting customer to come in. the method would find the first waiting customer from turn in the array
 * and return the index, if not found, -1 would be returned.
 */
int Shop::pickMyCustomer(int barberId) {
    int myCustomer = -1;
    if (waitingChairs[turn] == -1) {  // no waiting customer in array
        fprintf(stdout, "ERROR!!!!!! No customer is wating, said barber[-%d]\n", barberId);
    } else {
        myCustomer = turn;        // serve customer from turn in array
        turn = (turn + 1) % totalChair; // turn to next waiting customer
    }
    return myCustomer;
}

/**
 * byeCustomer(): barberId done with hair-cut, signal the customerId at the service chait to leave.
 * and then barberId could be available for another hair-cut service.
 */
void Shop::byeCustomer(int barberId) {
    pthread_mutex_lock(&myLock);
    fprintf(stdout, "Barber[-%d] says he has done hair-cut for customer[%d]\n", barberId, serviceChairs[barberId]);
    atService[barberId] = false;  // barberId is not at service
    pthread_cond_signal(serviceDone + barberId);   // signal customer at service chair service is done
    while (isOpen && inShop[barberId]) {
        pthread_cond_wait(customerLeave + barberId, &myLock);
    }
    pthread_mutex_unlock(&myLock);
}

/**
 * close(): barbers would not serve any customer.
 */
void Shop::close() {
    isOpen = false;
}
