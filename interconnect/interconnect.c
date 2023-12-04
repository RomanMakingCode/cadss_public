#include <getopt.h>
#include <stdio.h>

#include <memory.h>
#include <interconnect.h>


typedef enum _bus_req_state
{
    NONE,
    QUEUED,
    TRANSFERING_CACHE,
    TRANSFERING_MEMORY,
    WAITING_CACHE,
    WAITING_MEMORY
} bus_req_state;

typedef struct _bus_req {
    bus_req_type brt;
    bus_req_state currentState;
    uint64_t addr;
    int procNum;
    uint8_t shared;
    uint8_t data;
    uint8_t dataAvail;
    int countDown;
    struct _bus_req* next;
} bus_req;


typedef struct _waitstr{
  bus_req_type brt;
  uint64_t addr; 
  int procnum;
  bus_req* request; 
} waitstr;

typedef struct _node{
    waitstr* data; 
    void* next;
} node; 

typedef struct _linkedlist{
  node* top;
} linkedlist; 



bus_req* pendingRequest = NULL;
bus_req** queuedRequests;
interconn* self;
coher* coherComp;
memory* memComp;

int CADSS_VERBOSE = 0;
int processorCount = 1;

static const char* req_state_map[] = {
    [NONE] = "None",
    [QUEUED] = "Queued",
    [TRANSFERING_CACHE] = "Cache-to-Cache Transfer",
    [TRANSFERING_MEMORY] = "Memory Transfer",
    [WAITING_CACHE] = "Waiting for Cache",
    [WAITING_MEMORY] = "Waiting for Memory",
};

static const char* req_type_map[]
    = {[NO_REQ] = "None", [BUSRD] = "BusRd",   [BUSWR] = "BusRdX",
       [DATA] = "Data",   [SHARED] = "Shared", [MEMORY] = "Memory"};

const int CACHE_DELAY = 10;
const int CACHE_TRANSFER = 10;

void registerCoher(coher* cc);
void busReq(bus_req_type brt, uint64_t addr, int procNum);
int busReqCacheTransfer(uint64_t addr, int procNum);
void printInterconnState(void);
void interconnNotifyState(void);

/**
 * We are going to implement a interconnect that is cross-bar based
 * Instead of being all to all with processors and memory modules
 * Ours will instead look at handling one request per memory address
 * 
 * This linked list data structure will hold all of the currently processes addresses
 * If the requested address is in this linked list, it gets saved in the pending queue 
*/
linkedlist* processing = NULL;
linkedlist* pending = NULL;

linkedlist* new_linkedlist(void){
  linkedlist* ret = malloc(sizeof(linkedlist));
  ret->top = NULL;
  return ret;
}; 


/**
 * @param list - linked list to check for address
 * @param addr - address to check for
 * @return bool - true if in list, false if not
*/
bool checkfor(linkedlist* list, uint64_t addr){
  node* temp = list->top;
  while(temp != NULL){
    if (temp->data->addr == addr){
      return true; 
    }
    temp = temp->next; 
  }
  return false; 
};

void addNode(linkedlist** list, node** nodeto){
  node* temp = (*list)->top;
  if (temp == NULL){
    (*list)->top = *nodeto;
    return;
  }
  while(temp->next != NULL){
    temp = temp->next;
  }
  temp->next = *nodeto;
  return; 
}

void delNode(linkedlist** list, node** nodeto){
  node* prev = NULL;
  node* curr = (*list)->top; 
  if (curr == NULL){
    return;
  }
  while(curr != NULL){
    if(curr == *nodeto){
      if(prev == NULL){
        (*list)->top = (*list)->top->next;
        return; 
      } else {
        prev->next = curr->next;
        return; 
      }
    }
    prev = curr;
    curr = curr->next;
  }
  return; 
}


// Helper methods for per-processor request queues.
static void enqBusRequest(bus_req* pr, int procNum)
{
    bus_req* iter;

    // No items in the queue.
    if (!queuedRequests[procNum])
    {
        queuedRequests[procNum] = pr;
        return;
    }

    // Add request to the end of the queue.
    iter = queuedRequests[procNum];
    while (iter->next)
    {
        iter = iter->next;
    }

    pr->next = NULL;
    iter->next = pr;
}

static bus_req* deqBusRequest(int procNum)
{
    bus_req* ret;

    ret = queuedRequests[procNum];

    // Move the head to the next request (if there is one).
    if (ret)
    {
        queuedRequests[procNum] = ret->next;
    }

    return ret;
}

static int busRequestQueueSize(int procNum)
{
    int count = 0;
    bus_req* iter;

    if (!queuedRequests[procNum])
    {
        return 0;
    }

    iter = queuedRequests[procNum];
    while (iter)
    {
        iter = iter->next;
        count++;
    }

    return count;
}

interconn* init(inter_sim_args* isa)
{
    int op;

    while ((op = getopt(isa->arg_count, isa->arg_list, "v")) != -1)
    {
        switch (op)
        {
            default:
                break;
        }
    }

    queuedRequests = malloc(sizeof(bus_req*) * processorCount);
    for (int i = 0; i < processorCount; i++)
    {
        queuedRequests[i] = NULL;
    }

    self = malloc(sizeof(interconn));
    self->busReq = busReq;
    self->registerCoher = registerCoher;
    self->busReqCacheTransfer = busReqCacheTransfer;
    self->si.tick = tick;
    self->si.finish = finish;
    self->si.destroy = destroy;

    memComp = isa->memory;
    memComp->registerInterconnect(self);

    pending = new_linkedlist();
    processing = new_linkedlist();

    return self;
}

int lastProc = 0; // for round robin arbitration

void registerCoher(coher* cc)
{
    coherComp = cc;
}

void memReqCallback(int procNum, uint64_t addr)
{
    if (processing->top == NULL)
    {
        return;
    }

    node* temp = processing->top;
    while(temp != NULL && temp->data != NULL){
        bus_req* Request = temp->data->request;
         if (addr == Request->addr && procNum == Request->procNum)
        {
            Request->dataAvail = 1;
        }
        temp = temp->next;
    }


}
//We need to check if we add it to pending or processing pile
void busReq(bus_req_type brt, uint64_t addr, int procNum)
{
    //now on bus request, we are going to check our list to see if that address is being used
    if(checkfor(processing, addr)){
        //if true, we need to add it to pending queue
        waitstr* temp = malloc(sizeof(waitstr));
        temp->brt = brt;
        temp->addr = addr;
        temp->procnum = procNum;

        node* temppy = malloc(sizeof(node));
        temppy->data = temp;
        temppy->next = NULL; 
        
        addNode(&pending, &temppy);
        return; 
    }
    //if not add to processing pile
    if (pendingRequest == NULL)
    {
        //may fail this assertion
        assert(brt != SHARED);

        bus_req* nextReq = calloc(1, sizeof(bus_req));
        nextReq->brt = brt;
        nextReq->currentState = WAITING_CACHE;
        nextReq->addr = addr;
        nextReq->procNum = procNum;
        nextReq->dataAvail = 0;
        nextReq->countDown = 0;

        pendingRequest = nextReq;
        nextReq->countDown = CACHE_DELAY;

        //add to processing
        node* temp = malloc(sizeof(node));
        temp->data->request = nextReq;
        addNode(&processing, &temp);
        
        return;
    } 
    else if (brt == SHARED && pendingRequest->addr == addr)
    {
        pendingRequest->shared = 1;
        return;
    }
    else if (brt == DATA && pendingRequest->addr == addr)
    {
        assert(pendingRequest->currentState == WAITING_MEMORY);
        pendingRequest->data = 1;
        pendingRequest->currentState = TRANSFERING_CACHE;
        //countDown = CACHE_TRANSFER;
        return;
    }
    else
    {
        assert(brt != SHARED);

        bus_req* nextReq = calloc(1, sizeof(bus_req));
        nextReq->countDown = 0; 
        nextReq->brt = brt;
        nextReq->currentState = QUEUED;
        nextReq->addr = addr;
        nextReq->procNum = procNum;
        nextReq->dataAvail = 0;

        
        enqBusRequest(nextReq, procNum);
    }
}

int tick()
{
    //on tick, see if we can process any of the outstanding bus requests
    //TODO: check
    node* pendcheck = pending->top;
    while(pendcheck != NULL){
        if(pendcheck->data != NULL && !checkfor(processing, pendcheck->data->addr)){
            busReq(pendcheck->data->brt, pendcheck->data->addr, pendcheck->data->procnum);
            node* quicktemp = pendcheck->next; 
            delNode(&pending, &pendcheck);
            pendcheck = quicktemp; 
        } else {
            pendcheck = pendcheck->next;
        }
    }

    //we need to puralize memComp and countDown
    memComp->si.tick();

    if (self->dbgEnv.cadssDbgWatchedComp && !self->dbgEnv.cadssDbgNotifyState)
    {
        printInterconnState();
    }
    node* tempreq = processing->top;
    while(tempreq != NULL){
        bus_req* currreq = tempreq->data->request; 
        int countDown = tempreq->data->request->countDown;
        if (countDown > 0)
        {
            assert(currreq != NULL);
            tempreq->data->request->countDown--;

            // If the count-down has elapsed (or there hasn't been a
            // cache-to-cache transfer, the memory will respond with
            // the data.
            if (tempreq->data->request->dataAvail)
            {
                tempreq->data->request->currentState = TRANSFERING_MEMORY;
                tempreq->data->request->countDown = 0;
            }

            if (countDown == 0)
            {
                if (tempreq->data->request->currentState == WAITING_CACHE)
                {
                    // Make a request to memory.
                    tempreq->data->request->countDown
                        = memComp->busReq(tempreq->data->request->addr,
                                        tempreq->data->request->procNum, memReqCallback);

                    tempreq->data->request->currentState = WAITING_MEMORY;

                    // The processors will snoop for this request as well.
                    for (int i = 0; i < processorCount; i++)
                    {
                        if (tempreq->data->request->procNum != i)
                        {
                            coherComp->busReq(tempreq->data->request->brt,
                                            tempreq->data->request->addr, i);
                        }
                    }

                    if (tempreq->data->request->data == 1)
                    {
                        tempreq->data->request->brt = DATA;
                    }
                }
                else if (tempreq->data->request->currentState == TRANSFERING_MEMORY)
                {
                    bus_req_type brt
                        = (tempreq->data->request->shared == 1) ? SHARED : DATA;
                    coherComp->busReq(brt, tempreq->data->request->addr,
                                    tempreq->data->request->procNum);

                    interconnNotifyState();
                    free(tempreq->data->request);
                    tempreq->data->request = NULL;
                    //TODO delete node
                    delNode(&processing, &tempreq);
                }
                else if (tempreq->data->request->currentState == TRANSFERING_CACHE)
                {
                    bus_req_type brt = tempreq->data->request->brt;
                    if (tempreq->data->request->shared == 1)
                        brt = SHARED;

                    coherComp->busReq(brt, tempreq->data->request->addr,
                                    tempreq->data->request->procNum);

                    interconnNotifyState();
                    free(tempreq->data->request);
                    tempreq->data->request = NULL;

                    //todo delete node
                    delNode(&processing, &tempreq);
                }
            }
        }
        else if (countDown == 0)
        {
            for (int i = 0; i < processorCount; i++)
            {
                int pos = (i + lastProc) % processorCount;
                if (queuedRequests[pos] != NULL)
                {
                    pendingRequest = deqBusRequest(pos);
                    countDown = CACHE_DELAY;
                    pendingRequest->currentState = WAITING_CACHE;

                    lastProc = (pos + 1) % processorCount;
                    break;
                }
            }
        }
        tempreq = tempreq->next; 
    }
    

    return 0;
}

void printInterconnState(void)
{
    if (!pendingRequest)
    {
        return;
    }
    int countDown = 0; 

    printf("--- Interconnect Debug State (Processors: %d) ---\n"
           "       Current Request: \n"
           "             Processor: %d\n"
           "               Address: 0x%016lx\n"
           "                  Type: %s\n"
           "                 State: %s\n"
           "         Shared / Data: %s\n"
           "                  Next: %p\n"
           "             Countdown: %d\n"
           "    Request Queue Size: \n",
           processorCount, pendingRequest->procNum, pendingRequest->addr,
           req_type_map[pendingRequest->brt],
           req_state_map[pendingRequest->currentState],
           pendingRequest->shared ? "Shared" : "Data", pendingRequest->next,
           countDown);

    for (int p = 0; p < processorCount; p++)
    {
        printf("       - Processor[%02d]: %d\n", p, busRequestQueueSize(p));
    }
}

void interconnNotifyState(void)
{
    if (!pendingRequest)
        return;

    if (self->dbgEnv.cadssDbgExternBreak)
    {
        printInterconnState();
        raise(SIGTRAP);
        return;
    }

    if (self->dbgEnv.cadssDbgWatchedComp && self->dbgEnv.cadssDbgNotifyState)
    {
        self->dbgEnv.cadssDbgNotifyState = 0;
        printInterconnState();
    }
}

// Return a non-zero value if the current request
// was satisfied by a cache-to-cache transfer.
int busReqCacheTransfer(uint64_t addr, int procNum)
{
    //TODO: iterate through active proccesses and see if they are the one that is done
    assert(processing->top->data);

    node* temp = processing->top;
    while(temp != NULL && temp->data != NULL){
        bus_req* Request = temp->data->request;
        if (addr == Request->addr && procNum == Request->procNum)
            return (Request->currentState == TRANSFERING_CACHE);
        temp = temp->next;
    }
    return 0;
}

int finish(int outFd)
{
    memComp->si.finish(outFd);
    return 0;
}

int destroy(void)
{
    // TODO
    memComp->si.destroy();
    return 0;
}
