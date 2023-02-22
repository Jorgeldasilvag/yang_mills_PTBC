#ifndef YM_LOCAL_PT_C
#define YM_LOCAL_PT_C

#include"../include/macro.h"

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>

#ifdef OPENMP_MODE
  #include<omp.h>
#endif

#include"../include/function_pointers.h"
#include"../include/gauge_conf.h"
#include"../include/geometry.h"
#include"../include/gparam.h"
#include"../include/random.h"

void real_main(char *in_file)
{
	Gauge_Conf *GC;
	Geometry geo;
	GParam param;
	Rectangle swap_rectangle;
	Rectangle *most_update, *clover_rectangle;
	Acc_Utils acc_counters;
	int L_R_swap=1;

	//For some checks in the action and twist
	//long r;
	//int ii,jj;
	//int rep;
	//double re_tr_plaq_a_i, Ka_i;
	//double plaqs, plaqt, pla;
	//double Sa_i=0.0;

	char name[STD_STRING_LENGTH], aux[STD_STRING_LENGTH];
	int count;
	FILE *datafilep, *chiprimefilep, *swaptrackfilep, *topchar_tcorr_filep, *datafilep_top0, *datafilep_top1, *datafilep_top2, *datafilep_top3;
	time_t time1, time2;


	// to disable nested parallelism
	#ifdef OPENMP_MODE
	// omp_set_nested(0); // deprecated
	omp_set_max_active_levels(1); // should do the same as the old omp_set_nested(0)
	#endif

	// read input file
	readinput(in_file, &param);

	// initialize random generator
	initrand(param.d_randseed);

	// open data_file
	init_data_file(&datafilep, &datafilep_top0, &datafilep_top1, &datafilep_top2, &datafilep_top3, &chiprimefilep, &topchar_tcorr_filep, &param);

	// open swap tracking file
	init_swap_track_file(&swaptrackfilep, &param);

	// initialize geometry
	init_indexing_lexeo();
	init_geometry(&geo, &param);

	// initialize gauge configurations replica and volume defects
	init_gauge_conf_replica(&GC, &param);

	// initialize rectangles for hierarchical update
	init_rect_hierarc(&most_update, &clover_rectangle, &param);

	// initialize rectangle for swap probability evaluation (L_R_swap = 1)
	init_rect(&swap_rectangle, L_R_swap, &param);

	// init acceptances array
	init_swap_acc_arrays(&acc_counters, &param);

	// Checking the twist is correctly initialized in all replicas 
	//for(rep=0; rep<(param.d_N_replica_pt); rep++)
	//{
	//	for(r=0; r<(param.d_volume); r++)
	//	{
	//		for(ii=0; ii<STDIM; ii++)
	//		{
	//			for(jj=ii+1; jj<STDIM; jj++)
	//			{
	//				printf("%d %ld %.12g %.12g\n",planeid(ii,jj),r,creal(GC[rep].ztw[r][planeid(ii,jj)]),cimag(GC[rep].ztw[r][planeid(ii,jj)]));
	//			}
	//		}
	//	}
	//}

	// Checking the initial action is correct with the twisted background
	//Sa_i = 0.0;
	//for(r=0; r<(param.d_volume); r++)
	//{
	//	for(ii=0; ii<STDIM; ii++)
	//	{
	//		for(jj=ii+1; jj<STDIM; jj++)
	//		{
	//			re_tr_plaq_a_i=plaquettep(&(GC[0]), &geo, &param, r, ii, jj);
	//			Ka_i=(GC[0].C[r][ii])*(GC[0].C[nnp(&geo, r, ii)][jj])*(GC[0].C[nnp(&geo, r, jj)][ii])*(GC[0].C[r][jj]);
	//			Sa_i+=(1.0-Ka_i*re_tr_plaq_a_i);
	//		}
	//	}
	//}
	//Sa_i*=param.d_beta/(double)NCOLOR;
	//printf("%.12g\n",Sa_i);

	//plaquette(&(GC[0]), &geo, &param, &plaqs, &plaqt);
	//pla = 0.5*(plaqs+plaqt);
	//printf("%.12g\n",pla);

	// Monte Carlo begin
	time(&time1);

	for(count=0; count < param.d_sample; count++)
	{
		// perform a single step of parallel tempering wth hierarchical update and print state of replica swaps
		parallel_tempering_with_hierarchical_update(GC, &geo, &param, most_update, clover_rectangle, &swap_rectangle, &acc_counters);
		print_conf_labels(swaptrackfilep, GC, &param);

		// perform measures only on homogeneous configuration
		if(GC[0].update_index % param.d_measevery == 0 && GC[0].update_index >= param.d_thermal)
		{
			perform_measures_localobs_GF(&(GC[0]), &geo, &param, datafilep, chiprimefilep, topchar_tcorr_filep, datafilep_top0, datafilep_top1, datafilep_top2, datafilep_top3);
		}

		// save configurations for backup
		if(param.d_saveconf_back_every!=0)
		{
			if(GC[0].update_index % param.d_saveconf_back_every == 0 )
			{
				// simple
				write_replica_on_file(GC, &param);
				// backup copy
				write_replica_on_file_back(GC, &param);
			}
		}

		// save homogeneous configuration for offline analysis
		if(param.d_saveconf_analysis_every!=0)
		{
			if(GC[0].update_index % param.d_saveconf_analysis_every == 0 )
			{
				strcpy(name, param.d_conf_file);
				strcat(name, "_step_");
				sprintf(aux, "%ld", GC[0].update_index);
				strcat(name, aux);
				write_conf_on_file_with_name(&(GC[0]), &param, name);
			}
		}
	}

	time(&time2);
	// Monte Carlo end

	// close data file
	fclose(datafilep);
	fclose(datafilep_top0);
	fclose(datafilep_top1);
	fclose(datafilep_top2);
	fclose(datafilep_top3);
	if (param.d_chi_prime_meas==1) fclose(chiprimefilep);
	if (param.d_topcharge_tcorr_meas==1) fclose(topchar_tcorr_filep);

	// close swap tracking file
	if (param.d_N_replica_pt > 1) fclose(swaptrackfilep);

	// save configurations
	if (param.d_saveconf_back_every!=0)
	{
	write_replica_on_file(GC, &param);
	}

	// print simulation details
	print_parameters_local_pt(&param, time1, time2);

	// print acceptances of parallel tempering
	print_acceptances(&acc_counters, &param);

	// free gauge configurations
	free_replica(GC, &param);

	// free geometry
	free_geometry(&geo, &param);

	// free rectangles for hierarchical update
	free_rect_hierarc(most_update, clover_rectangle, &param);

	// free rectangle for swap probability evaluation
	free_rect(&swap_rectangle);

	// free acceptances array
	end_swap_acc_arrays(&acc_counters, &param);

	// free hierarchical update parameters
	free_hierarc_params(&param);
}


void print_template_input(void)
{
	FILE *fp;

	fp=fopen("template_input.example", "w");

	if(fp==NULL)
	 {
		fprintf(stderr, "Error in opening the file template_input.example (%s, %d)\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	 }
	else
	{
		fprintf(fp,"size 12 4 4 12  # Nt Nx Ny Nz\n");
		fprintf(fp,"\n");
		fprintf(fp,"# twist parameters\n");
		fprintf(fp,"k_twist 0 0 0 1 0 0 # ktwist on the plane 01,02,03,12,13,23");
		fprintf(fp,"\n");
		fprintf(fp,"# parallel tempering parameters\n");
		fprintf(fp,"defect_dir    0             # choose direction of defect boundary: 0->t, 1->x, 2->y, 3->z\n");
		fprintf(fp,"defect_size   2 2 2         # size of the defect (order: y-size z-size t-size)\n");
		fprintf(fp,"N_replica_pt  10    1.0 0.876 0.76 0.652 0.56 0.468 0.369 0.261 0.143 0.0  # number of parallel tempering replica ____ boundary conditions coefficients\n");
		fprintf(fp,"\n");
		fprintf(fp,"# hierarchical update parameters\n");
		fprintf(fp,"# Order: num of hierarc levels ____ extension of rectangles ____ num of sweeps per rectangle\n");
		fprintf(fp,"hierarc_upd 2    2 1    3 4\n");
		fprintf(fp,"\n");
		fprintf(fp,"# Simulations parameters\n");
		fprintf(fp, "beta  6.600\n");
		fprintf(fp, "theta 0.0\n");
		fprintf(fp,"\n");
		fprintf(fp, "sample     1\n");
		fprintf(fp, "thermal    0\n");
		fprintf(fp, "overrelax  5\n");
		fprintf(fp, "measevery  1\n");
		fprintf(fp,"\n");
		fprintf(fp, "start                    3  # 0=ordered  1=random  2=from saved configuration 3=cold twisted(only for 12 dir)\n");
		fprintf(fp, "saveconf_back_every      5  # if 0 does not save, else save backup configurations every ... updates\n");
		fprintf(fp, "saveconf_analysis_every  5  # if 0 does not save, else save configurations for analysis every ... updates\n");
		fprintf(fp, "\n");
		fprintf(fp, "coolsteps             3  # number of cooling steps to be used\n");
		fprintf(fp, "coolrepeat            5  # number of times 'coolsteps' are repeated\n");
		fprintf(fp, "chi_prime_meas        0  # 1=YES, 0=NO\n");
		fprintf(fp, "topcharge_tcorr_meas  0  # 1=YES, 0=NO\n");
		fprintf(fp,"\n");
		fprintf(fp, "#for gradient flow evolution\n");
		fprintf(fp, "gfstep      0.01    # integration step for gradient flow\n");
		fprintf(fp, "num_gfsteps 100     # number of integration steps for gradient flow\n");
		fprintf(fp, "gf_meas_each 100       # compute observables every <gf_meas_each> integration steps during the gradient flow\n");
		fprintf(fp,"\n");
		fprintf(fp, "# output files\n");
		fprintf(fp, "conf_file             conf.dat\n");
		fprintf(fp, "data_file             dati.dat\n");
		fprintf(fp, "chiprime_data_file    chiprime_cool.dat\n");
		fprintf(fp, "topcharge_tcorr_file  topo_tcorr_cool.dat\n");
		fprintf(fp, "log_file              log.dat\n");
		fprintf(fp, "swap_acc_file         swap_acc.dat\n");
		fprintf(fp, "swap_track_file       swap_track.dat\n");
		fprintf(fp, "\n");
		fprintf(fp, "randseed 0    # (0=time)\n");
		fclose(fp);
	 }
}

int main (int argc, char **argv)
{
	char in_file[STD_STRING_LENGTH];

	if(argc != 2)
	{
		printf("\nSU(N) Hasenbusch Parallel Tempering implemented by Claudio Bonanno (claudiobonanno93@gmail.com) within yang-mills package\n");
		printf("Usage: %s input_file\n\n", argv[0]);

		printf("\nDetails about yang-mills package:\n");
		printf("\tPackage %s version: %s\n", PACKAGE_NAME, PACKAGE_VERSION);
		printf("\tAuthor: Claudio Bonati %s\n\n", PACKAGE_BUGREPORT);

		printf("Compilation details:\n");
		printf("\tN_c (number of colors): %d\n", NCOLOR);
		printf("\tST_dim (space-time dimensionality): %d\n", STDIM);
		printf("\tNum_levels (number of levels): %d\n", NLEVELS);
		printf("\n");
		printf("\tINT_ALIGN: %s\n", QUOTEME(INT_ALIGN));
		printf("\tDOUBLE_ALIGN: %s\n", QUOTEME(DOUBLE_ALIGN));

		#ifdef DEBUG
			printf("\n\tDEBUG mode\n");
		#endif

		#ifdef OPENMP_MODE
			printf("\n\tusing OpenMP with %d threads\n", NTHREADS);
		#endif

		#ifdef THETA_MODE
			printf("\n\tusing imaginary theta\n");
		#endif

		printf("\n");

		#ifdef __INTEL_COMPILER
			printf("\tcompiled with icc\n");
		#elif defined(__clang__)
			printf("\tcompiled with clang\n");
		#elif defined( __GNUC__ )
			printf("\tcompiled with gcc version: %d.%d.%d\n",
				__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
		#endif

		print_template_input();

		return EXIT_SUCCESS;
	}
	 else
	{
		if(strlen(argv[1]) >= STD_STRING_LENGTH)
		{
		fprintf(stderr, "File name too long. Increse STD_STRING_LENGTH in /include/macro.h\n");
		return EXIT_SUCCESS;
		  }
		else
		{
			#if(STDIM==4 && NCOLOR>1)
				strcpy(in_file, argv[1]);
				real_main(in_file);
				return EXIT_SUCCESS;
			#else
				fprintf(stderr, "Parallel tempering of volume defect not implemented for STDIM =/= 4 and N_color < 2.\n");
				return EXIT_SUCCESS;
			#endif
		}
	}
}

#endif
