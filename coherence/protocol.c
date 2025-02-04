#include "coher_internal.h"

void sendBusRd(uint64_t addr, int procNum)
{
    inter_sim->busReq(BUSRD, addr, procNum);
}

void sendBusWr(uint64_t addr, int procNum)
{
    inter_sim->busReq(BUSWR, addr, procNum);
}

void sendData(uint64_t addr, int procNum)
{
    inter_sim->busReq(DATA, addr, procNum);
}

void indicateShared(uint64_t addr, int procNum)
{
    inter_sim->busReq(SHARED, addr, procNum);
}

coherence_states
cacheMI(uint8_t is_read, uint8_t* permAvail, coherence_states currentState,
        uint64_t addr, int procNum)
{
    switch (currentState)
    {
        case INVALID:
            *permAvail = 0;
            sendBusWr(addr, procNum);
            return INVALID_MODIFIED;
        case MODIFIED:
            *permAvail = 1;
            return MODIFIED;
        case INVALID_MODIFIED:
            fprintf(stderr, "IM state on %lx, but request %d\n", addr,
                    is_read);
            *permAvail = 0;
            return INVALID_MODIFIED;
        default:
            fprintf(stderr, "State %d not supported, found on %lx\n",
                    currentState, addr);
            break;
    }

    return INVALID;
}

coherence_states
snoopMI(bus_req_type reqType, cache_action* ca, coherence_states currentState,
        uint64_t addr, int procNum)
{
    *ca = NO_ACTION;
    switch (currentState)
    {
        case INVALID:
            return INVALID;
        case MODIFIED:
            sendData(addr, procNum);
            // indicateShared(addr, procNum); // Needed for E state
            *ca = INVALIDATE;
            return INVALID;
        case INVALID_MODIFIED:
            if (reqType == DATA || reqType == SHARED)
            {
                *ca = DATA_RECV;
                return MODIFIED;
            }

            return INVALID_MODIFIED;
        default:
            fprintf(stderr, "State %d not supported, found on %lx\n",
                    currentState, addr);
            break;
    }

    return INVALID;
}


/**
 * MSI cache function, permission request function 
 * @param is_read - whether is read or write
 * @param currentState - state of cache line
 * @return permAvail  - set true if avaliable, false if not
 * @return nextState
 */
coherence_states
cacheMSI(uint8_t is_read, uint8_t* permAvail, coherence_states currentState,
        uint64_t addr, int procNum)
{
    switch (currentState)
    {
        case INVALID:
            *permAvail = 0;
            //TODO: figure this out lol
            if(is_read){
                sendBusRd(addr, procNum);
                return INVALID_SHARED;
            } else {
                sendBusWr(addr, procNum);
                return INVALID_MODIFIED; 
            }

        case MODIFIED:
            *permAvail = 1;
            return MODIFIED; 

        case INVALID_MODIFIED:
            fprintf(stderr, "IM state on %lx, but request %d\n", addr,
                    is_read);
            *permAvail = 0;
            return INVALID_MODIFIED; 

        case INVALID_SHARED:
            fprintf(stderr, "IS state on %lx, but request %d\n", addr,
                    is_read);
            *permAvail = 0;
            return INVALID_SHARED;

        case SHARE:
            if (is_read){
                *permAvail = 1;
                return SHARE;
            } else {
                *permAvail = 0;
                sendBusWr(addr, procNum);
                return INVALID_MODIFIED; 
            }

        default:
            fprintf(stderr, "State %d not supported, found on %lx\n",
                    currentState, addr);
            break;
    }

    return INVALID; 
}

/**
 * MSI snoop function - called on bus request
 * @param reqType - read or write
 * @return ca - cache accession to take
 * @param currentState - currentState
 * @param addr - address of interest
 * @param procNum - identifier of requesting processor 
*/
coherence_states
snoopMSI(bus_req_type reqType, cache_action* ca, coherence_states currentState,
        uint64_t addr, int procNum)
{
    *ca = NO_ACTION;
    switch (currentState)
    {
        case INVALID:
            return INVALID; 

        case MODIFIED:
            sendData(addr, procNum);
            if (reqType == BUSRD){
                return SHARE;
            } else if (reqType == BUSWR) {
                *ca = INVALIDATE;
                return INVALID; 
            }

        case INVALID_MODIFIED:
            if (reqType == DATA || reqType == SHARED){
                *ca = DATA_RECV;
                return MODIFIED;
            }
            return INVALID_MODIFIED; 

        case INVALID_SHARED:
            if (reqType == DATA || reqType == SHARED){
                *ca = DATA_RECV;
                return SHARE;
            }
            return INVALID_SHARED; 

        case SHARE:
            if (reqType == BUSWR){
                *ca = INVALIDATE;
                return INVALID; 
            }
            return SHARE; 

        default:
            fprintf(stderr, "State %d not supported, found on %lx\n",
                    currentState, addr);
            break;
    }

    return INVALID;
}


/**
 * MESI cache function, permission request function 
 * @param is_read - whether is read or write
 * @param currentState - state of cache line
 * @return permAvail  - set true if avaliable, false if not
 * @return nextState
 */
coherence_states
cacheMESI(uint8_t is_read, uint8_t* permAvail, coherence_states currentState,
        uint64_t addr, int procNum)
{
    switch (currentState)
    {
        case INVALID:
            *permAvail = 0;
            if(is_read){
                sendBusRd(addr, procNum);
                return INVALID_EXCLUSIVE;
            } else {
                sendBusWr(addr, procNum);
                return INVALID_MODIFIED; 
            }
        
        case INVALID_EXCLUSIVE:
            *permAvail = 0;
            return INVALID_EXCLUSIVE; 

        case MODIFIED:
            *permAvail = 1;
            return MODIFIED;

        case INVALID_MODIFIED:
            *permAvail = 0;
            return INVALID_MODIFIED;

        case SHARE:
            if (is_read){
                *permAvail = 1;
                return SHARE;
            } else {
                *permAvail = 0;
                sendBusWr(addr, procNum);
                return INVALID_MODIFIED; 
            }

        case EXCLUSIVE:
            *permAvail = 1;
            if(is_read){
                return EXCLUSIVE;
            } else {
                return MODIFIED;
            }

        default:
            fprintf(stderr, "State %d not supported, found on %lx\n",
                    currentState, addr);
            break;
    }

    return INVALID; 
}



/**
 * MESI snoop function - called on bus request
 * @param reqType - read or write
 * @return ca - cache accession to take
 * @param currentState - currentState
 * @param addr - address of interest
 * @param procNum - identifier of requesting processor 
*/
coherence_states
snoopMESI(bus_req_type reqType, cache_action* ca, coherence_states currentState,
        uint64_t addr, int procNum)
{
    *ca = NO_ACTION;
    switch (currentState)
    {
        case INVALID:
            return INVALID;

        case MODIFIED:
            sendData(addr, procNum);
            if (reqType == BUSRD){
                indicateShared(addr, procNum);
                sendData(addr, procNum);
                return SHARE;
            } else if (reqType == BUSWR){
                sendData(addr, procNum);
                *ca = INVALIDATE;
                return INVALID;
            }

        case INVALID_MODIFIED:
            if (reqType == DATA || reqType == SHARED){
                *ca = DATA_RECV;
                return MODIFIED;
            }
            return INVALID_MODIFIED; 

        case INVALID_EXCLUSIVE:
            if(reqType == SHARED){
                return SHARE;
            } else if (reqType == DATA){
                *ca = DATA_RECV;
                return EXCLUSIVE;
            }
            break;

        case SHARE:
            if(reqType == BUSRD){
                indicateShared(addr, procNum);
                return SHARE;
            } else {
                *ca = INVALIDATE;
                return INVALID;
            }

        case EXCLUSIVE:
            if(reqType == BUSRD){
                indicateShared(addr, procNum);
                return SHARE;
            } else {
                *ca = INVALIDATE;
                return INVALID;
            }

        default:
            fprintf(stderr, "State %d not supported, found on %lx\n",
                    currentState, addr);
            break;
    }

    return INVALID;
}
