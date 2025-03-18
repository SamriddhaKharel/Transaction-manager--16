/***************** Transaction class **********************/
/*** Implements methods that handle Begin, Read, Write, ***/
/*** Abort, Commit operations of transactions. These    ***/
/*** methods are passed as parameters to threads        ***/
/*** spawned by Transaction manager class.              ***/
/**********************************************************/
 
/* Spring 2025: CSE 4331/5331 Project 2 : Tx Manager */
 
/* Required header files */
#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include "zgt_def.h"
#include "zgt_tm.h"
#include "zgt_extern.h"
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <pthread.h>
 
 
extern void *start_operation(long, long);  //start an op with mutex lock and cond wait
extern void *finish_operation(long);        //finish an op with mutex unlock and con signal
 
extern void *do_commit_abort_operation(long, char);   //commit/abort based on char value
extern void *process_read_write_operation(long, long, int, char);
 
extern zgt_tm *ZGT_Sh;      // Transaction manager object
 
/* Transaction class constructor */
/* Initializes transaction id and status and thread id */
/* Input: Transaction id, status, thread id */
 
zgt_tx::zgt_tx( long tid, char Txstatus, char type, pthread_t thrid){
  this->lockmode = (char)' ';   // default
  this->Txtype = type;          // R = read only, W=Read/Write
  this->sgno =1;
  this->tid = tid;
  this->obno = -1;              // set it to a invalid value
  this->status = Txstatus;
  this->pid = thrid;
  this->head = NULL;
  this->nextr = NULL;
  this->semno = -1;             // init to an invalid sem value
}
 
/* Method used to obtain reference to a transaction node      */
/* Inputs the transaction id. Makes a linear scan over the    */
/* linked list of transaction nodes and returns the reference */
/* of the required node if found. Otherwise returns NULL      */
 
zgt_tx* get_tx(long tid1){  
  zgt_tx *txptr, *lastr1;
 
  if(ZGT_Sh->lastr != NULL){  // If the list is not empty
      lastr1 = ZGT_Sh->lastr; // Initialize lastr1 to first node's ptr
      for  (txptr = lastr1; (txptr != NULL); txptr = txptr->nextr)
      if (txptr->tid == tid1)     // if required id is found                  
         return txptr;
      return (NULL);      // if not found in list return NULL
   }
  return(NULL);       // if list is empty return NULL
}
 
/* Method that handles "BeginTx tid" in test file     */
/* Inputs a pointer to transaction id, obj pair as a struct. Creates a new  */
/* transaction node, initializes its data members and */
/* adds it to transaction list */
 
void *begintx(void *arg){
  //intialise a transaction object. Make sure it is
  //done after acquiring the semaphore for the tm and making sure that
  //the operation can proceed using the condition variable. When creating
  //the tx object, set the tx to TR_ACTIVE and obno to -1; there is no
  //semno as yet as none is waiting on this tx.
 
  struct param *node = (struct param*)arg;// get tid and count
  start_operation(node->tid, node->count);
    zgt_tx *tx = new zgt_tx(node->tid,TR_ACTIVE, node->Txtype, pthread_self()); // Create new tx node
 
    // Writes the Txtype to the file.
 
    zgt_p(0);       // Lock Tx manager; Add node to transaction list
 
    tx->nextr = ZGT_Sh->lastr;
    ZGT_Sh->lastr = tx;  
    zgt_v(0);       // Release tx manager
    fprintf(ZGT_Sh->logfile, "T%-4ld    %c    \tBeginTx\n", node->tid, node->Txtype); // Write log record and close
    fflush(ZGT_Sh->logfile);
  finish_operation(node->tid);
  pthread_exit(NULL);       // thread exit
}
 
/* Method to handle Readtx action in test file    */
/* Inputs a pointer to structure that contans     */
/* tx id and object no to read. Reads the object  */
/* if the object is not yet present in hash table */
/* or same tx holds a lock on it. Otherwise waits */
/* until the lock is released */
 
void *readtx(void *arg){
  struct param *node = (struct param*)arg;// get tid and objno and count
  start_operation(node->tid, node->count);
 
  zgt_tx *tx = get_tx(node->tid); //getting transaction
  // do the operations for reading. Write your code
  if (!tx)    // if transaction does not exist
  {
    pthread_exit(NULL);
  }
 
  if(tx->get_status() != 'P') // if transaction is not active
  {
    do_commit_abort_operation(node->tid, 'A');  // abort if inactive
    pthread_exit(NULL);
  }
 
  if (tx->set_lock(node->tid, 1, node->obno, node->count, 'S') != 0) // Acquire shared lock on read
  {
    pthread_exit(NULL);
  }
  finish_operation(node->tid); // Signal completion
  pthread_exit(NULL);
}
 
 
void *writetx(void *arg){ //do the operations for writing; similar to readTx
  struct param *node = (struct param*)arg;  // struct parameter that contains
  start_operation(node->tid, node->count);
 
  zgt_tx *tx = get_tx(node->tid); //getting transaction
  // do the operations for writing; similar to readTx. Write your
  if (!tx)    // if transaction does not exist
  {  
    pthread_exit(NULL);
  }
 
  if (tx->get_status() != 'P')    // if transaction is waiting, abort the transaction
  {  
    do_commit_abort_operation(node->tid, 'A');  
    finish_operation(node->tid);
    pthread_exit(NULL);
  }
 
  if (tx->set_lock(node->tid, 1, node->obno, node->count, 'X') != 0)  // Acquire exclusive lock on write
  {
    pthread_exit(NULL);
  }
  finish_operation(node->tid);
  pthread_exit(NULL);
}
 
// common method to process read/write: Just a suggestion
// void *process_read_write_operation(long tid, long obno,  int count, char mode){ }
 
void *aborttx(void *arg)
{
  struct param *node = (struct param*)arg;// get tid and count  
  start_operation(node->tid, node->count);
 
  zgt_tx *tx = get_tx(node->tid); //getting transaction
  //write your code
  if (!tx)    // if transaction does not exist
  {  
    pthread_exit(NULL);
  }
 
  do_commit_abort_operation(node->tid, 'A');  // Abort the transaction
  finish_operation(node->tid);
  pthread_exit(NULL);     // thread exit
}
 
void *committx(void *arg)
{
 
  //remove the locks/objects before committing
  struct param *node = (struct param*)arg;// get tid and count
  start_operation(node->tid, node->count);
 
  zgt_tx *tx = get_tx(node->tid); //getting transaction
 
  //write your code
  if (!tx)    // if transaction does not exist
  {  
    pthread_exit(NULL);
  }
  do_commit_abort_operation(node->tid, 'C'); // Commit the transaction
  finish_operation(node->tid);
  pthread_exit(NULL);     // thread exit
}
 
//suggestion as they are very similar
 
// called from commit/abort with appropriate parameter to do the actual
// operation. Make sure you give error messages if you are trying to
// commit/abort a non-existent tx
 
void *do_commit_abort_operation(long t, char status)
{
  // write your code
  zgt_tx* tx = get_tx(t);
  if (!tx)
  {  
    printf("::: Error: Transaction %ld does not exist.\n", t);
    fflush(stdout);
    pthread_exit(NULL);
  }
 
  // Log Commit or Abort
  fprintf(ZGT_Sh->logfile, "T%-4ld         \t%s\t", t, (status == 'C') ? "CommitTx" : "AbortTx ");
  fflush(ZGT_Sh->logfile);

  // Set transaction status
  tx->status = status;
 
  // Release all locks held by this transaction
  tx->free_locks();
 
  // Remove transaction entry
  int waitSemNum = tx->semno;
  tx->remove_tx();
 
  // If other transactions are waiting, wake one up
  if (waitSemNum > -1 && zgt_nwait(waitSemNum) > 0)
  {
    zgt_v(waitSemNum);
  }
  pthread_exit(NULL);
}
 
int zgt_tx::remove_tx ()
{
  //remove the transaction from the TM
 
  zgt_tx *txptr, *lastr1;
  lastr1 = ZGT_Sh->lastr;
  for(txptr = ZGT_Sh->lastr; txptr != NULL; txptr = txptr->nextr){  // scan through list
    if (txptr->tid == this->tid){   // if correct node is found          
     lastr1->nextr = txptr->nextr;  // update nextr value; done
     //delete this;
         return(0);
    }
    else lastr1 = txptr->nextr;     // else update prev value
   }
  fprintf(ZGT_Sh->logfile, "Trying to Remove a Tx:%ld that does not exist\n", this->tid);
  fflush(ZGT_Sh->logfile);
  printf("Trying to Remove a Tx:%ld that does not exist\n", this->tid);
  fflush(stdout);
  return(-1);
}
 
/* this method sets lock on objno1 with lockmode1 for a tx*/
 
int zgt_tx::set_lock(long tid1, long sgno1, long obno1, int count, char lockmode1)
{
  //if the thread has to wait, block the thread on a semaphore from the
  //sempool in the transaction manager. Set the appropriate parameters in the
  //transaction list if waiting.
  //if successful  return(0); else -1
 
  //write your code
  
  zgt_hlink *existingLock = ZGT_Ht->findt(tid1, sgno1, obno1); // Check if transaction already holds this lock
  if (existingLock && existingLock->tid == this->tid) {  
    this->lockmode = lockmode1;
    this->perform_read_write_operation(tid1, obno1, lockmode1);
    return 0;
  }
 
  zgt_hlink *otherTxnLock = ZGT_Ht->find(sgno1, obno1);
  // If no other transaction holds a lock, acquire it immediately
  if (!otherTxnLock || (otherTxnLock->lockmode == 'S' && lockmode1 == 'S')) 
  {  
    if (ZGT_Ht->add(this, sgno1, obno1, lockmode1) >= 0) {
        this->perform_read_write_operation(tid1, obno1, lockmode1);
        return 0;
    } else {
        printf("\nError: Failed to insert lock into Lock Table.\n");
        fflush(stdout);
        return -1;
    }
  }

  // If another transaction holds X lock when write wants the lock, log "Not Granted"
  if (lockmode1 == 'X' && otherTxnLock->lockmode == 'X')
  {
    fprintf(ZGT_Sh->logfile, "T%-4ld         \tWriteTx  \t%ld:X:X              \tWriteLock\tNotGranted\tW for T%ld\n", this->tid, obno1, otherTxnLock->tid);
    fflush(ZGT_Sh->logfile);
  }
  // If another transaction holds S lock when write wants the lock, log "Not Granted"
  if (lockmode1 == 'X' && otherTxnLock->lockmode == 'S')
  {
    fprintf(ZGT_Sh->logfile, "T%-4ld         \tWriteTx  \t%ld:X:X              \tWriteLock\tNotGranted\tW for T%ld\n", this->tid, obno1, otherTxnLock->tid);
    fflush(ZGT_Sh->logfile);
  }
  // If another transaction holds X lock when read wants the lock, log "Not Granted"
  if (lockmode1 == 'S' && otherTxnLock->lockmode == 'X')
  {
    fprintf(ZGT_Sh->logfile, "T%-4ld         \tReadTx   \t%ld:X:X              \tReadLock \tNotGranted\tW for T%ld\n", this->tid, obno1, otherTxnLock->tid);
    fflush(ZGT_Sh->logfile);
  }
 
  // Setting transaction to end state
  this->status = TR_WAIT;
  this->lockmode = lockmode1;
  this->obno = obno1;
  this->setTx_semno(otherTxnLock->tid, otherTxnLock->tid);
 
  // No deadlock detection, Just wait for the lock to be released
  zgt_p(otherTxnLock->tid);  
  this->status = TR_ACTIVE;

  // Wake up all waiting transactions after lock is released
  for (int i = 0; i < ZGT_Nsema; i++) 
  { 
    if (zgt_nwait(i) > 0) 
    {
        zgt_v(i);  // Wake up waiting transactions
    }
  }
 
  return set_lock(this->tid, sgno1, obno1, count, lockmode1);
}


 
int zgt_tx::free_locks()
{
 
  // this part frees all locks owned by the transaction
  // that is, remove the objects from the hash table
  // and release all Tx's waiting on this Tx
 
  zgt_hlink* temp = head;  //first obj of tx
  print_tm();
  ZGT_Ht->print_ht();
  for(temp;temp != NULL;temp = temp->nextp){  // SCAN Tx obj list
 
      fprintf(ZGT_Sh->logfile, "%ld : %d, ", temp->obno, ZGT_Sh->objarray[temp->obno]->value);
      fflush(ZGT_Sh->logfile);
     
      if (ZGT_Ht->remove(this,1,(long)temp->obno) == 1){
     printf(":::ERROR:node with tid:%ld and onjno:%ld was not found for deleting", this->tid, temp->obno);    // Release from hash table
     fflush(stdout);
      }
      else {
#ifdef TX_DEBUG
     printf("\n:::Hash node with Tid:%ld, obno:%ld lockmode:%c removed\n",
                            temp->tid, temp->obno, temp->lockmode);
     fflush(stdout);
#endif
      }
    }
  fprintf(ZGT_Sh->logfile, "\n");
  fflush(ZGT_Sh->logfile);
 
  return(0);
}  
 
// CURRENTLY Not USED
// USED to COMMIT
// remove the transaction and free all associate dobjects. For the time being
// this can be used for commit of the transaction.
 
int zgt_tx::end_tx()  
{
  zgt_tx *linktx, *prevp;
 
  // USED to COMMIT
  //remove the transaction and free all associate dobjects. For the time being
  //this can be used for commit of the transaction.
 
  linktx = prevp = ZGT_Sh->lastr;
 
  while (linktx){
    if (linktx->tid  == this->tid) break;
    prevp  = linktx;
    linktx = linktx->nextr;
  }
  if (linktx == NULL) {
    printf("\ncannot remove a Tx node; error\n");
    fflush(stdout);
    return (1);
  }
  if (linktx == ZGT_Sh->lastr) ZGT_Sh->lastr = linktx->nextr;
  else {
    prevp = ZGT_Sh->lastr;
    while (prevp->nextr != linktx) prevp = prevp->nextr;
    prevp->nextr = linktx->nextr;    
  }
  return 0;   // always return a value (fixes make warning)
}
 
// currently not used
int zgt_tx::cleanup()
{
  return(0);
 
}
 
// routine to print the tx list
// TX_DEBUG should be defined in the Makefile to print
void zgt_tx::print_tm(){
 
  zgt_tx *txptr;
 
#ifdef TX_DEBUG
  printf("printing the tx  list \n");
  printf("Tid\tTxType\tThrid\t\tobjno\tlock\tstatus\tsemno\n");
  fflush(stdout);
#endif
  txptr=ZGT_Sh->lastr;
  while (txptr != NULL) {
#ifdef TX_DEBUG
    printf("%ld\t%c\t%ld\t%ld\t%c\t%c\t%d\n", txptr->tid, txptr->Txtype, txptr->pid, txptr->obno, txptr->lockmode, txptr->status, txptr->semno);
    fflush(stdout);
#endif
    txptr = txptr->nextr;
  }
  fflush(stdout);
}
 
//need to be called for printing
void zgt_tx::print_wait(){
 
  //route for printing for debugging
 
  printf("\n    SGNO        TxType       OBNO        TID        PID         SEMNO   L\n");
  printf("\n");
}
 
void zgt_tx::print_lock(){
  //routine for printing for debugging
 
  printf("\n    SGNO        OBNO        TID        PID   L\n");
  printf("\n");
 
}
 
// routine to perform the actual read/write operation as described the project description
// based  on the lockmode
 
void zgt_tx::perform_read_write_operation(long tid,long obno, char lockmode){
 
  // write your code
    int objectValue = ZGT_Sh->objarray[obno]->value; // Get current value
 
    if (lockmode == 'X') {  // Write operation
        ZGT_Sh->objarray[obno]->value = objectValue + 7; // Increase by 7 (as per project spec)
        fprintf(ZGT_Sh->logfile, "T%-4ld         \tWriteTx  \t%ld:%d:%-14d\tWriteLock\tGranted\t\t%c\n",
                this->tid, obno, ZGT_Sh->objarray[obno]->value, ZGT_Sh->optime[tid], this->status);
        fflush(ZGT_Sh->logfile);
        usleep(ZGT_Sh->optime[tid] * 10);  // Simulate operation delay
    }
    else {  // Read operation
        ZGT_Sh->objarray[obno]->value = objectValue - 4; // Decrease by 4 (as per project spec)
        fprintf(ZGT_Sh->logfile, "T%-4ld         \tReadTx   \t%ld:%d:%-14d\tReadLock \tGranted\t\t%c\n",
                this->tid, obno, ZGT_Sh->objarray[obno]->value, ZGT_Sh->optime[tid], this->status);
        fflush(ZGT_Sh->logfile);
        usleep(ZGT_Sh->optime[tid] * 10);  // Simulate operation delay
    }
 
}
 
// routine that sets the semno in the Tx when another tx waits on it.
// the same number is the same as the tx number on which a Tx is waiting
int zgt_tx::setTx_semno(long tid, int semno){
  zgt_tx *txptr;
 
  txptr = get_tx(tid);
  if (txptr == NULL){
    printf("\n:::ERROR:Txid %ld wants to wait on sem:%d of tid:%ld which does not exist\n", this->tid, semno, tid);
    fflush(stdout);
    exit(1);
  }
  if ((txptr->semno == -1)|| (txptr->semno == semno)){  //just to be safe
    txptr->semno = semno;
    return(0);
  }
  else if (txptr->semno != semno){
#ifdef TX_DEBUG
    printf(":::ERROR Trying to wait on sem:%d, but on Tx:%ld\n", semno, txptr->tid);
    fflush(stdout);
#endif
    exit(1);
  }
  return(0);
}
 
void *start_operation(long tid, long count)
{
  pthread_mutex_lock(&ZGT_Sh->mutexpool[tid]);  // Lock mutex[t] to make other
  // threads of same transaction to wait
 
  while(ZGT_Sh->condset[tid] != count)    // wait if condset[t] is != count
    pthread_cond_wait(&ZGT_Sh->condpool[tid],&ZGT_Sh->mutexpool[tid]);
 
  return NULL;  //fixes warning in make
}
 
// Otherside of teh start operation;
// signals the conditional broadcast
 
void *finish_operation(long tid){
  ZGT_Sh->condset[tid]--; // decr condset[tid] for allowing the next op
  pthread_cond_broadcast(&ZGT_Sh->condpool[tid]);// other waiting threads of same tx
  pthread_mutex_unlock(&ZGT_Sh->mutexpool[tid]);
 
  return NULL;  // fixes warning in make
}