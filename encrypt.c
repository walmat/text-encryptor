/**

   MATTHEW WALL • IOWA STATE UNIVERSITY • SPRING 2017

   Project 2 for ISU COM S 352 – Operating Systems (Written in C)

   Description: 
   
   The purpose of this project is to implement a multi-threaded text file
   encryptor. Conceptually, the function of the program is simple: to read
   characters from the input file, encrypt the letters, and write the 
   encrypted characters to the output file. Also, the encryption program 
   counts the number of occurrences of each letter in the input and output
   files.

   Files:

   • encrypt.c – this file
   • Makefile – compiles and makes the executable

   Usage: 

   1) run the encryptor on two files: example -> ./encrypt [infile] [outfile]
   2) provide a buffer size (int): example -> 5

   Synchronization:

   this program has semaphores and concurrency to prevent spinlocks and provide
   synchronization between threads. This is to allow maximum concurrency, as 
   different threads can operate on different buffer slots concurrectly.

 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct node node;

/**

   A single instance of an item inside of the buffer. 

   @char c – character value representing this
   @int has_been_counted – whether or not the instance has been counted
   @int has_been_encrypted – whether or not the instance has been encrypted

 */
struct node {

  char c;
  int has_been_counted;
  int has_been_encrypted;

  node* past;

};


/**

   implementation of a priority queue for the buffer

   @n* front – the first element in the priority queue 
   @n* back – the last element in the priority queue
   @int max – the max size of the buffer
   @int curr – the respective current size of the buffer

 */
typedef struct {

  node* front;
  node* back;
  
  int max;
  int curr;
  
} priority_queue;

/**
   PROTOTYPE FUNCTION DECLARATIONS
   
   @a - arguments passed into the function
   
 */

void *count_input(void *a);
void *read_input(void *a);
void *input_encrypt(void *a);
void *count_output(void *a);
void *write_output(void *a);
//int is_character(char c);
void outlog(char *text);

/**
   GLOBAL CONSTANTS
 */

int in_count[255];
int out_count[255];
int size;

priority_queue input_buffer;
priority_queue output_buffer;

sem_t read_in;
sem_t write_out;
sem_t input_count;
sem_t output_count;
sem_t encrypt_input;
sem_t encrypt_output;

FILE *in_file;
FILE *out_file;

int toLog = 0;

int main(int argc, char **argv) {

  //list of threads we need to keep track of
  pthread_t in, count_in, encrypt, count_out, out;

  //check to see if the number of command line argument is correct (3)
  if (argc != 3) {
    printf("Invalid number of arguments.\n   Usage: ./encrypt [infile] [outfile] \n");
    exit(-1); //exit with an error
  }

  //correct number of arguments, file check now
  in_file = fopen(argv[1], "r");
  out_file = fopen(argv[2], "w");

  //if we correctly opened a file for reading then we can continue
  if (in_file != NULL) {
    
    printf("Please enter a buffer size: ");
    fflush(stdout);

    char buff_size[256];
    fgets(buff_size, 256, stdin);
    buff_size[strlen(buff_size) - 1] = '\0';

    size = atoi(buff_size);
    
    /**
       init all the global variables
     */
    input_buffer.max = size;
    input_buffer.curr = 0;
    output_buffer.max = size;
    output_buffer.curr = 0;

    sem_init(&read_in, 0, 1);
    sem_init(&write_out, 0, 0);
    sem_init(&input_count, 0, 0);
    sem_init(&output_count, 0, 0);
    sem_init(&encrypt_input, 0, 0);
    sem_init(&encrypt_output, 0, 1);
    
    pthread_create(&in, NULL, read_input, NULL);
    pthread_create(&count_in, NULL, count_input, NULL);
    pthread_create(&encrypt, NULL, input_encrypt, NULL);
    pthread_create(&count_out, NULL, count_output, NULL);
    pthread_create(&out, NULL, write_output, NULL);

    pthread_join(in, NULL);
    pthread_join(count_in, NULL);
    pthread_join(encrypt, NULL);
    pthread_join(count_out, NULL);
    pthread_join(out, NULL);
    
    printf("input file contains: \n");

    //print out the input file contents
    for (int i = 0; i < 255; i++) {
      if (((char) i) != '\n' && in_count[i] > 0) {//&& is_character((char) i)) {
	//not sure whether or not we're supposed to print out non alphabetic
	//characters here as well, so i'm just going to anyway
	printf("%c: %d\n", (char) i, in_count[i]);
      }
    }

    printf("output file contains: \n");
    //print out the output file contents
    for (int i = 0; i < 255; i++) {
      if (((char) i) != '\n' && out_count[i] > 0){//&& is_character((char) i)) {
	//not sure whether or not we're supposed to print out non alphabetic
	//characters here as well, so i'm just going to anyway
	printf("%c: %d\n", (char) i, out_count[i]);
      }
    }

    return 1; //success
    
  } else {
    printf("File not found for given name: %s \n", argv[1]);
    exit(-1); //exit with an error
  }
}

int is_character(char c) {
  return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

/**

   Moves the object up one slot in the priority queue 

   @priority_queue queue – the queue to move the character forward in
   @char c – the character to be enqueued

 */
int enqueue(priority_queue *queue, char c) {

  //check to see if the queue is full
  if (queue->curr == queue->max) {
    return 0;
  }

  node* ch = (node*) malloc(sizeof(node));
  ch->has_been_counted = 0;
  ch->has_been_encrypted = 0;
  ch->c = c;

  //if nothing is in the queue so far
  if (queue->curr == 0) {
    queue->front = ch;
    queue->back = ch;
  } else { //otherwise set ch's past equal to the back  and put ch at the back
    queue->back->past = ch;
    queue->back = ch;
  }
  queue->curr++;
  return 1;
}

/**

  Rremoves the object from the buffer

  @priority_queue queue – the queue to remove the object from

 */
node *dequeue(priority_queue *queue) {

  //nothing to remove from the queue
  if (queue->curr == 0) {
    return (node*) NULL;
  }

  //otherwise remove the front
  node *n = (node*) malloc(sizeof(node));
  n->c = queue->front->c;
  n->has_been_counted = queue->front->has_been_counted;
  n->has_been_encrypted = queue->front->has_been_encrypted;

  //"pop" the front of the queue off
  queue->front = queue->front->past;

  //check to see if there is only one element in the queue
  //and set the back equal to null if so
  if (queue->curr == 1) {
    queue->back = NULL; 
  }
  queue->curr--; //decrement the num of nodes
  return n; //return the new node
}

/**

   Encryption of data. Here is the algorithm laid out very plainly.


   1) s = 1;
   2) Get next character c.
   3) if c is not a letter then goto (7).
   4) if (s==1) then increase c with wraparound (e.g., 'A' becomes 'B', 'c' becomes 'd', 
      'Z' becomes 'A', 'z' becomes 'a'), set s=-1, and goto (7).
   5) if (s==-1) then decrease c with wraparound (e.g., 'B' becomes 'A', 'd' becomes 'c',
      'A' becomes 'Z', 'a' becomes 'z'), set s=0, and goto (7).
   6) if (s==0), then do not change c, and set s=1.
   7) Encrypted character is c.
   8) If c!=EOF then goto (2).

 */

char encrypt(char c, int *s) {

  int int_value = (int) c;

  //we're only encrypting if the character is a letter.. soooo checking for that
  if ((int_value >= 65 && int_value <= 90) || (int_value >= 97 && int_value <= 122)) {

    switch(*s) {
      
    case -1: //decrement with wrap around functionality
      *s = 0;
      int_value = (int_value == 65) ? 90 :
	(int_value == 97) ? 122 :
	int_value - 1;
      return (char) int_value; 
    case 0: //we don't need to do anything
      *s = 1;
      return c;
    case 1: //increment with wrap around functionality
      *s = -1;
      int_value = (int_value == 90) ? 65 :
	(int_value == 122) ? 97 :
	int_value + 1;
      return (char) int_value;
    }
  } else { return c; } //it's not a letter so don't encrypt it
}

void *input_encrypt(void *a){

  node *curr;
  node *temp;

  int initial_s = 1;

  for(;;) {

    sem_wait(&encrypt_input);
    outlog("input encryption\n");

    curr = input_buffer.front;

    while (NULL != curr) {

      if (curr->has_been_counted && !curr->has_been_encrypted) {
	if (curr->c != EOF && curr->c != '\n') {
	  curr->c = encrypt(curr->c, &initial_s);
	}
	curr->has_been_encrypted = 1;
	outlog("encrypted input\n");
	break;
      }
      curr = curr->past;
    }
    if (input_buffer.curr > 0 && input_buffer.front->has_been_encrypted) {
      temp = dequeue(&input_buffer);
      sem_post(&read_in);
    }
    sem_wait(&encrypt_output);
    enqueue(&output_buffer, temp->c);
    outlog("Sent to output\n");

    sem_post(&output_count);

    if (temp->c == EOF) {
      outlog("Encrypting done\n");
      break;
    }
  }
}

/**


 */

void *count_output(void *a) {

  node *curr;

  for(;;) {

    sem_wait(&output_count);
    curr = output_buffer.front;

    outlog("output counting\n");
    while (NULL != curr) {

      if (!curr->has_been_counted) {

	out_count[curr->c]++;
	curr->has_been_counted = 1;
	sem_post(&write_out);
	outlog("Counted output\n");

	if (curr->c == EOF) {

	 outlog("Counting output done\n");
	  return (void*) NULL;
	} else {
	  break;
	} 
      } else {
	curr = curr->past;
      }
    }
  }
}

void *count_input(void *a) {

  node *curr;

  for(;;) {

    sem_wait(&input_count);
    curr = input_buffer.front;
    outlog("input counting\n");
    while (NULL != curr) {
      if (curr->has_been_counted == 0) {
	in_count[curr->c]++;
	curr->has_been_counted = 1;
	sem_post(&encrypt_input);
	outlog("Counted input\n");

	if(curr->c == EOF) {
	  outlog("Counting input done\n");
	  return (void*) NULL;
	} else {
	  break;
	}
      } else {
	curr = curr->past;
      }
    }
  }
}

void *read_input(void *a) {

  char curr;

  curr = fgetc(in_file);

  for(;;) {
    sem_wait(&read_in);

    if (enqueue(&input_buffer, curr)) {
      outlog("Char put into queue\n");
      sem_post(&input_count);

      if(curr == EOF) {
	outlog("Done reading input\n");
	break;
      } else {
	curr = fgetc(in_file);
      }
    }
  }
}

void *write_output(void *a) {
  
  node *curr;

  for(;;) {
    sem_wait(&write_out);
    curr = output_buffer.front;

    if (curr->has_been_counted) {
      dequeue(&output_buffer);
      
      if (curr->c == EOF) {
	break;
      }

      fputc(curr->c, out_file);
      fflush(out_file);
      curr = curr->past;
    }
    sem_post(&encrypt_output);
  }
  outlog("Done writing output\n");
}

/**

   Helper method to be used solely for debugging. Displays exactly what is wrong.

   @char *text – the string to print out
 */
void outlog(char *text) {
  if (toLog) {
    printf("%s",text);
  }
}
