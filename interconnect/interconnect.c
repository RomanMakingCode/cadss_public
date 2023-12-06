#include <getopt.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

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
uint64_t myticker = 0;

int verbose = 0;
int veboseUltra = 0;  
void printv(const char *format, ...) { // wrapper for printf, only prints when verbose is set to true
    va_list args;
    va_start(args, format);
    if (verbose) vprintf(format, args);
}

void printu(const char *format, ...) { // wrapper for printf, only prints when verbose is set to true
    va_list args;
    va_start(args, format);
    if (veboseUltra) vprintf(format, args);
}
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
void delNodeProcessing(node* nodeto);
void printNode(node* nodeto);
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
    printv("adding node to processing\n");
    printNode(*nodeto);
    (*nodeto)->next = NULL;
    node* temp = (*list)->top;
    if (temp == NULL){
        (*list)->top = *nodeto;
        (*list)->top->next = NULL;

        return;
    }
    while(temp->next != NULL){
        temp = temp->next;
    }
    temp->next = *nodeto;
    printv("finished adding\n");
    return; 
}

void addNodePending(node* nodeto){
  node* temp = pending->top;
  printv("Adding Pending node\n");
  printNode(nodeto);
  if (temp == NULL){
    pending->top = nodeto;
    return;
  }
  while(temp->next != NULL){
    temp = temp->next;
  }
  temp->next = nodeto;
  printv("finished adding to pending\n");
  return; 
}

void addNodePending2(uint64_t addr, int proc){
    node* temp = pending->top;

    printv("Adding Pending node of address| %lu and proc %d\n", addr, proc);
    if (temp == NULL){
        pending->top = malloc(sizeof(waitstr));
        pending->top->data->addr = addr;
        pending->top->data->procnum = proc;

        return;
    }
    while(temp->next != NULL){
        temp = temp->next;
    }
    temp->next = malloc(sizeof(waitstr));
    temp->data->addr = addr;
    temp->data->procnum = proc; 
  return; 
}

void printNode(node* nodeto){
    if(nodeto->data == NULL){
        printv("NULL Node\n");
    } else if (nodeto->data->request == NULL){
        printv("Node (waiting type) Address: %lu | ProcNum: %d\n", nodeto->data->addr, nodeto->data->procnum);
        return; 
    }
    printv("Node (bus request type) Address: %lu | ProcNum: %d | CountDown: %d | data_avil: %s | currentState: ", nodeto->data->request->addr,nodeto->data->request->procNum, nodeto->data->request->countDown, nodeto->data->request->dataAvail ? "true" : "false");
    
    switch(nodeto->data->request->currentState){
        case(NONE):
            printv("NONE\n");
            break;
        case(QUEUED):
            printv("QUEUED\n");
            break;
        case(TRANSFERING_CACHE):
            printv("TRANSFERING_CACHE\n");
            break;
        case(TRANSFERING_MEMORY):
            printv("TRANSFERING_MEMORY\n");
            break;
        case(WAITING_CACHE):
            printv("WAITING_CACHE\n");
            break;
        case(WAITING_MEMORY):
            printv("WAITING_MEMORY\n");
            break;
        default:
            printv("ERROR\n");
            break;
    }
}


void printProcessing(){
    printv("Processing Requests\n");
    node* temp = processing->top;
    while(temp != NULL){
        if(temp->data != NULL && temp->data->request != NULL){
            printNode(temp);
            if(temp->data->request->currentState > 5){
                printv("err\n");
                delNodeProcessing(temp);
                return;
            }
        }
        temp = temp->next;
    }
}

void delNode(linkedlist** list, node** nodeto){
    printv("Deleting node: \n");
    printNode(*nodeto);
    printv("current processing\n");
    printProcessing();

  node* prev = NULL;
  node* curr = (*list)->top;
  printNode(curr);
  if (curr == NULL){
    printv("true\n");
    (*list)->top = NULL;
    return;
  }
  while(curr != NULL){
    if(curr == *nodeto){
        printv("match found in delete\n");
      if(prev == NULL){
        (*list)->top = (*list)->top->next;
        //printv("new processing\n");
        //printProcessing();
        return; 
      } else {
        prev->next = curr->next;
        //printv("new processing elsed\n");
        //printProcessing();
        return; 
      }
    }
    prev = curr;
    curr = curr->next;
  }
  printv("nothing deleted\n");
  return; 
}

void delNodeProcessing(node* nodeto){
    printu("Deleting node: \n");
    if(veboseUltra)
        printNode(nodeto);
    printu("current processing\n");
    if(veboseUltra)
        printProcessing();

    node* prev = NULL;
    node* curr = processing->top;
    if(veboseUltra)
        printNode(curr);
    if (curr == NULL){
        printu("true\n");
        processing->top = NULL;
        return;
    }
    while(curr != NULL){
        if(curr == nodeto){
            printu("match found in delete\n");
        if(prev == NULL){
            processing->top = processing->top->next;
            //printv("new processing\n");
            //printProcessing();
            return; 
        } else {
            prev->next = curr->next;
            //printv("new processing elsed\n");
            //printProcessing();
            return; 
        }
        }
    prev = curr;
    curr = curr->next;
  }
  printu("nothing deleted\n");
  return; 
}

void delNodePending(node* nodeto){
    printv("Deleting node: \n");
    printNode(nodeto);
    printv("current processing\n");
    printProcessing();
    node* prev = NULL;
    node* curr = pending->top;
    printNode(curr);
    if (curr == NULL){
        printv("true\n");
        pending->top = NULL;
        return;
    }
    while(curr != NULL){
        if(curr == nodeto){
            printv("match found in delete\n");
        if(prev == NULL){
            pending->top = pending->top->next;
            //printv("new processing\n");
            //printProcessing();
            return; 
        } else {
            prev->next = curr->next;
            //printv("new processing elsed\n");
            //printProcessing();
            return; 
        }
        }
    prev = curr;
    curr = curr->next;
  }
  printv("nothing deleted\n");
  return; 
}
/*
void deleteNode(struct Node** head_ref, int key) 
{ 
    // Store head node 
    struct Node *temp = *head_ref, *prev; 
  
    // If head node itself holds the key to be deleted 
    if (temp != NULL && temp->data == key) { 
        *head_ref = temp->next; // Changed head 
        free(temp); // free old head 
        return; 
    } 
  
    // Search for the key to be deleted, keep track of the 
    // previous node as we need to change 'prev->next' 
    while (temp != NULL && temp->data != key) { 
        prev = temp; 
        temp = temp->next; 
    } 
  
    // If key was not present in linked list 
    if (temp == NULL) 
        return; 
  
    // Unlink the node from linked list 
    prev->next = temp->next; 
  
    free(temp); // Free memory 
} */
  


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
    //printv("Init Interconnect\n");
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

    printf("Starting\n");

    return self;
}

int lastProc = 0; // for round robin arbitration

void registerCoher(coher* cc)
{
    coherComp = cc;
}

void memReqCallback(int procNum, uint64_t addr)
{
    //printv("memReqCallBack\n");
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
    //printv("BusRequest\n");
    //now on bus request, we are going to check our list to see if that address is being used
    if(checkfor(processing, addr)){
        //printv("adding to pending\n");
        //if true, we need to add it to pending queue
        waitstr* temp = malloc(sizeof(waitstr));
        temp->brt = brt;
        temp->addr = addr;
        temp->procnum = procNum;

        node* temppy = malloc(sizeof(node));
        temppy->data = temp;
        temppy->next = NULL; 
        printv("addres %lu and proc %d\n", addr, processing);
        //addNodePending(temppy);
        addNodePending2(addr, procNum);
        return; 
    }
    //if not add to processing pile
    if (pendingRequest == NULL)
    {
        //printv("Processing new request\n");
        //may fail this assertion
        assert(brt != SHARED);

        bus_req* nextReq = calloc(1, sizeof(bus_req));
        nextReq->brt = brt;
        nextReq->currentState = WAITING_CACHE;
        nextReq->addr = addr;
        nextReq->procNum = procNum;
        nextReq->dataAvail = 0;
        nextReq->countDown = 0;
        //printv("busreq object created, proc: %d, addr %lu\n", procNum, addr);
        //pendingRequest = nextReq;
        nextReq->countDown = CACHE_DELAY;

        //add to processing
        node* temp = malloc(sizeof(node));
        waitstr* tempwait = malloc(sizeof(waitstr));
        tempwait->request = nextReq; 
        temp->data = tempwait;
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

void printPending(){
    printv("Pending Requests\n");
    node* temp = processing->top;
    while(temp != NULL){
        if(temp->data != NULL){
            printv("Address: %lu | ProcNum:%d\n", temp->data->addr, temp->data->procnum);
        }
        temp = temp->next;
    }
}

int tick()
{
    myticker++;
    /*if(myticker > 1000){
        assert(false);
    }*/
    printv("tick count = %d\n", myticker);
    if (verbose){
        printProcessing();
        printPending();
    }
    //on tick, see if we can process any of the outstanding bus requests
    //TODO: check
    node* pendcheck = pending->top;
    while(pendcheck != NULL){
        if(pendcheck->data != NULL) {
            if(!checkfor(processing, pendcheck->data->addr)){
                printv("RUnning pending instruction\n");
                busReq(pendcheck->data->brt, pendcheck->data->addr, pendcheck->data->procnum);
                node* quicktemp = pendcheck->next; 
                delNodePending(pendcheck);
                pendcheck = quicktemp; 
            } else {
                //check if we can snoop
                //find address match
                node* temp = processing->top;
                while(temp != NULL){
                    //remove if shared
                    if (temp->data->request->addr == pendcheck->data->addr){
                        printv("Match!\n");
                        if (temp->data->request->shared){
                            printv("snooped!\n");
                            node* quicktemp = pendcheck->next; 
                            delNodePending(pendcheck);
                            pendcheck = quicktemp; 
                        }
                    }
                    temp = temp->next; 
                }
                pendcheck = pendcheck->next;
            }
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
    node** tempreq = &processing->top;
    node** quicktemp = NULL;
    bool quicktemp_use = false; 
    while(tempreq != NULL && (*tempreq) != NULL && (*tempreq)->data != NULL && (*tempreq)->data->request != NULL){
        printu("start\n");
        printv("updating: "); printNode((*tempreq));
        bus_req* currreq = (*tempreq)->data->request; 
        int *countDown = &((*tempreq)->data->request->countDown);
        //printv("what is coundown rn: %lu \n", *countDown);
        if (*countDown > 0)
        {
            assert(currreq != NULL);
            (*countDown)--; 

            // If the count-down has elapsed (or there hasn't been a
            // cache-to-cache transfer, the memory will respond with
            // the data.
            if ((*tempreq)->data->request->dataAvail)
            {
                (*tempreq)->data->request->currentState = TRANSFERING_MEMORY;
                (*tempreq)->data->request->countDown = 0;
            }

            if ((*countDown) == 0)
            {
                printv("countdown up ---------------\n");
                if ((*tempreq)->data->request->currentState == WAITING_CACHE)
                {
                    printv("Issuing Memory request\n");
                    // Make a request to memory.
                    (*tempreq)->data->request->countDown
                        = memComp->busReq((*tempreq)->data->request->addr,
                                        (*tempreq)->data->request->procNum, memReqCallback);

                    (*tempreq)->data->request->currentState = WAITING_MEMORY;

                    // The processors will snoop for this request as well.
                    for (int i = 0; i < processorCount; i++)
                    {
                        if ((*tempreq)->data->request->procNum != i)
                        {
                            coherComp->busReq((*tempreq)->data->request->brt,
                                            (*tempreq)->data->request->addr, i);
                        }
                    }

                    if ((*tempreq)->data->request->data == 1)
                    {
                        (*tempreq)->data->request->brt = DATA;
                    }
                }
                else if ((*tempreq)->data->request->currentState == TRANSFERING_MEMORY)
                {
                    printu("Transfering Memory\n");
                    bus_req_type brt
                        = ((*tempreq)->data->request->shared == 1) ? SHARED : DATA;
                    coherComp->busReq(brt, (*tempreq)->data->request->addr,
                                    (*tempreq)->data->request->procNum);

                    interconnNotifyState();
                    printu("freeing structure\n");
                    //free((*tempreq)->data->request);
                    //(*tempreq)->data->request = NULL;
                    //TODO delete node
                    quicktemp = (node**)&((*tempreq)->next);
                    quicktemp_use = true;
                    if(((*tempreq)->next) == NULL){
                        printu("true\n");
                    }

                    printu("deleting node\n");
                    //printNode(*tempreq);
                    delNodeProcessing((*tempreq));
                    printu("deleted node\n");
                    //printProcessing();
                }
                else if ((*tempreq)->data->request->currentState == TRANSFERING_CACHE)
                {
                    printv("Transfering Cache\n");
                    bus_req_type brt = (*tempreq)->data->request->brt;
                    if ((*tempreq)->data->request->shared == 1)
                        brt = SHARED;

                    coherComp->busReq(brt, (*tempreq)->data->request->addr,
                                    (*tempreq)->data->request->procNum);

                    interconnNotifyState();

                    quicktemp = (node**)&((*tempreq)->next);
                    quicktemp_use = true;
                    
                    //free((*tempreq)->data->request);
                    //(*tempreq)->data->request = NULL;

                    //todo delete nodema
                    //delNode(&processing, &(*tempreq));
                    delNodeProcessing((*tempreq));
                    //printProcessing();
                }
            }
        } else if ((*countDown) == 0) {
            delNodeProcessing(*tempreq);
            for (int i = 0; i < processorCount; i++)
            {
                int pos = (i + lastProc) % processorCount;
                if (queuedRequests[pos] != NULL)
                {
                    pendingRequest = deqBusRequest(pos);
                    (*countDown) = CACHE_DELAY;
                    pendingRequest->currentState = WAITING_CACHE;

                    lastProc = (pos + 1) % processorCount;
                    break;
                }
            }
        } else if ((*countDown) < 0) {
            printv("overflow delete???\n");

            quicktemp = (node**)&((*tempreq)->next);
            quicktemp_use = true;
            delNodeProcessing((*tempreq));
        }
        printu("onto next node\n");
        if(quicktemp_use){
            printu("temp used\n");
            tempreq = quicktemp;
            quicktemp = NULL;
            quicktemp_use = false;
        } else {
            printu("no temp\n");
            tempreq = (node**)&((*tempreq)->next); 
        }
        printu("proceed\n");
    
    }
    printProcessing();
    printv("tick finished\n");
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
    printv("busReqCacheTransfer\n");
    //TODO: iterate through active proccesses and see if they are the one that is done
    assert(processing->top->data);

    node* temp = processing->top;
    while(temp != NULL && temp->data != NULL){
        bus_req* Request = temp->data->request;
        if (addr == Request->addr && procNum == Request->procNum){
            if(Request->currentState == TRANSFERING_CACHE){
                printv("True\n");
            } else {
                printv("False\n");
            }
            return (Request->currentState == TRANSFERING_CACHE);
        }
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
