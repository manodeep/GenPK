/* Copyright (c) 2009,2010 Simeon Bird <spb41@cam.ac.uk>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include "gen-pk.h"
#include <math.h>
//For getopt
#include <unistd.h>
//For omp_get_num_procs
#include <omp.h>
#include <stdlib.h>

/* In practice this means we need just over 4GB, as sizeof(float)=4*/
#define FIELD_DIMS 1024

using namespace GadgetReader;
using namespace std;

int main(int argc, char* argv[]){
  int nrbins,field_dims=0,type;
  float *field, *power, *keffs;
  int *count; 
  string infiles(""),outdir("");
  char c;
  double box;
  fftwf_plan pl;
  fftwf_complex *outfield;
  while((c = getopt(argc, argv, "i:o:h")) !=-1){
    switch(c){
        case 'o':
           outdir=static_cast<string>(optarg);
           break;
        case 'i':
           infiles=static_cast<string>(optarg);
           break;
        case 'h':
        default:
           help();
           return 0;
      }
  }
  //Open the snapshot
  GSnap snap(infiles);
  if(outdir.empty() || snap.GetNumFiles() < 1){
          help();
          return 0;
  }
  //Work out how large a field we need
  for(type=0;type<N_TYPE;type++){
    int tmp=2*nexttwo(cbrt(snap.GetNpart(type)));
    field_dims=std::max(field_dims, std::min(tmp, FIELD_DIMS));
  }
  //Get the header and print out some useful things
  box=snap.GetHeader().BoxSize;
  nrbins=floor(sqrt(3)*((field_dims+1.0)/2.0)+1);
  fprintf(stderr, "Boxsize=%g, ",box);
  fprintf(stderr, "redshift=%g, Ω_M=%g\n",snap.GetHeader().redshift,snap.GetHeader().Omega0);
  fprintf(stderr, "NPart=(%g,%g,%g,%g,%g,%g)**3\n",cbrt(snap.GetNpart(0)),cbrt(snap.GetNpart(1)),cbrt(snap.GetNpart(2)),cbrt(snap.GetNpart(3)),cbrt(snap.GetNpart(4)),cbrt(snap.GetNpart(5)));
  fprintf(stderr, "Masses=[%g %g]\n",snap.GetHeader().mass[0],snap.GetHeader().mass[1]);
  //Memory for the field
  /* Allocating a bit more memory allows us to do in-place transforms.*/
  if(!(field=(float *)fftwf_malloc(2*field_dims*field_dims*(field_dims/2+1)*sizeof(float)))){
  	fprintf(stderr,"Error allocating memory for field\n");
  	return 1;
  }
  string filename=outdir;
  size_t last=infiles.find_last_of("/\\");
  //Set up FFTW
  outfield=(fftwf_complex *) &field[0];
  if(!fftwf_init_threads()){
  		  fprintf(stderr,"Error initialising fftw threads\n");
  		  return 0;
  }
  fftwf_plan_with_nthreads(omp_get_num_procs());
  pl=fftwf_plan_dft_r2c_3d(field_dims,field_dims,field_dims,&field[0],outfield, FFTW_ESTIMATE);
  //Allocate memory for output
  power=(float *) malloc(nrbins*sizeof(float));
  count=(int *) malloc(nrbins*sizeof(int));
  keffs=(float *) malloc(nrbins*sizeof(float));
  if(!power || !count || !keffs){
  	fprintf(stderr,"Error allocating memory for power spectrum.\n");
        return 1;
  }
  /*Now make a power spectrum for each particle type*/
  for(type=0; type<N_TYPE; type++){
        if(read_fieldize(field,&snap,type, box, field_dims))
                continue;
        if(powerspectrum(field_dims,&pl,outfield,nrbins, power,count,keffs))
                continue;
        filename=outdir;
        filename+="/PK-"+type_str(type)+"-"+infiles.substr(last+1);
        print_pk(filename,nrbins,keffs,power,count);
  }
  //Free memory
  free(power);
  free(count);
  free(keffs);
  fftwf_free(field);
  fftwf_destroy_plan(pl);
  return 0;
}

