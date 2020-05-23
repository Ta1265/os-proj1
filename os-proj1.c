#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/const.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/cred.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include "linux/uidgid.h"
#include <linux/slab.h>
#include <linux/rwlock.h>

//=====================//
//===Start_fifo_queue==//
//=====================//
typedef struct q_node{

    unsigned long data;

    struct q_node* nextNode;

} q_node_t;

void enqueue(q_node_t** front, unsigned long new_data){

    q_node_t* newNode = kmalloc(sizeof(q_node_t),GFP_KERNEL);
    if(!newNode){
        printk("\nmymessege:: enqueue newNode, kmalloc failure\n");
        return;
    }

    newNode->data = new_data;
    newNode->nextNode = *front;

    *front = newNode;
}

unsigned long dequeue(q_node_t **front) {
    q_node_t *temp = *front;
    q_node_t *prev = 0;

    if (*front == 0)
        return 0;

    while (temp->nextNode != 0) {
        prev = temp;
        temp = temp->nextNode;
    }

    unsigned long returnValue = temp->data;
    kfree(temp);

    if (prev)
        prev->nextNode = 0;
    else
        *front = 0;

    return returnValue;
}

long dumpQ(q_node_t* front){
    long count = 0;
    q_node_t* temp = front;
    printk("\n mymessege:: Dumping queue\n ");
    while( temp != 0){
        printk("mymessege:: Message[%lu] = %lu\n", count, temp->data);
        temp = temp->nextNode;
        count++;
    }
    return count;
}

void killQ(q_node_t** front){
    while(dequeue(front) != 0);
    kfree(front);
    *front = 0;
}

typedef struct fifo_queue{

    q_node_t* front;

    void (*enqueue)(q_node_t** Front, unsigned long data);

    unsigned long (*dequeue)(q_node_t** mailboxQ);

    long (*dumpQ)(q_node_t* mailboxQ);

    void (*killQ)(q_node_t** front);

}fifo_queue_t;

fifo_queue_t* createQ(void (*enqueue)(q_node_t** Front, unsigned long data), unsigned long(*dequeue)(q_node_t** mailboxQ),long (*dumpQ)(q_node_t* mailboxQ),void (*killQ)(q_node_t** front)){
    fifo_queue_t* q = (fifo_queue_t*)kmalloc(sizeof(fifo_queue_t),GFP_KERNEL);
    if(!q){
        printk("\nmymessege:: createQ q, kmalloc failure\n");
        return 0;
    }
    q->front = 0;
    q->enqueue = enqueue;
    q->dequeue = dequeue;
    q->dumpQ = dumpQ;
    q->killQ = killQ;

    return q;
}

//    fifo_queue_t* mailboxQ;
//    mailboxQ = createQ(enqueue, dequeue, dumpQ);
//    mailboxQ->enqueue(&mailboxQ->front,5);
//    printk("getFront_and_dequeue = %lu\n", mailboxQ->dequeue(&mailboxQ->front));
//    mailboxQ->dumpQ(mailboxQ->front);



//=====================//
//=====END_fifo_queue==//
//=====================//

//=====================//
//=====Start_ACL_LL====//
//=====================//
typedef struct ACL_node {
    pid_t allowedID;

    struct ACL_node* nextNode;

}ACL_node_t;

static void insert_ACL_node(ACL_node_t** h, pid_t allowedid){

    //insert to head of LL
    ACL_node_t* link = kmalloc(sizeof(ACL_node_t),GFP_KERNEL);
    if(link == 0){
        printk("\nmymessege:: insert_ACL_node link, kmalloc failure\n");
        return;
    }else{
        link->allowedID = allowedid;
        link->nextNode = *h;
        *h = link;
    }

}
static int checkExist(ACL_node_t** h, pid_t allowedid){
    ACL_node_t* temp = *h;

    while(temp != 0){
        if(temp->allowedID == allowedid){
            return 0; //returns true aka 0
        }
        temp = temp->nextNode;
    }
    return 1; //returns false aka 1
}

int deleteAllowedID(ACL_node_t** h, pid_t deleteid){
    ACL_node_t* temp = *h;
    ACL_node_t* prev = *h;
    while(temp != 0 && temp->allowedID == deleteid ){ // check if headnode contains the id to delete
        *h = temp->nextNode;
        kfree(temp);
        return 0; //deletion happened true
    }
    while(temp != 0 && temp->allowedID != deleteid ){
        prev = temp;
        temp = temp->nextNode;
    }
    if(temp == 0) return 1; //node not found and not deleted return false 1
    prev->nextNode = temp->nextNode;
    kfree(temp);
    return 0; //node found and deleted return true
}

void dumpList(ACL_node_t* h)
{
    while (h != 0) {
        printk(" %u ", h->allowedID);
        h = h->nextNode;
    }
}
void killACL(ACL_node_t** h){
    ACL_node_t* temp = *h;
    ACL_node_t* prev = *h;
    while(temp != 0){
        prev = temp;
        temp = temp->nextNode;
        kfree(prev);
    }
    kfree(temp);
    kfree(h);
}

typedef struct ACL_LIST{
    ACL_node_t* h;
    void (*killACL)(ACL_node_t** h);
    void (*insert_ACL_node)(ACL_node_t** h,pid_t allowedid);
    int (*checkExist)(ACL_node_t** h,pid_t allowedid);
    int (*deleteAllowedID)(ACL_node_t** h,pid_t deleteid);
    void (*dumpList)(ACL_node_t* h);
}ACL_LIST_T;

ACL_LIST_T* createACL(void (*killACL)(ACL_node_t** h),void (*insert_ACL_node)(ACL_node_t** h,pid_t allowedid),int (*checkExist)(ACL_node_t** h,pid_t allowedid),int (*deleteAllowedID)(ACL_node_t** h,pid_t deleteid),void(*dumpList)(ACL_node_t* h)){
    ACL_LIST_T* ACL = (ACL_LIST_T*)kmalloc(sizeof(ACL_LIST_T),GFP_KERNEL);
    if(ACL == 0){
        printk("\nmymessege:: createACL ACL, kmalloc failure\n");
        return 0;
    }
    ACL->h = 0;
    ACL->killACL = killACL;
    ACL->insert_ACL_node = insert_ACL_node;
    ACL->checkExist = checkExist;
    ACL->deleteAllowedID = deleteAllowedID;
    ACL->dumpList = dumpList;
    return ACL;
}
//
// ==how to use ACL==
//
//ACL_LIST_T* ACL = createACL(killACL,insert_ACL_node,checkExist,deleteAllowedID,dumpList);
//ACL->insert_ACL_node(&ACL->h,seven);
//ACL->dumpList(ACL->h);
//ACL->deleteAllowedID(&ACL->h,five);
//if(checkExist(&ACL->h, four) == 0);

//=====================//
//=====END_ACL_LL======//
//=====================//

//=====================//
//===Start_SkipList====//
//=====================//
unsigned int MAX_LEVEL;

unsigned int PROBABILITY;

unsigned long MAXNUMBER = (2^63)-1;

static unsigned int next_random = 423534345;

static unsigned int generate_random_int(void) {
    next_random = next_random * 1103515245 + 12345;

    return (next_random / 65536) % 32768;
}

static unsigned int getLevel(void){
    unsigned int level = 1;

    unsigned int rand_int = generate_random_int();

    while(rand_int <= (32768/PROBABILITY) && level <= MAX_LEVEL){
        level++;
        rand_int = generate_random_int();
    }
    return level;

}

typedef struct sl_node {

    unsigned long id;

    ACL_LIST_T* ACL;

    fifo_queue_t* mailBox;

    struct sl_node** forwardNodes; // Array to hold pointers to node of different level

} sl_node;

typedef struct skip_list {

    unsigned int level;

    sl_node* head; // head of the list of nodes

} skip_list_t;

skip_list_t* theList;

static int skip_list_init(void) {

    theList = (skip_list_t*)kmalloc(sizeof(skip_list_t),GFP_KERNEL);
    if(!theList){
        printk("\nmymessege:: skip_list_init theList, kmalloc failure\n");
        return -1;
    }

    sl_node* ahead = (sl_node*)kmalloc(sizeof(sl_node),GFP_KERNEL); //allocate mem for head node
    if(!ahead){
        printk("\nmymessege:: skip_list_init ahead, kmalloc failure\n");
        return -1;
    }
    theList->head = ahead;
    ahead->id = MAXNUMBER; //initialize to int max so that it cannot be replaced
    ahead->mailBox = createQ(enqueue, dequeue, dumpQ, killQ);
    ahead->forwardNodes = (sl_node**)kmalloc((MAX_LEVEL+1) * sizeof(sl_node*),GFP_KERNEL); // allocate mem for the nodes forwardnodel list of node pointers
    if(!ahead->forwardNodes){
        printk("\nsmymessege:: kip_list_init ahead->forwardNodes, kmalloc failure\n");
        return -1;
    }
    theList->head = ahead;
    unsigned int i;
    for(i = 0; i<=MAX_LEVEL;i++){
        ahead->forwardNodes[i] = ahead; //fill the entire head node pointer at all levels
    }
    theList->level = 1;

    printk("\nmymessege:: skip list init made it to 320\n");
    return 0;

};

static void insert(unsigned long insert_id, unsigned long insert_mailbox_data) {

    unsigned int assign_level;

    //sl_node *updateSpot[MAX_LEVEL + 1];

    sl_node **updateSpot = (sl_node**)kmalloc_array((MAX_LEVEL+1),sizeof(sl_node*), GFP_KERNEL);

    sl_node *n = theList->head;



    unsigned int i;
    for (i = theList->level; i >= 1; i--) {
        //printk("comapiring n->forwardNodes[%lu]->id(%lu)  < insert_id(%lu)\n", i, n->forwardNodes[i]->id, insert_id);
        while (n->forwardNodes[i]->id < insert_id) {
            //printk("its less than so moving into forwardnodes\n");
            n = n->forwardNodes[i];
        }
        //printk("its greater than or = to, so adding note to update list at this level\n");
        updateSpot[i] = n;
    }
    n = n->forwardNodes[1];

    if (n->id == insert_id) {
        n->mailBox->enqueue(&n->mailBox->front, insert_mailbox_data); //if this spot in the skip list already exists just replace its data with given
        return;
    } else { // else create a new node to hold the data, coin toss how many levels to assign its forwardNodes
        assign_level = getLevel();

        if (assign_level > theList->level) { //make sure the list header reaches to highest level
            for (i = theList->level + 1; i <= assign_level; i++) {
                updateSpot[i] = theList->head;
            }
            theList->level = assign_level;
        }

        n = (sl_node *) kmalloc(sizeof(sl_node),GFP_KERNEL);
        n->id = insert_id;
        n->mailBox = createQ(enqueue, dequeue, dumpQ, killQ);
        n->mailBox->enqueue(&n->mailBox->front, insert_mailbox_data);
        n->forwardNodes = (sl_node **) kmalloc((assign_level + 1) * sizeof(sl_node *),GFP_KERNEL);

        for (i = 1; i <= assign_level; i++) {  // update all assigned levels with the info
            n->forwardNodes[i] = updateSpot[i]->forwardNodes[i];
            updateSpot[i]->forwardNodes[i] = n;
        }
    }
}
static void create_mb_empty(unsigned long insert_id) {

    unsigned int assign_level;

    sl_node **updateSpot = (sl_node**)kmalloc_array((MAX_LEVEL+1), sizeof(sl_node*), GFP_KERNEL);

    printk("\nmymessege:: creat_mb_empty made it to 380\n");

    sl_node *n = theList->head;
    unsigned int i;
    for (i = theList->level; i >= 1; i--) {
        while (n->forwardNodes[i]->id < insert_id) {
            n = n->forwardNodes[i];
        }
        updateSpot[i] = n;
    }
    printk("\nmymessege:: creat_mb_empty made it to 390\n");

    assign_level = getLevel();

    if (assign_level > theList->level) { //make sure the list header reaches to highest level
        for (i = theList->level + 1; i <= assign_level; i++) {
            updateSpot[i] = theList->head;
        }
        theList->level = assign_level;
    }

    n = (sl_node *) kmalloc(sizeof(sl_node),GFP_KERNEL);
    if(!n){
        printk("\nmymessege:: create_mb_empty n, kmalloc failure\n");
        return;
    }
    printk("\nmymessege:: creat_mb_empty made it to 406\n");
    n->id = insert_id;
    n->mailBox = createQ(enqueue, dequeue, dumpQ, killQ);
    n->forwardNodes = (sl_node **) kmalloc(((assign_level + 1) * sizeof(sl_node *)),GFP_KERNEL);
    printk("\ncreat_mb_empty made it to 410\n");
    for (i = 1; i <= assign_level; i++) {  // update all assigned levels with the info
        n->forwardNodes[i] = updateSpot[i]->forwardNodes[i];
        updateSpot[i]->forwardNodes[i] = n;
    }
    printk("\nmymessege:: creat_mb_empty made it to 415\n");
}

static sl_node* search(unsigned long search_id){

    sl_node* n = theList->head;
    unsigned int i;
    for(i = theList->level; i >= 1; i--){
        while(n->forwardNodes[i] != 0 && n->forwardNodes[i]->id < search_id){
            n = n->forwardNodes[i];
        }
    }

    if(n->forwardNodes[1]->id == search_id){
        return n->forwardNodes[1];
    }
    else{
        printk("mymessege:: error id value not found in skip_list, maybe you deleted it?");
        return 0;
    }
}

static unsigned long delete(unsigned long delete_id){
    sl_node* updateSpot[MAX_LEVEL+1];
    sl_node* n = theList->head;
    unsigned int i;
    for(i = theList->level; i >=1; i--){
        while(n->forwardNodes[i]->id < delete_id){
            n = n->forwardNodes[i];
        }
        updateSpot[i] = n;
    }
    n = n->forwardNodes[1];
    if(n->id == delete_id){
        for(i = 1; i <= theList->level; i++){
            if(updateSpot[i]->forwardNodes[i] != n){
                break;
            }
            updateSpot[i]->forwardNodes[i] = n->forwardNodes[i];
        }
        n->mailBox->killQ(&n->mailBox->front);
        kfree(n->forwardNodes);
        kfree(n);
        while (theList->level > 1 && theList->head->forwardNodes[theList->level] == theList->head) {
            theList->level--;
        }
        return 0; //deletion works
    }
    return -ENOENT; //error deletion didn't work.
}

static void dump(void){

    if(theList == 0){
        printk("mymessege:: list = 0");
        return;
    }

    sl_node* n = theList->head;
    printk("\nmymessege:: dumping skip list(should be in order)");
    while(n && n->forwardNodes[1] != theList->head){
        printk("\nmymessege:: MailBox ID [%lu] contains: ", n->forwardNodes[1]->id);
        n->forwardNodes[1]->mailBox->dumpQ(n->forwardNodes[1]->mailBox->front);
        n = n->forwardNodes[1];
    }
}

static void killAll(void){
    sl_node* n = theList->head;
    sl_node* temp = 0;

    while(n && n->forwardNodes[1] != theList->head){
        temp = n->forwardNodes[1];
        n = n->forwardNodes[1];
        delete(temp->id);
    }
    delete(theList->head->id);
    kfree(theList);
    theList = 0;
}

//=================//
//===End SkipList==//
//=================//

//======================//
//===Start SystemCalls==//
//======================//




SYSCALL_DEFINE2(mbx421_init, unsigned int, ptrs, unsigned int, prob){

    //checks for root access only
    if(!uid_eq(current_uid(), GLOBAL_ROOT_UID)) {return -EPERM;}; //not root

    if(prob != 2 && prob != 4 && prob != 8 && prob != 16) return -EINVAL;
    if(ptrs == 0) return -EINVAL;
    MAX_LEVEL = ptrs; // maximum number of pointers any single note may have.
    PROBABILITY = prob; //prob 2,4,8,16?
    if(theList == 0){
        skip_list_init();
    }

    return 0;
}


SYSCALL_DEFINE0(mbx421_shutdown){
    //root user 0 access only
    printk("mymessege:: uid : %d : called shutdown:", current_uid());
    if(!uid_eq(current_uid(), GLOBAL_ROOT_UID)) {return -EPERM;}// not root
    killAll();
    return 0;
}



SYSCALL_DEFINE1(mbx421_create, unsigned long, id){
    //root user 0 access only looking at uid only not euid
    if(!uid_eq(current_uid(), GLOBAL_ROOT_UID)) {return -EPERM;} // not root
    if(id == 0 || id == ((2^64)-1)) return -EINVAL; //invalid parameter
    if(search(id) != 0) return -EEXIST; // mailbox already exists

    create_mb_empty(id);
    return 0;
}



SYSCALL_DEFINE1(mbx421_destroy, unsigned long, id){
    //correct permissions or root
    sl_node* temp;
    temp = search(id);
    if(temp == 0){
        return -ENOENT;
    }
    if(temp->ACL !=0){ //if an ACL exists
        if(!uid_eq(current_uid(), GLOBAL_ROOT_UID)){ // and the user is not root
            if(!temp->ACL->checkExist(&temp->ACL->h, task_pid_nr(current))) { // and the user is not in the ACL list
                return -EPERM; // then they dont have permission to execute this.
            }
        }
    }
    return delete(id);
}


SYSCALL_DEFINE1(mbx421_count, unsigned long, id){
    //correct permissions or root
    sl_node* temp;
    temp = search(id);
    if(temp == 0){
        return -ENOENT;
    }
    if(temp->ACL !=0){ //if an ACL exists
        if(!uid_eq(current_uid(), GLOBAL_ROOT_UID)){ // and the user is not root
            if(!temp->ACL->checkExist(&temp->ACL->h, task_pid_nr(current))) { // and the user is not in the ACL list
                return -EPERM; // then they dont have permission to execute this.
            }
        }
    }
    if(temp != 0){
        long count = temp->mailBox->dumpQ(temp->mailBox->front);
        return count;
    }
   return -ENOENT; //mailbox DNE
}




SYSCALL_DEFINE3(mbx421_send, unsigned long, id, const unsigned char __user *, msg, long, len){
    //correct permissions or root
    sl_node* temp;
    temp = search(id);
    if(!uid_eq(current_uid(), GLOBAL_ROOT_UID)) {return -EPERM;} // not root

    unsigned long *kernMsg;
    kernMsg = (unsigned long*)kmalloc((unsigned int)len, GFP_KERNEL);
    if(sizeof(kernMsg) == 0){
        printk("mymessege:: kernMsg not allocated properly at 572 sizeof(kernMsg) returned 0");
        return -1;
    }
    unsigned long bytes_not_copied;
    bytes_not_copied = copy_from_user(kernMsg,(const void*)msg, len);
    if(bytes_not_copied == 0){ //successful converted to kern
        insert(id,kernMsg);
        return 0;
    }
    return -EFAULT; //bad pointer failure
}



SYSCALL_DEFINE3(mbx421_recv, unsigned long, id, const unsigned char __user *, msg, long, len){
    //correct permissions or root
    sl_node* temp;
    temp = search(id);
    if(temp == 0){
        return -ENOENT;
    }
    if(!uid_eq(current_uid(), GLOBAL_ROOT_UID)) {return -EPERM;} // not root


    if(temp != 0){
        unsigned long kernMsg = temp->mailBox->dequeue(&temp->mailBox->front);

        if(sizeof(kernMsg) >= len){ //if messege size is larger then len size use the smaller(len)
            copy_to_user(msg, (const void*)kernMsg, len);
            return len;
        }
        copy_to_user((void*)msg, kernMsg, sizeof(kernMsg));
        return sizeof(kernMsg);
    }
    return -ENOENT; //mailbox DNE
}




SYSCALL_DEFINE1(mbx421_length, unsigned long, id){
    //correct permissions or root
    sl_node* temp;
    temp = search(id);
    if(temp == 0){
        return -ENOENT;
    }
    if(temp->ACL !=0){ //if an ACL exists
        if(!uid_eq(current_uid(), GLOBAL_ROOT_UID)){ // and the user is not root
            if(!temp->ACL->checkExist(&temp->ACL->h, task_pid_nr(current))) { // and the user is not in the ACL list
                return -EPERM; // then they dont have permission to execute this.
            }
        }
    }

    if(temp != 0){
        if(!uid_eq(current_uid(), GLOBAL_ROOT_UID) && !temp->ACL->checkExist(&temp->ACL->h, task_pid_nr(current))) {return -EPERM;}
        return sizeof((temp->mailBox->front->data));
    }
    return -ENOENT;//mailbox DNE
}





SYSCALL_DEFINE2(mbx421_acl_add, unsigned long, id, pid_t, process_id){
    //root only
    if(!uid_eq(current_uid(), GLOBAL_ROOT_UID)) {return -EPERM;} //not root

    sl_node* temp;
    temp = search(id);

    if (temp != 0) { // if the mailbox exists at all then continue
        if (temp->ACL == 0) { //if no ACL exist create one and insert into it
            temp->ACL = createACL(killACL, insert_ACL_node, checkExist, deleteAllowedID, dumpList);
            temp->ACL->insert_ACL_node(&temp->ACL->h, process_id);
            return 0;
        } else { //ACL exists insert to it.
            temp->ACL->insert_ACL_node(&temp->ACL->h, process_id);
            return 0;
        }
    }
    return -ENOENT; //mailbox dNE
}

SYSCALL_DEFINE2(mbx421_acl_remove, unsigned long, id, pid_t, process_id){
    //root only
    if(!uid_eq(current_uid(), GLOBAL_ROOT_UID)) {return -EPERM;} //not root

    sl_node* temp;
    temp = search(id);

    if (temp != 0) { // if the mailbox exists at all then continue
        if (temp->ACL == 0) { //if no ACL exist no need to do anything just exit create one and insert into it
            return 0;
        } else { //ACL exists insert to it.
            temp->ACL->deleteAllowedID(&temp->ACL->h, process_id);
            return 0;
        }
    }
    return -ENOENT; //mailbox dNE
}

SYSCALL_DEFINE0(mbx421_dump){
    dump();
    return 0;
}





//======================//
//===End SystemCalls====//
//======================//








