#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include "helper.h"

#define NORTH 0x01
#define EAST 0x02
#define SOUTH 0x04
#define WEST 0x08


typedef struct _check_bound_params {
    int pos;
    int *res;
    world w;
} check_bound_params;

typedef struct _evolve_param {
    int start_row;
    int end_row;
    world *w;
    world *nw;
} evolve_param_t;

typedef struct _populate_world_th_params_t {
    world *w;
    world *nw;
    int r;
    int c;
    int start_row;
    int end_row;
} populate_world_th_params_t;

void * check_boundary(void *cbp);

void * process_world_slice(void* args);

void seed(world w){
    int i, j;
    /*  for(i=0; i<50; i++){
        world[rand()%w.size][rand()%w.size] = alive;
        }
        */

    create_glider(w, 2, 5);
    create_block(w, 1, 1);
//    create_pulsar(w, 2, 2);
}

int get_neigh(world *w, int x, int y){
    int n=0, i, j, a, b;
    for(i=-1; i<=1; i++){
        for(j=-1; j<=1; j++){
            if(i==0 && j==0) {
                continue;
            }
            a = x+i;
            b = y+j;
            if(a >=0 && a < w->size.rows && b >=0 && b < w->size.cols){
                if(w->space[a][b] == alive){
                    n++;
                }
            }
        }
    }
    return n;
}

void free_world(world w){
    int i;
    for(i=0; i<w.size.rows;i++){
        free(w.space[i]);
    }
    free(w.space);
}

void * populate_world_slice(void *args){
    int i, j;
    populate_world_th_params_t *params = (populate_world_th_params_t *) args;
    for(i=params->start_row; i<params->end_row; i++){
        for(j=0; j<params->w->size.cols; j++){
            params->nw->space[params->r+i][params->c+j] = params->w->space[i][j];
        }
    }
}

void populate_world(world w, world neww, int r, int c, int num_threads){
    int i, j;
    if(num_threads == 1){
        for(i=0; i<w.size.rows; i++){
            for(j=0; j<w.size.cols;j++){
                neww.space[r+i][c+j] = w.space[i][j];
            }
        }
        return;
    }
    populate_world_th_params_t *pwtparams = (populate_world_th_params_t *) malloc(num_threads * sizeof(populate_world_th_params_t));
    pthread_t *pw_th = (pthread_t *)malloc(num_threads * sizeof(pthread_t));

    // printf("Populating new world\n");
    for(i=0; i<num_threads; i++){
        pwtparams[i].start_row = i * (w.size.rows / num_threads);
        if(i==num_threads-1) {
            pwtparams[i].end_row = w.size.rows;
        }
        else {
            pwtparams[i].end_row = (i+1) * (w.size.rows / num_threads);
        }
        pwtparams[i].w = &w;
        pwtparams[i].nw = &neww;
        pwtparams[i].r = r;
        pwtparams[i].c = c;

        pthread_create(&pw_th[i], NULL, &populate_world_slice, (void *)(pwtparams+i));
    }

    // wait for all threads; synchronization
    for(i=0;i<num_threads;i++){
        pthread_join(pw_th[i], NULL);
    }

    free(pwtparams);
    free(pw_th);
}

void * check_boundary(void *cbparams) {
    check_bound_params *cbp = ( check_bound_params *) cbparams;
    world w = cbp->w;
    int *res = cbp->res;
    int i;
    switch(cbp->pos){
        case 0:
            for(i=0; i<w.size.cols; i++){
                if((w.space[0][i]) == alive){
                    *res |= NORTH;
                    break;
                }
            }
            break;
        case 1:
            for(i=0; i<w.size.rows; i++){
                if((w.space[i][0]) == alive){
                    *res |= WEST;
                    break;
                }
            }
            break;
        case 2:
            for(i=0; i<w.size.cols; i++){
                if((w.space[w.size.rows-1][i]) == alive){
                    *res |= SOUTH;
                    break;
                }
            }
            break;
        case 3:
            for(i=0; i<w.size.rows; i++){
                if((w.space[i][w.size.cols-1]) == alive){
                    *res |= EAST;
                    break;
                }
            }
            break;
    }
}

// NOTE: mutex and lock
void * process_world_slice(void* args){
    int i, j, n;
    evolve_param_t *evp = (evolve_param_t *) args;
    for(i = evp->start_row; i < evp->end_row; i++){
        for(j=0; j < evp->w->size.cols; j++){
            n = get_neigh(evp->w, i, j);
            if(evp->w->space[i][j]==alive){
                if(n<2 || n>3){
                    evp->nw->space[i][j] = dead;
                }
                else {
                    evp->nw->space[i][j] = alive;
                }
            }
            else {
                if (n==3){
                   evp->nw->space[i][j] = alive;
                }
                else {
                    evp->nw->space[i][j] = dead;
                }
            }
        }
    }
}

world evolve(world w, int num_threads) {
    int i, j, n, nedge=0;
    pthread_t bound_th[4];
    pthread_t *evolve_th = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    evolve_param_t *evp = (evolve_param_t *)malloc(num_threads * sizeof(evolve_param_t));

    world newworld;
    world nextgen = create_world(w.size);
    // threading and shared data
    num_threads = num_threads < w.size.rows ? num_threads : w.size.rows;
    for(i=0; i<num_threads; i++){
        evp[i].start_row = i * (w.size.rows / num_threads);
        if(i==num_threads-1) {
            evp[i].end_row = w.size.rows;
        }
        else {
            evp[i].end_row = (i+1) * (w.size.rows / num_threads);
        }
        evp[i].w = &w;
        evp[i].nw = &nextgen;
        pthread_create(&evolve_th[i], NULL, &process_world_slice, (void *)(evp+i));
    }

    // wait for all threads; synchronization
    for(i=0;i<num_threads;i++){
        pthread_join(evolve_th[i], NULL);
    }

    free(evolve_th);
    free(evp);

    check_bound_params cbp;
    cbp.res = &nedge;
    cbp.w = nextgen;
    // 4 threads
    for(i=0; i<4;i++){
        cbp.pos = i;
        pthread_create(&bound_th[i], NULL, &check_boundary, (void *)&cbp);
    }
    for(i=0;i<4;i++){
        pthread_join(bound_th[i], NULL);
    }

    free_world(w);
    newworld = nextgen;
    if(nedge != 0){
        // printf("Expanding world... %x\n", nedge);
        int pos_row = 0;
        int pos_col = 0;
        world_size_t new_world_size;
        new_world_size.rows = nextgen.size.rows;
        new_world_size.cols = nextgen.size.cols;

        if(nedge & NORTH){
            new_world_size.rows += nextgen.size.rows;
            pos_row += nextgen.size.rows;
        }
        if(nedge & SOUTH){
            new_world_size.rows += nextgen.size.rows;
        }

        if(nedge & EAST){
            new_world_size.cols += nextgen.size.cols;
        }
        if(nedge & WEST){
            new_world_size.cols += nextgen.size.cols;
            pos_col += nextgen.size.cols;
        }

        newworld = create_world(new_world_size);
        // TODO: multi-threaded
        populate_world(nextgen, newworld, pos_row, pos_col, num_threads);
        free_world(nextgen);
    }
    return newworld;
}


int main (int argc, char * argv[]){
    int i, num_threads = 2, in_file = 0;
    world w;
    if(argc >= 2){
        num_threads = atoi(argv[1]);
    }
    if(argc == 3){
        in_file = 1;
    }
    srand(time(NULL));
    if(in_file == 0){
    world_size_t world_size;
    world_size.rows = 20;
    world_size.cols = 10;

    w = create_world(world_size);
    seed(w);
    }
    else {
        w = parse_world(argv[2]);
    }
    //    print(w);
    struct timeval start_time, end_time, diff_time;
    gettimeofday(&start_time, 0);
    for(i=0; i<1000; i++){
        w = evolve(w, num_threads);
        print(w);
        if(i == 60) {
            create_pulsar(w, 5, 10);
        }
    }
    free_world(w);
    gettimeofday(&end_time, 0);
    if(timeval_subtract(&diff_time, &end_time, &start_time) != 1){
        printf("Time elapsed: %d us\n", diff_time.tv_sec * 1000000 + diff_time.tv_usec);
    }
    return 0;
}
