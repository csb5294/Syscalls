#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/kernel.h>
#include <time.h>

#define BUFFER_SIZE 20

struct timespec current_time;
struct timespec start_time;
double elapsed_time;

//cs1550_sem struct definition
struct cs1550_sem
{
	int vlaue;
	struct mutex *lock;
	struct list_node *list_head;
};

//init syscall macro
void sem_init(struct cs1550_sem *sem, int value){
	syscall(441, sem, value);
}

//down syscall macro
void down(struct cs1550_sem *sem){
	syscall(442, sem);
}

//up syscall macro
void up(struct cs1550_sem *sem){
	syscall(443, sem);
}

//Queue struct definition
typedef struct
{
	int buffer[BUFFER_SIZE];
	int in;
	int out;
	int car_num;
	int counter;
	struct cs1550_sem mutex;
	struct cs1550_sem full;
	struct cs1550_sem empty;
} Queue;

//Road struct definition
typedef struct
{
	Queue north_queue;
	Queue south_queue;
	struct cs1550_sem flag_sem;
	int num_cars;
	char flag_dir;
} Road;

//Method to initialize the queue semaphores
void queue_init(Queue *queue){

	queue->in = 0;
	queue->out = 0;
	queue->car_num = 0;
	queue->counter = 0;
	sem_init(&queue->mutex, 1);
	sem_init(&queue->full, 0);
	sem_init(&queue->empty, BUFFER_SIZE);

}

void enqueue(Queue *queue, char direction){

	//empty down
	down(&queue->empty);
	//mutex down
	down(&queue->mutex);

	//add to buffer and increment car_num
	queue->buffer[queue->in] = queue->car_num++;

	//output
	printf("Car %d coming from the %c direction", queue->car_num, direction);

	//enqueue car
	queue->in = (queue->in + 1) % BUFFER_SIZE;
	queue->counter++;

	//mutex up
	up(&queue->mutex);
	//full up
	up(&queue->full);

}

void dequeue(Queue *queue, char direction){

	//full down
	down(&queue->full);
	//mutex down
	down(&queue->mutex);

	///output
	printf("Car %d coming from the %c direction", queue->car_num, direction);

	//dequeue car
	queue->out = (queue->out + 1) % BUFFER_SIZE;
	queue->counter--;

	//mutex up
	up(&queue->mutex);
	//empty up
	up(&queue->empty);

}

//Producer/Road
void producer(char direction, Road *road){

	while(1){

		Queue *queue;
		int r;

		//set queue based on direction
		if(direction == 'N')
			queue = &road->north_queue;
		else if(direction == 'S')
			queue = &road->south_queue;
		else
			continue;

		do{
			//enqueue new car
			enqueue(queue, direction);

			//increment num_cars
			road->num_cars++;

			//if num_cars is now 1, flagperson was sleeping
			if(1 == road->num_cars){
				clock_gettime(CLOCK_MONOTONIC, &current_time);
				elapsed_time = (current_time.tv_sec - start_time.tv_sec) +
				(double)(current_time.tv_nsec - start_time.tv_nsec) / 1e9;
				printf(", blew their horn at time %f\n", elapsed_time);
				road->flag_dir = direction;
				up(&road->flag_sem);
				printf("The flagperson is now awake.\n");
			}
			else{
				clock_gettime(CLOCK_MONOTONIC, &current_time);
				elapsed_time = (current_time.tv_sec - start_time.tv_sec) +
				(double)(current_time.tv_nsec - start_time.tv_nsec) / 1e9;
				printf(" arrived in the queue at time %f\n", elapsed_time);
			}

			//new random number
			r = rand();

		}while(r % 100 < 75 && queue->counter < 20);

		sleep(8);

	}
}



//Consumer/Flagperson
void consumer(Road *road){

	Queue *queue;
	Queue *other_queue;

	while(1){

		//no cars in either queue so flagperson falls asleep
		if(0 == road->num_cars){
			down(&road->flag_sem);
			printf("The flagperson is now asleep.\n");
		}

		//flagperson facing north
		if('N' == road->flag_dir){
			queue = &road->north_queue;
			other_queue = &road->south_queue;
		}
		//flagperson facing south
		else{
			queue = &road->south_queue;
			other_queue = &road->north_queue;
		}

		while(queue->counter > 0){

			//dequeue car
			sleep(1);
			dequeue(queue, road->flag_dir);
			clock_gettime(CLOCK_MONOTONIC, &current_time);
			elapsed_time = (current_time.tv_sec - start_time.tv_sec) +
					(double)(current_time.tv_nsec - start_time.tv_nsec) / 1e9;
			printf(" left the construction zone at time %f\n", elapsed_time);

			//check if other que has 8 or more cars
			if(other_queue->counter >= 8){
				//if so switch flag direction
				if(road->flag_dir == 'N')
					road->flag_dir = 'S';
				else
					road->flag_dir = 'N';

				break;
			}
		}

		//once one queue is empty switch directions
		if(road->flag_dir == 'N')
			road->flag_dir = 'S';
		else
			road->flag_dir = 'N';
	}
}

int main(){

	clock_gettime(CLOCK_MONOTONIC, &start_time);

	//Road initialization
	Road *road = mmap(NULL, sizeof(Road), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	if(road == MAP_FAILED){
		perror("mmap failed");
		return 1;
	}

	//Semaphore initializations
	queue_init(&road->north_queue);
	queue_init(&road->south_queue);
	sem_init(&road->flag_sem, 1);
	road->num_cars = 0;
	road->flag_dir = 'N';

	pid_t north_pid, south_pid, flag_pid;

	//fork processes here
	north_pid = fork();
	if(north_pid < 0){
		perror("North fork failed");
		return 1;
	}else if(north_pid == 0){
		producer('N', road);
		return 0;
	}

	south_pid = fork();
	if(south_pid < 0){
		perror("South fork failed");
		return 1;
	}else if(south_pid == 0){
		producer('S', road);
		return 0;
	}

	flag_pid = fork();
	if(flag_pid < 0){
		perror("Flag fork failed");
		return 1;
	}else if(flag_pid == 0){
		consumer(road);
		return 0;
	}

return 0;

}
