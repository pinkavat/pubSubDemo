#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* pubSub.c
 *  
 * tester code for a publish/subscription event queue with multithreading
 *
 * written November 2019 (?) by Thomas Pinkava
 */

#define THREAD_COUNT 4  // The number of execution threads to employ
#define MAX_PUBLISHABLE_EVENTS 512 // Per tick, a primitive guard against infinite recursions

#define EVENT_TYPES 26  // The total number of event types (arbitrary)


// ========== SUBSCRIPTION DEFINITIONS ==========
// A node in a list of event subscribers
typedef struct subscriberNode{
    void (*subscriberFunction)(void *); // The subscriber function
    struct subscriberNode *next;        // Linked List Link
} subscriberNode_t;

// A set of all event subscribers, ordered in a map by event type
typedef struct subscriberSet{
    subscriberNode_t * map[EVENT_TYPES]; // Each item of map points to a list of subscribers
                                         // which respond to that event
} subscriberSet_t;

// Initialize a subscriber set (simply nulls its buckets)
void initSubscriberSet(subscriberSet_t *sSet){
    for(int i = 0; i < EVENT_TYPES; i++){
        sSet->map[i] = NULL;
    }
}

// Deallocate a subscriber set
void destroySubscriberSet(subscriberSet_t *sSet){
    for(int i = 0; i < EVENT_TYPES; i++){
        // Linked-List free all nodes in the ith bucket
        subscriberNode_t *temp;
        while(sSet->map[i] != NULL){
            temp = sSet->map[i];
            sSet->map[i] = temp->next;
            free(temp);
        }
    }
}

// Add a subscriber to the subscriber set
void subscribe(subscriberSet_t *sSet, unsigned int eventType, void (*subscriberFunction)(void *)){
    subscriberNode_t *newSub = (subscriberNode_t *)malloc(sizeof(subscriberNode_t)); // Perhaps add error checking
    newSub->subscriberFunction = subscriberFunction;
    newSub->next = sSet->map[eventType];
    sSet->map[eventType] = newSub;
}



// ========== EVENT DEFINITIONS ==========
// An event
typedef struct eventNode{
    unsigned int type;      // The event's type
    void *data;             // The event-type-specific data associated with this event
    struct eventNode *next; // Linked List Link
} event_t;

// The event stack for live events
typedef struct eventStack{
    event_t *head;
    unsigned int count;
    pthread_mutex_t lock;
} eventStack_t;

// Initialize an event stack
void eventStack_init(eventStack_t *eventStack){
    eventStack->head = NULL;
    eventStack->count = 0;
    pthread_mutex_init(&(eventStack->lock), NULL);
}

// Prepend a new event to an event stack
void publish(eventStack_t *eventStack, unsigned int eventType, void *eventData){
    // Acquire the stack lock
    if(pthread_mutex_lock(&(eventStack->lock))){
        perror("Locking failed");
        exit(2);
    }
    // Ensure that no more than the maximum Events are published
    if(eventStack->count++ > MAX_PUBLISHABLE_EVENTS){
        // TODO: Standardize error reporting over all exposed TPECS functionality
        fprintf(stderr, "Event of type %u could not be published (tick publishing limit reached)\n", eventType);
    } else {
        // Allocate, initialize, and push a new event
        event_t *newEvent = (event_t *)malloc(sizeof(event_t)); // Perhaps add error checking
        newEvent->next = eventStack->head;
        newEvent->type = eventType;
        newEvent->data = eventData;
        eventStack->head = newEvent;
    }
    // Relinquish the stack lock
    if(pthread_mutex_unlock(&(eventStack->lock))){
        perror("Unlocking failed");
        exit(2);
    }
}

// Remove and return the first event from an event stack
event_t *popEvent(eventStack_t *eventStack){
    // Acquire the stack lock
    if(pthread_mutex_lock(&(eventStack->lock))){
        perror("Locking failed");
        exit(2);
    }
    // Pop the head event if one exists
    event_t *doomedEvent = eventStack->head;
    if(doomedEvent != NULL){
        eventStack->head = doomedEvent->next;
    }
    // Relinquish the stack lock
    if(pthread_mutex_unlock(&(eventStack->lock))){
        perror("Unlocking failed");
        exit(2);
    }
    return doomedEvent;
}



// ========== MULTITHREADED EVENT SUBSCRIBER EXECUTION ==========
// Args for the below function
typedef struct executorThreadArgs {
    eventStack_t *eventStack;
    subscriberSet_t *sSet;
} executorThreadArgs_t;

// Thread function, repeatedly executes all subscribers to events taken from the event stack
void *eventExecutor(void *p){
    // Cast the argument
    executorThreadArgs_t * args = (executorThreadArgs_t *)p;

    // Fetch events from the stack until there are no more events
    event_t *currentEvent;
    do {
               
        // Pop a fresh event from the stack
        currentEvent = popEvent(args->eventStack);

        if (currentEvent != NULL){
            if(currentEvent->type >= EVENT_TYPES){
                // Event falls outside the range of valid events
                // TODO: Standardize errors over all TPECS functions
                fprintf(stderr, "Event of type %u found (not in valid range 0-%u)\n", currentEvent->type, EVENT_TYPES - 1);
            } else {
                // Invoke all subscribers to this event
                subscriberNode_t *currentSub = args->sSet->map[currentEvent->type];
                while(currentSub != NULL){
                    // Run the subscribed function, handing down the event data
                    currentSub->subscriberFunction(currentEvent->data);
                    currentSub = currentSub->next;
                }
            }

            // Deallocate the event's data (N.B. relies on freeing NULL having no effect)
            free(currentEvent->data);
            // Deallocate the event
            free(currentEvent);
        }
        
    } while(currentEvent != NULL);
    return NULL;
}


void runAllEvents(int threadCount, eventStack_t *eventStack, subscriberSet_t *sSet){

    // Reset event counter
    eventStack->count = 0;

    // Reserve space for threads and args
    pthread_t threads[threadCount];
    executorThreadArgs_t args[threadCount];

    // Create and launch threads
    for(int i = 0; i < threadCount; i++){
        args[i].eventStack = eventStack;
        args[i].sSet = sSet;
        if(pthread_create(&threads[i], NULL, eventExecutor, &args[i])){
            perror("Failed to create pthread");
            exit(2);
        }
    }

    // Finally join with threads
    for(int i = 0; i < threadCount; i++){
        if(pthread_join(threads[i], NULL)){ // Nothing returned
            perror("Failed to join pthread");
            exit(2);
        }
    } 

}






// ========== TEST CODE ==========


// Dummy Globals (in the real use case, this would be part of the root World struct to protect namespace)
// We would also hand the world struct down to the subscriber functions
subscriberSet_t gSSet;
eventStack_t gEStack;


// Dummy Subscribers
void testSubOne(void *arg){
    printf("This is a '0'-type subscriber!\n");
}
void testSubTwo(void *arg){
    printf("This is a '1'-type subscriber, and it generates a '0'-type event!\n");
    publish(&gEStack, 0, NULL);
}

void testSubThree(void *arg){
    if(arg == NULL){
        printf("This is a '2'-type subscriber with no data\n");
    } else {
        printf("This is a '2'-type subscriber; here's the event's datum: %d\n", *(int *)arg);
    }
}
void testSubFour(void *arg){
    printf("This is a '3'-type subscriber, and it generates '2'-type events with a datum of 32!\n");
    int *p = (int *)malloc(sizeof(int));
    *p = 32;
    publish(&gEStack, 2, p);
}
void testSubFive(void *arg){
    printf("This is a '4'-type subscriber, and it generates '2'-type events with a datum of 64!\n");
    int *p = (int *)malloc(sizeof(int));
    *p = 64;
    publish(&gEStack, 2, p);
}

void testSubRecursion(void *arg){
    printf("This is a '5'-type subscriber, and it generates another '5'-type event!\n");
    publish(&gEStack, 5, NULL);
}



// Test Driver
int main(void){

    // Init sample set of subscribers
    initSubscriberSet(&gSSet);

    // Get some subscribers (this will be done at start of actual use-case app)
    subscribe(&gSSet, 0, testSubOne);
    subscribe(&gSSet, 1, testSubTwo);
    subscribe(&gSSet, 2, testSubThree);
    subscribe(&gSSet, 3, testSubFour);
    subscribe(&gSSet, 4, testSubFive);
    subscribe(&gSSet, 5, testSubRecursion);
    subscribe(&gSSet, 5, testSubRecursion); // Double the recursion!

    // Init sample starting stack of events
    eventStack_init(&gEStack);

    // add events to the stack from user
    char controlChar;
    while((controlChar = getchar()) != '\n'){
        publish(&gEStack, controlChar - 'a', NULL);
    }

    // Run the constructed stack
    runAllEvents(THREAD_COUNT, &gEStack, &gSSet);
    
    // Clean up
    destroySubscriberSet(&gSSet);

}
