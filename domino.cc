# include <unistd.h>

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

#include <iostream>
#include <set>
#include <vector>
#include <utility>
#include <algorithm>

using namespace std;

#define N 63
#define PLAYERS 32

//#define next(x) ((x+PLAYERS-1)%PLAYERS)
#define next(x) ((x+1)%PLAYERS)

typedef pair <int,int> intpair;
typedef set <intpair> pairset;
typedef vector <intpair> pairvector;

void * player (void *);
pairset * draw();
intpair update(intpair);
int starter(pairset *);
int islocked();
int points(pairset);

intpair choosesmaller(pairset);

/* Syncronization */

sem_t * mutexes;
pthread_mutex_t jugando;

/* Shared resources */

int fin = 0;
int score[PLAYERS] = {0};

pairvector table;


intpair edges = intpair (N-1,N-1);

/* Arguments passed to the thread */

typedef struct {
    int x;
    pairset * xs;
    intpair (*fnc)(pairset);
} param;

/* Entry point */

int main() {
    int i;
    pairset * players;
    pairset :: iterator it;

    mutexes = new sem_t [PLAYERS];
    for(i = 0; i < PLAYERS; ++i){
        sem_init(mutexes+i, 0, 0);
    }

    pthread_mutex_init(&jugando, NULL);

    pthread_t *threads;
    threads = new pthread_t [PLAYERS];

    param * p;
    p = new param [PLAYERS];

    players = draw();

    for(i = 0; i < PLAYERS; ++i){
        (p+i)->x = i;
        (p+i)->xs = players+i;
        (p+i)->fnc = choosesmaller; // don't set it here to have multiple AI
        pthread_create(&threads[i], NULL, player, (void *) (p+i));
    }

    /* show the players' pieces */
    for(i = 0; i < PLAYERS; ++i){
        cout << "\n--- PLAYER " << i << " ---\n";
        for(it = (players + i)->begin(); it != (players + i)->end(); ++it){
            cout << " (" << it->first << ", " << it->second << ") ";
        }
    }
    cout << "\n";

    // unlocks the player with the highest piece
    sem_post(mutexes+starter(players));

    // for multiple meta-threads of execution, unlock someone else too
    // if only one meta-thread, the player on the right will always play next
    // if many, it has a chance of one of them going faster (maybe fun)
    // sem_post(mutexes+(starter(players)+PLAYERS/2)%PLAYERS);

    for(i = 0; i < PLAYERS; ++i){
        pthread_join(threads[i], NULL);
    }

    // all threads ended, show score
    for(i = 0; i < PLAYERS; ++i){
        cout << "player " << i << ": -" << score[i] << endl;
    }

    delete[] players;

    delete[] p;
    delete[] threads;
    delete[] mutexes;

    return 0;
}

pairset * draw() {
    int i, j;
    time_t seed;
    pairset * players;

    players = new pairset [PLAYERS];

    pairvector ys;
    pairvector :: iterator it;

    for(i = 0; i < N; ++i){
        for(j = i; j < N; ++j){
            ys.push_back(intpair (i,j));
        }
    }

    time(&seed);
    srandom(seed); // seeds 'random'
    srand(seed); // seeds 'random_shuffle'
    random_shuffle(ys.begin(), ys.end());

    for(it = ys.begin(), i = 0; it != ys.end(); ++it, ++i){
        (players + (i%PLAYERS))->insert(*it);
    }

    return players;
}

/* returns the number of player with the highest piece (0-indexed) */
int starter(pairset * xs){
    int i;
    for(i = 0; i < PLAYERS; ++i){
        if ((xs+i)->count(intpair (N-1,N-1))){
            return i;
        }
    }

    return -1; // will never happen unless something very wrong
}

int points(pairset xs){
    int ret = 0;
    pairset :: iterator it;
    for(it = xs.begin(); it != xs.end(); ++it){
        ret += it->first;
        ret += it->second;
    }
    return ret;
}

/* returns the new open values of the game */
/* RUNS INSIDE CRITICAL REGION (while using the table) */
intpair update(intpair x){
    /* If the same piece can be played both ways it will go to the
       first available place; FIXME */

    intpair ret = edges;

    if(edges.first == x.first || edges.second == x.first){
        if(edges.first == x.first){
            ret.first = x.second;
        } else {
            ret.second = x.second;
        }
    }
    if(edges.first == x.second || edges.second == x.second){
        if(edges.first == x.second){
            ret.first = x.first;
        } else {
            ret.second = x.first;
        }
    }

    return ret;
}

/* evaluates if, for both open values, every playable piece is already
   on the table (maybe slow) */
int islocked(){
    int izq,der;
    izq = N;
    der = N;

    for(pairvector :: iterator it = table.begin(); it != table.end(); ++it){
        if(it->first == edges.first || it->second == edges.first){ --izq; }
        if(it->first == edges.second || it->second == edges.second){ --der; }
    }

    return (!(izq > 0 || der > 0));
}

/* threads' running funcion */
void * player (void * arg){
    int offset = ((param *) arg) -> x;
    pairset mano = *(((param *) arg) -> xs);

    // receives a "choosing" function
    // to allow a different AI on each thread
    intpair (*choose)(pairset) = ((param *) arg) -> fnc;

    struct timespec espera = {0};
    espera.tv_nsec= (random() % 1000000000);
    //espera.tv_nsec = 324545633;

    while(1){

        sem_wait(mutexes + offset);

        //if(!(random()%6)){sleep(1);}

        /* CRITICAL REGION: this player is using the table */

        pthread_mutex_lock(&jugando);
        if (fin || islocked()) {
            cout << "[" << edges.first << " " << edges.second << "]" << endl;
            score[offset] = points(mano);
            pthread_mutex_unlock(&jugando);
            sem_post(mutexes + next(offset));
            break;
        }
        //nanosleep(&espera,NULL);
        cout << "player" << offset;

        cout << " [" << edges.first << " " << edges.second << "] ";
        intpair t = choose(mano); // this player's AI function
        if( edges.first == t.first ||
                edges.second == t.second ||
                edges.first == t.second ||
                edges.second == t.first) {

            // the current player puts a piece on the table here
            mano.erase(t);
            table.push_back(t);
            edges = update(t);

            cout << " " << " (" << t.first << "," << t.second << ")\n";
        } else {
            cout << " pass. " << endl;
        }

        /* You have just played, and you won */
        if(mano.empty()){
            fin = 1;
            pthread_mutex_unlock(&jugando);
            sem_post(mutexes + next(offset));
            break;
        }

        pthread_mutex_unlock(&jugando);

        /* END CRITICAL REGION */

        /* Tell the player to your right to go on */
        sem_post(mutexes + next(offset));
    }

    return NULL;
}

/* RUNS INSIDE CRITICAL REGION (while using the table) */
/* say which piece do you want to play; set it at 'ret' */
intpair choosesmaller(pairset xs){

    intpair ret;

    /* You have the highest piece; play first ̣*/
    if(xs.count(intpair (N-1,N-1))){ return intpair (N-1,N-1); }

    pairset candidates;
    pairset :: iterator it;

    /* Put on a set only the pieces that you own and that can be played */
    for(it = xs.begin(); it != xs.end(); ++it){
        if(it->first == edges.first ||
                it->first == edges.second ||
                it->second == edges.first ||
                it->second == edges.second){
            candidates.insert(*it);
        }
    }

    /* Nothing can be played, anything you return will turn into "pass" */
    if(candidates.empty()){
        return intpair (-1,-1);
    }

    /////////////////// ADD OWN ALGORITHM HERE ///////////////////////

    ret = *(candidates.lower_bound(intpair (0,0)));

    //////////////////////////// END ALGORITHM ///////////////////////

    /* do not try to wreck the edges by returning something you don't have */
    if(candidates.count(ret) > 0) {
        return ret;
    } else {
        return intpair (-1,-1);
    }
}
