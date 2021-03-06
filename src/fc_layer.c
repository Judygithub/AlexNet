//
// File:        fc_layer.c
// Description: Implementation of full connected layer
// Author:      Haris Wang
//
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
#include "fc_layer.h"
#include "matrix.h"
#define MIN(a,b) (((a) < (b)) ? (a) : (b))


typedef struct fc_args{
    fc_op *op;
    short batch_id;
    short st_tunits;
    short ed_tunits;
} fc_args;

static void* pthread_fc_op_forward(void *argv)
{
    /**
     * pthread fc_op_forward
     * */
    fc_args args;
    memcpy(&args, (fc_args *)argv, sizeof(fc_args));
    short internal = args.ed_tunits-args.st_tunits;
    float *weights = (float *)malloc(internal * (args.op->in_units) * sizeof(float));
    for(int j=0; j<args.op->in_units; j++)
    {
        register int w_offset = j*internal;
        register int ow_offset = j*(args.op->out_units)+args.st_tunits;
        for(int i=0; i<internal; i++)
            weights[w_offset++] = args.op->weights[ow_offset++];
    }

    float *output = (float *)calloc(internal * (args.op->batchsize), sizeof(float));
    matrix_multiply( args.op->input, weights, output,  args.op->batchsize,  args.op->in_units, internal);
    for(int j=0; j<args.op->batchsize; j++)
    {
        register int o_offset = j*internal;
        register int oo_offset = j*(args.op->out_units)+args.st_tunits;
        for(int i=0; i<internal; i++, o_offset++, oo_offset++)
            args.op->output[oo_offset] = output[o_offset] + args.op->bias[args.st_tunits+i]; 
    }
    free(output);
    free(weights);
}

void fc_op_forward(fc_op *op)
{
    short tnum = 12;
    if(op->out_units < tnum)
    {
        fc_args args;
        args.op = op;
        args.st_tunits = 0;
        args.ed_tunits = op->out_units;
        pthread_fc_op_forward((void *)(&args));
    }else{
        fc_args args[tnum+1];
        pthread_t tid[tnum+1];
        short internal = ceil(1.0 * op->out_units / tnum);

        for(int p=0; p<tnum; p++)
        {
            args[p].op = op;
            args[p].st_tunits = p*internal;
            args[p].ed_tunits = MIN(args[p].st_tunits+internal, op->out_units);            
            pthread_create(&tid[p], NULL, pthread_fc_op_forward, (void *)(&args[p]));
        }

        for(int p=0; p<tnum; p++)
        {
            pthread_join(tid[p], NULL);
        }
    }

}


static void* pthread_fc_op_backward(void *argv)
{
    /**
     * pthread fc_op_backward
     * */
    fc_args args;
    memcpy(&args, (fc_args *)argv, sizeof(fc_args));

    register int w_offset=0;
    if(args.st_tunits==0)
    {
        for (register int j=0; j< args.op->out_units; j++)
        {
            register float d_o = args.op->d_output[j];
            for (register int i=0; i< args.op->in_units; i++, w_offset++)
            {
                args.op->d_input[i] += args.op->weights[w_offset] * d_o;
            }
            args.op->d_bias[j] = d_o;
        }
    }
 

    register float *input = args.op->input;
    register float *out_error = args.op->d_output;
    register float *w_deltas = args.op->d_weights;

    for(int i=args.st_tunits; i<args.ed_tunits; i++)
    {
        register float oe =out_error[i];
        if(oe<0.0008)
            continue;
        register int input_offset=0;
        for(int p=0; p<args.op->batchsize; p++)
        {
            w_offset = i*(args.op->in_units);
            register int w_o_b = w_offset + args.op->in_units;
            while(w_offset < w_o_b)
            {
                w_deltas[w_offset++] += oe * input[input_offset++] / args.op->batchsize;
            }
        }
    }
}

void fc_op_backward(fc_op *op)
{
    short tnum = 12;
    if(op->out_units < tnum)
    {
        fc_args args;
        args.op = op;
        args.st_tunits = 0;
        args.ed_tunits = op->out_units;
        pthread_fc_op_backward((void *)(&args));
    }else{
        fc_args args[tnum+1];
        pthread_t tid[tnum+1];
        short internal = ceil(1.0 * op->out_units / tnum);

        for(int p=0; p<tnum; p++)
        {
            args[p].op = op;
            args[p].st_tunits = p*internal;
            args[p].ed_tunits = MIN(args[p].st_tunits+internal, op->out_units);            
            pthread_create(&tid[p], NULL, pthread_fc_op_backward, (void *)(&args[p]));
        }

        for(int p=0; p<tnum; p++)
        {
            pthread_join(tid[p], NULL);
        }
    }

}

