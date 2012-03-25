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

#define N 15
#define PLAYERS 8

//#define next(x) ((x+PLAYERS-1)%PLAYERS)
#define next(x) ((x+1)%PLAYERS)

typedef pair <int,int> intpair;
typedef set <intpair> pairset;
typedef vector <intpair> pairvector;

void * worker (void *);
pairset * fichas();
intpair actualiza(intpair);
int inicial(pairset *);
int islocked();
int points(pairset);

intpair escogefifo(pairset);

/* Syncronization */

sem_t * mutexes;
pthread_mutex_t jugando;

/* Shared resources */

int fin = 0;
int score[PLAYERS] = {0};

pairvector mesa;
intpair punta = intpair (N-1,N-1); // inicial, para pegar el (6,6) verdadero del primer jugador

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

    players = fichas();

    for(i = 0; i < PLAYERS; ++i){
        (p+i)->x = i;
        (p+i)->xs = players+i;
        (p+i)->fnc = escogefifo;
        pthread_create(&threads[i], NULL, worker, (void *) (p+i));
    }

    for(i = 0; i < PLAYERS; ++i){
        cout << "\n--- PLAYER " << i << " ---\n";
        for(it = (players + i)->begin(); it != (players + i)->end(); ++it){
            cout << " (" << it->first << ", " << it->second << ") ";
        }
    }
    cout << "\n";

    sem_post(mutexes+inicial(players)); // desbloquea al primer jugador; buscar al (6,6)
    sem_post(mutexes+(inicial(players)+PLAYERS/2)%PLAYERS); // creando meta-hilos
    for(i = 0; i < PLAYERS; ++i){
        pthread_join(threads[i], NULL);
    }

    for(i = 0; i < PLAYERS; ++i){
        cout << "player " << i << ": -" << score[i] << endl;
    }

    delete[] players;

    delete[] p;
    delete[] threads;
    delete[] mutexes;
    return 0;
}

pairset * fichas() {
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
    srandom(seed); // para 'random'
    srand(seed); // para 'shuffle'
    random_shuffle(ys.begin(), ys.end());

    for(it = ys.begin(), i = 0; it != ys.end(); ++it, ++i){
        (players + (i%PLAYERS))->insert(*it);
    }

    return players;

    return 0;
}

int inicial(pairset * xs){
    int i;
    for(i = 0; i < PLAYERS; ++i){
        if ((xs+i)->count(intpair (N-1,N-1))){
            return i;
        }
    }

    return -1; // no debe suceder
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

// requiere exclusión mutua
intpair actualiza(intpair x){
    // en orden; en el futuro, definir de qué lado jugar doble opción
    // FIXME
    intpair ret = punta;

    if(punta.first == x.first || punta.second == x.first){
        if(punta.first == x.first){
            ret.first = x.second;
        } else {
            ret.second = x.second;
        }
    }
    if(punta.first == x.second || punta.second == x.second){
        if(punta.first == x.second){
            ret.first = x.first;
        } else {
            ret.second = x.first;
        }
    }

    return ret;
}

int islocked(){
    // pregunta si, para ambas puntas, todas las posibles piezas están en la mesa
    // posiblemente lento, analizar
    int izq,der;
    izq = N;
    der = N;

    for(intvector :: iterator it = mesa.begin(); it != mesa.end(); ++it){
        if(it->first == punta.first || it->second == punta.first){ --izq; }
        if(it->first == punta.second || it->second == punta.second){ --der; }
    }

    return (!(izq > 0 || der > 0));
}

intpair escogefifo(pairset xs){

    if(xs.count(intpair (N-1,N-1))){return intpair (N-1,N-1);}

    pairset candidatos;
    pairset :: iterator it;

    for(it = xs.begin(); it != xs.end(); ++it){
        if(it->first == punta.first ||
                it->first == punta.second ||
                it->second == punta.first ||
                it->second == punta.second){
            candidatos.insert(*it);
        }
    }

    if(candidatos.empty()){
        return intpair (-1,-1);
    }

    /////////////////// EDITAR ALGORITMO AQUÍ ///////////////////////

    return *(candidatos.lower_bound(intpair (0,0)));
}

