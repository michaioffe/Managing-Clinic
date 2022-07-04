#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>

#define N 10
#define SIZE_OF_SOFA 4
#define NUMBER_OF_DEN 3

// a node in a linked list
struct Node {
	int id;
	struct Node* next;
	struct Node* prev;
};

// basic linked list struct
struct LinkedList {
	int size;
	struct Node* head;
	struct Node* tail;
};

// function declarations
void* enter_clinic(void* );
void sit_on_sofa(int );
void get_treatment(int );
void payment(int );
void give_treatment(int );
void take_payment(int );
void* den_sleep(void* );
void insert_to_head(struct LinkedList* , int );
void delete_from_tail(struct LinkedList* );
void wait_to_enter(int );
struct LinkedList create_list();
void wait_to_sit(int );

// semaphores for syncing threads
sem_t den_hygienist; //semaphore for denHygienist
sem_t patients; //seamphore for each patients
sem_t mutex; //for critical section
sem_t sofa; //each patient can sit on three sits on the sofa
sem_t treatment_chairs; //each patient can sit on the treatment chairs (3 chairs)
sem_t paying; //after the denHygienist done the treatment, the patient need to pay
sem_t payed; //samphore for the denHygienist so she can take the payment
sem_t cashBox; //we have ONLY one cashBox in the clinic

// global lists and variables
struct LinkedList sitting;
struct LinkedList standing;
int waiting = 0;
int working_den = 0;

// This program simulates a dental clinic that has NUMBER_OF_DEN dental hygienists,
// a sofa which has room for SIZE_OF_SOFA patients and overall a capacity for N patients
// in the clinic.
// The simulation is done by using threads and syncing them with semaphores.
void main() {
	pthread_t patients_t[N + 2];
	pthread_t den_t[NUMBER_OF_DEN];
	sem_init(&den_hygienist, 0, 0);
	sem_init(&patients, 0, N);
	sem_init(&mutex, 0, 1);
	sem_init(&sofa, 0, SIZE_OF_SOFA);
	sem_init(&treatment_chairs, 0, NUMBER_OF_DEN);
	sem_init(&paying, 0, 0);
	sem_init(&payed, 0, 0);
	sem_init(&cashBox, 0, 1);

	// create the linked lists
	sitting = create_list();
	standing = create_list();
	int ans, i, index_p[N + 2], index_d[NUMBER_OF_DEN];

	// create N + 2 threads for patients
	for (i = 0; i < N + 2; i++) {
		index_p[i] = i+1;
		ans = pthread_create(&patients_t[i], NULL, enter_clinic, (void*)&index_p[i]);
		if (ans != 0) {
			printf("Error! couldn't create patient thread!\n");
			exit(1);
		}
	}
	
	// create NUMBER_OF_DEN threads for dental hygienists
	for (i = 0; i < NUMBER_OF_DEN; i++) {
		index_d[i] = i + 1;
		ans = pthread_create(&den_t[i], NULL, den_sleep, (void*)&index_d[i]);
		if (ans != 0) {
			printf("Error! couldn't create dentist thread!\n");
			exit(1);
		}
	}

	// wait for threads to end
	for (i = 0; i < N + 2; i++)
		pthread_join(patients_t[i],NULL);
	for (i = 0; i < NUMBER_OF_DEN ; i++)
		pthread_join(den_t[i], NULL);

}

// This function gets a linked list and deletes the tail.
// It also handles different cases.
void delete_from_tail(struct LinkedList* list)
{	
	int* size = &((*list).size);
	struct Node** tail = &((*list).tail);
	struct Node* tail_to_del = *tail;
	struct Node** head = &((*list).head);
    /* base case */
	if (*size == 0)
		return;

	if (*size == 1) {
		(*size)--;
		*tail = NULL;
		*head = NULL;
	}
    /* Change prev only if node to be deleted is NOT the first node */
	else {
		*tail = (*tail)->prev;
		(*size)--;
	}
        
    /* Finally, free the memory occupied by del*/
    free(tail_to_del);
    return;
}

// create an empty list
struct LinkedList create_list() {
	struct LinkedList new_list;
	new_list.head = NULL;
	new_list.tail = NULL;
	new_list.size = 0;

	return new_list;
}

// This function gets a linked list, creates and inserts a new node to the head.
// It also handles different cases.
void insert_to_head(struct LinkedList* list, int id) {
	// create new node
	struct Node* new_node = (struct Node*)malloc(sizeof(struct Node));
	new_node->id = id;
	new_node->prev = NULL;
	int size = (*list).size;

	// if empty
	if (size == 0) {
		(*list).tail = new_node;
	}
	else if (size == 1) { // if only one node in list
		(*list).tail->prev = new_node;
		(*list).head->prev = new_node;
		new_node->next = (*list).head;
	}
	else { // if more than one
		(*list).head->prev = new_node;
		new_node->next = (*list).head;
	}

	// insert new node as the new head of the list
	(*list).head = new_node;
	(*list).size++;
}

// Patients start here - first, check if there is room in the clinic, if not, go to wait_to_enter()
// If yes, declare that patient entered the clinic and check if there is room on the sofa to sit on.
void* enter_clinic(void* id) {
	int currentId = (*(int*)id); // get id
	while (1) {
		if (waiting == N)
			wait_to_enter(currentId);
		sem_wait(&patients);

		sem_wait(&mutex);
		waiting++;
		printf("I'm Patient #%d, I got into the clinic\n", currentId);
		sem_post(&mutex);
		sleep(1);

		// if not enough room on sofa
		if (sitting.size == SIZE_OF_SOFA)
			wait_to_sit(currentId);
		sit_on_sofa(currentId);
	}
}

// Function that enters patient to a standing queue and waits until he is the last
// in the queue and there is room on the sofa.
void wait_to_sit(int id) {
	sem_wait(&mutex);
	insert_to_head(&standing, id);
	sem_post(&mutex);

	while (standing.tail->id != id);
	while (sitting.size == SIZE_OF_SOFA);

	sem_wait(&mutex);
	delete_from_tail(&standing);
	sem_post(&mutex);
}

// Wait in while until there is room in clinic
void wait_to_enter(int id) {
	printf("I'm Patient #%d, I'm out of clinic\n",id);
	while (waiting == N);
}

// Function that enters patient into a sitting queue. When he is the last on the sofa,
// go and wait for a treatment.
void sit_on_sofa(int id) {
	sem_wait(&sofa);

	sem_wait(&mutex);
	insert_to_head(&sitting, id);
	printf("I'm Patient #%d, I'm sitting on the sofa\n", id);
	sem_post(&mutex);
	sleep(1);

	while (sitting.tail->id != id);
	get_treatment(id);
}

// Wait for a den to be free and then wake up a dentist to give the patient a treatment.
// After getting a treatment, go to pay.
void get_treatment(int id) {
	while (working_den == NUMBER_OF_DEN); // wait for den to be free
	sem_wait(&treatment_chairs);

	sem_wait(&mutex);
	delete_from_tail(&sitting);
	sem_post(&sofa);
	printf("I'm Patient #%d, I'm getting treatment\n", id);
	sem_post(&den_hygienist);
	sem_post(&mutex);
	sleep(1);

	payment(id);
}

// Wait for a dentist to be free to take a payment - there is only one cash
// register.
void payment(int id) {
	sem_wait(&paying);
	sem_wait(&cashBox);
	sem_wait(&mutex);
	printf("I'm Patient #%d, I'm paying now\n", id);
	sem_post(&payed);
	sem_post(&mutex);
	sleep(1);
}

// Dentists start here - wait for a patient to wake up the dentist
// and then give treatment.
void* den_sleep(void* id) {
	int currentId = (*(int*)id);

	while (1) {
		sleep(1);
		sem_wait(&den_hygienist);
		give_treatment(currentId);
	}
}

// Give a treatment to a patient
void give_treatment(int id) {
	sem_wait(&mutex);
	working_den++;
	printf("I'm Dental Hygienist #%d, I'm working now\n", id);
	sem_post(&paying);
	sem_post(&mutex);
	take_payment(id);
}

// Recieve payment from a patient
void take_payment(int id) {
	sem_wait(&payed);
	sem_wait(&mutex);
	printf("I'm Dental Hygienist #%d, I'm getting a payment\n",id);
	working_den--;
	waiting--;
	sem_post(&cashBox);
	sem_post(&patients);
	sem_post(&treatment_chairs); 
	sem_post(&mutex);
	sleep(1);
}